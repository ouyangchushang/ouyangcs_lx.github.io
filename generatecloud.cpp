#include <string>
#include <fstream>
#include <sstream>
#include <streambuf>
#include <map>
#include <cctype>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <algorithm>
#include <vector>
#include <getopt.h>
#include <cstdlib>
#include <ctime>
#include <opencv2/opencv.hpp>
using namespace std;


// First of all, a series of ignored words will be specified.
// These words with no specific meaning commonly used in English will be eliminated when we calculate the word frequency.
const char* IGNORES[] = { "a","an","am","and","the","you","me","my","us","be","it","us","he","she","him","her","his","hers",\
                        "ye","your","so","or","is","isnt","not","mr","mrs","ms","dr","sr","they","we","of","to","its",\
                        "i","on","oh","if","as","by","them","our","in","for","do","dont","does","doesnt","did","at","are","did",\
                        "im","youre","theyre","was","were","this","that","what","which","who","where","whom","when","how","and","but","as","or"\
                        "with","within","from","will","shall","would","should"};


////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////// Utility Functions //////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////
// PART1. GENERATE A WORD DICTIONARY FOR STORING FREQUENCY. ///
///////////////////////////////////////////////////////////////

string loadFile(string filename)
{
    ifstream t(filename.c_str());
    stringstream buffer;
    buffer << t.rdbuf();
    return buffer.str();
}

bool fileExist(string maybeFilename)
{
    ifstream infile(maybeFilename);
    return infile.good();
}

// To judge whether a word should be counted or not.
bool goodWord(string word)
{
    // No empty words (should never happen)
    if (word.empty() || word.length()==1) {return false;}

    // See if the word is in our explicit ignore list
    for(auto ignore : IGNORES)
    {
        if (word == ignore)
        {
            return false;
        }
    }
    // See if the word contains bad characters (numbers, punctuation, specials)
    if (word.find_first_not_of("abcdefghijklmnopqrstuvwxyz") != string::npos)
        return false;
    return true;

}

// In this function, we use map to build a dictionary, traverse the document and count every "good word" that appears. 
void buildWordMap(string words, map<string,float> &wordCloud)
{
   stringstream wordstream(words);
   string word;
   while (wordstream >> word)
   {
       // Make word lowercase, strip punctuation from it
        transform(word.begin(), word.end(), word.begin(), ::tolower);
        string str = "'s";
        std::size_t found = word.find(str);
        if (found != std::string::npos) {
            word.erase(found, str.size());
        }
        word.erase(remove_if(word.begin(), word.end(), ::ispunct), word.end());

        // Add word to our map if it passes all validity checks
        if (goodWord(word))
        {
            wordCloud[word] = wordCloud[word] + 1;  // Values initialized to 0 by default, can just start incrementing
        }
   }
}

bool compare(const std::pair<string, float> a, const std::pair<string, float> b)
{
    return a.second > b.second;
}

// This function is the final program to combine the fuctions above.
// Input: the txt file named "words.txt"
// Output: a sorted and normalized vector with element of pairs of a word and its frequency.
std::vector<pair<string,float>> getmap(string input)
{
    // Declare/Allocate variables
    map <string,float> wordCloud;         // Map/Dictionary to hold word=count mappings
    string contents;                    // Content to build word cloud for
    string startswith = "";             // Potential word starting letters for word cloud
    int option_index;                   // Optional argument variables
    int c;

    bool isvalid = fileExist(input);
    
    if (isvalid)
    {
        contents = loadFile(input);
    }
    else
    {
        contents = input;
    }

    // Build up our word dictionary! 
    buildWordMap(contents, wordCloud);

    std::vector<pair<string,float>> wordmap;
    for(auto elem: wordCloud)
    {
        wordmap.emplace_back(elem);
    }
    std::sort(wordmap.begin(), wordmap.end(),compare);
    float max_freq=wordmap.begin()->second;
    for(auto &elem:wordmap)
    {
        elem.second=elem.second/max_freq;
    }
    
    return wordmap;
}

/////////////////////////////////////////////////////////////////////////////
// PART2. GENERATE THE FINAL WORD CLOUD FROM THE DICTIONARY USING OPENCV. ///
/////////////////////////////////////////////////////////////////////////////

// Convert the font size expressed in pixels to the double font size for opencv.
// Choose the smaller one of weight and height as result.
double get_size(string text,int height,int width,int *baseline)
{
  cv::Size textsize = cv::getTextSize(text, cv::FONT_HERSHEY_SIMPLEX, 1.0, int(1.0/2+1), baseline);
  double extend=min(double(height)/double(textsize.height),double(width)/double(textsize.width));
  textsize = cv::getTextSize(text, cv::FONT_HERSHEY_SIMPLEX, extend, int(extend/2+1), baseline);
  return extend;
}

// USED IN THE NEXT FUNCTION TO FIND A LEGAL POSITION TO SET THE TEXTBOX.
// To judge if one point is qualified to be set as the lower left corner of the textbox.
bool qualified(int r, int c,cv::Size size,int baseline,cv::Mat canvas,cv::Mat mask,int upperbound,int lowerbound,int leftbound,int rightbound)
{
    // Make two judgments on the point in the box.
    // One is whether the point has text, and the other is whether the point is beyond the mask range.
    
    int margin=2;   // margin reserved for the box.
    
    // Get the edges of the textbox.
    int up=r-size.height-margin, down=r+baseline+margin, left=c-margin, right=c+size.width+margin;
    if(up<upperbound || down>lowerbound || left<leftbound || right>rightbound)
        return false;
    
    // Test if the box is legal.
    bool qualify=true;
    for(int i=left;i<=right && qualify;i++)
        for(int j=up;j<=down;j++)
            {
                if(mask.at<cv::Vec3b>(j,i)[0]+mask.at<cv::Vec3b>(j,i)[1]+mask.at<cv::Vec3b>(j,i)[2]>=3*255
                && canvas.at<cv::Vec3b>(j,i)[0]+canvas.at<cv::Vec3b>(j,i)[1]+canvas.at<cv::Vec3b>(j,i)[2]>=3*255)
                    continue;
                else
                    {
                        qualify=false;
                        break;
                    }
            }
    return qualify;
}

// To get a legal position to set as the lower left corner of the textbox.
// Use random sampling and testing.
bool get_position(cv::Mat canvas,cv::Mat mask,int upperbound,int lowerbound,int leftbound,int rightbound,cv::Size size,int baseline, cv::Point* position)
{
    int r=rand()%(lowerbound-upperbound)+upperbound;
    int c=rand()%(rightbound-leftbound)+leftbound;
    int iter=0;
    while(!qualified(r, c, size,baseline,canvas,mask,upperbound,lowerbound,leftbound,rightbound)&& iter<1000)
    {
        r=rand()%(lowerbound-upperbound)+upperbound;
        c=rand()%(rightbound-leftbound)+leftbound;
        iter++;
    }
    if(iter==1000)
        return false;
    else
    {   
        position->x=c;
        position->y=r;
    }
    return true;
}


// Calculate the max font size using the first two frequencies, originally with the size as big as possible.
// Try to fit the two words using the same process, and use their final sizes to get the real max font size.
double get_max_font_size(pair<string,float> pair1, pair<string,float> pair2,cv::Mat mask,int upperbound,int lowerbound,int leftbound, int rightbound)
{
    int baseline=0;
    int height=mask.rows,width=mask.cols;
    double font_step=0.1,min_font=0.3;
    cv::Mat canvas(height, width, CV_8UC3, cv::Scalar(255,255,255));
    double scale1=get_size(pair1.first,lowerbound-upperbound,rightbound-leftbound,&baseline);
    double font_size1=(0.5*pair1.second+0.5)*scale1;
    cv::Size textsize1=cv::getTextSize(pair1.first,cv::FONT_HERSHEY_SIMPLEX,font_size1,int(font_size1/2+1),&baseline);
    cv::Point pos;
    while(1)
    {
        if(get_position(canvas,mask,upperbound,lowerbound,leftbound,rightbound,textsize1,baseline,&pos))
            {
                putText(canvas,pair1.first,pos,cv::FONT_HERSHEY_SIMPLEX,font_size1,cv::Scalar(0, 0, 0), int(font_size1/2+1), cv::LINE_AA, false);
                break;
            }
         else if(font_size1< min_font)
            return 0;
        else
            { 
                
                font_size1-=font_step;
                textsize1=cv::getTextSize(pair1.first,cv::FONT_HERSHEY_SIMPLEX,font_size1,int(font_size1/2+1),&baseline);
            }
    }

    double scale2=get_size(pair2.first,height,width,&baseline);
    double font_size2=int(round(0.5*pair2.second+0.5)*scale2);
    cv::Size textsize2=cv::getTextSize(pair2.first,cv::FONT_HERSHEY_SIMPLEX,font_size2,int(font_size2/2+1),&baseline);
    while(1)
    {
        if(get_position(canvas,mask,upperbound,lowerbound,leftbound,rightbound,textsize2,baseline,&pos))
            {
                putText(canvas,pair2.first,pos,cv::FONT_HERSHEY_SIMPLEX,font_size2,cv::Scalar(0, 0, 0), int(font_size2/2+1), cv::LINE_AA, false);
                break;
            }
        else if(font_size2< min_font)
            return 0;
        else
            {
                font_size2-=font_step;
                textsize2=cv::getTextSize(pair2.first,cv::FONT_HERSHEY_SIMPLEX,font_size2,int(font_size2/2+1),&baseline);
            }
    }

    return 2*font_size1*font_size2/(font_size1+font_size2);
}

bool drawing=false;
int pt1_x=-1,pt1_y=-1;
cv::Mat mask;
void line_drawing(int event,int x,int y,int flags,void* param)
{
    if(event==cv::EVENT_LBUTTONDOWN)
    {
        drawing=true;
        pt1_x=x;
        pt1_y=y;
    }
    else if(event==cv::EVENT_MOUSEMOVE)
    {
        if(drawing)
        {
            cv::line(mask,cv::Point(pt1_x,pt1_y),cv::Point(x,y),cv::Scalar(255,255,255),40);
            pt1_x=x;
            pt1_y=y;
        }
    }
    else if(event==cv::EVENT_LBUTTONUP)
    {
        drawing=false;
        cv::line(mask,cv::Point(pt1_x,pt1_y),cv::Point(x,y),cv::Scalar(255,255,255),40);
    }
}
void getmask()
{
    printf("Draw the mask and press ENTER");
    mask=cv::Mat(512, 512, CV_8UC3, cv::Scalar(0,0,0));
    cv::namedWindow("Draw the mask and press ENTER");
    cv::setMouseCallback("Draw the mask and press ENTER",line_drawing);
    while(1)
    {
        cv::imshow("Draw the mask and press ENTER",mask);
        if (cv::waitKey(1) == 13)
            break;
    }
    cv::destroyAllWindows();
}
////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////// Runtime Execution //////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////

int main(int argc, char **argv)
{
    // Declare/Allocate program variables:

    // mask: the shape of the cloud, assigned by 'mask.jpg'.
    // last_freq: The frequency of the last word calculated.

    // About the font:
    // min_font: The minimum font size appearing in the word cloud, which is also the bottom line for the font size reduction compromise.
    // font_step: The step we use to shrink the font if no proper position are found foe the word.
    // max_font_size: The maximum font size appearing in the word cloud, determined by the function get_max_font_size().
    // font_size: Dynamic changing, the font size of each word element.

    // About the boundary and box:
    // upper/lower/left/rightbound: The upper/lower/left/right boundary of the determined word cloud, which is directly calculated by the mask.
    // baseline: Determined by the opencv function: getTextSize.

    int option_index;                   // Optional argument variables
    int c;
    string input="";
    string maskpath="";

    const struct option longopts[] =
    {
        {"help",      no_argument,        0, 'h'},
        {"input",     required_argument,  0, 'i'},
        {"mask",      required_argument,  0, 'm'},
        {0,0},
    };
    //Parse optional arguments
    while ((c = getopt_long(argc, argv, "i:mh:", longopts, &option_index)) != -1)
    {
        switch (c)
            {
            case 'm':
                maskpath=string(optarg);
                break;
            case 'i':
                input = string(optarg);
                break;
            case 'h':
                printf("Usage:\n\
                ./WordCloud (input) (--options)\n\
                \n\
                input:\n\
                    _/--input:  String or path to filename to load and generate word cloud for\n\
                \n\
                options:\n\
                    --mask:         The filename of a jpg/png image, whose white area determine the shape of the cloud\n\
                \n\
                Examples: \n\
                    # Generate unconstrained wordcloud from file 'test.txt'm \n\
                    ./WordCloud test.txt \n\
                    \n\
                    # Set 'mask.jpg' as the mask for word cloud from file 'test.txt' \n\
                    ./WordCloud test.txt mask.jpg \n\
                    ");
                    
                    abort();
            default:
                printf("\nOptional arguments could not be parsed! Call with --help for usage instructions\n");
                abort ();
            }
    }
    if(input=="")
        input="words.txt";
    if(maskpath=="")
        getmask();
    else
        mask=cv::imread(maskpath);

    // Get the dictionary in part 1.
    vector<pair<string,float>> wordmap=getmap(input);
    if(wordmap.size()<1)
    {
        cout<<"error! The text should at least contain one word.";
        exit(1);
    }

    // Read in the mask and build the canvas of the same size.
    // Definite the parameters.
    cv::Mat canvas(mask.rows, mask.cols, CV_8UC3, cv::Scalar(255,255,255));
    double min_font=0.3,font_step=0.1;
    int upperbound=1e7,lowerbound=-1,leftbound=1e7,rightbound=-1;
    srand(time(0));
    for(int r=0;r<mask.rows;r++)
        for(int c=0;c<mask.cols;c++)
        {
            if(mask.at<cv::Vec3b>(r,c)[0]+mask.at<cv::Vec3b>(r,c)[1]+mask.at<cv::Vec3b>(r,c)[2]>100)
                {
                    upperbound=min(upperbound,r);
                    lowerbound=max(lowerbound,r);
                    leftbound=min(leftbound,c);
                    rightbound=max(rightbound,c);
                }
        }
    int height = lowerbound-upperbound, width = rightbound-leftbound,max_words=100,max_font_size=-1;
    int baseline=0;
    double font_size=0;
    double last_freq=1.0;


    // Get max_font_size.
    if(max_font_size<0)
    {
        if(wordmap.size()<2)
            font_size=get_size(wordmap[0].first,lowerbound-upperbound,rightbound-leftbound,&baseline);
        else
        {
            font_size=get_max_font_size(wordmap[0],wordmap[1],mask,upperbound,lowerbound,leftbound,rightbound);
        }
    }

    // Determine each word's font size and draw.
    bool stopsign=false;
    for(auto elem: wordmap)
    {
        font_size=(0.5*elem.second/last_freq+0.5)*font_size+font_step;
        cv::Size textsize=cv::getTextSize(elem.first,cv::FONT_HERSHEY_SIMPLEX,font_size,int(font_size/2+1),&baseline);
        cv::Point pos(0,0);
        while(1)
        {
            if(get_position(canvas,mask,upperbound,lowerbound,leftbound,rightbound,textsize,baseline,&pos))
            {
                int color_b=rand()%254;
                int color_g=rand()%254;
                int color_r=rand()%254;
                putText(canvas,elem.first,pos,cv::FONT_HERSHEY_SIMPLEX,font_size,cv::Scalar(color_b, color_g, color_r),int(font_size/2+1), cv::LINE_AA, false);
                break;
            }
            else if(font_size< min_font)
            {
                stopsign=true;
                break;
            }
            else
                {font_size-=font_step;
                textsize=cv::getTextSize(elem.first,cv::FONT_HERSHEY_SIMPLEX,font_size,int(font_size/2+1),&baseline);}
        }
        if(stopsign)
            break;
        last_freq=elem.second;
    }

    // 
    cv::imwrite("Word Cloud.jpg",canvas);
    cv::imshow("Canvas",canvas);
    cv::waitKey(0);
    return 0;

    // 

}