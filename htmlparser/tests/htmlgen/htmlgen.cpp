/*========================================================
  To Do:
    1. Tag sequences on the command line p..table
    2. Dumping documents into the right place
 *========================================================*/


/*========================================================
  Special keywords:
    $0..$9:     represent command line arguments
    @file:      the name of the file being writtent
    @nextfile:  the name of the next file to be written
    @import:    imports text from another file.
 *========================================================*/


#include <iostream.h>
#include <fstream.h>
#include <strstrea.h>
#include <string>

#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include <direct.h>


static char* tagTable[] = {
  "A", "ABBR", "ACRONYM", "ADDRESS", "APPLET", "AREA", 
  "B", "BASE", "BASEFONT", "BDO", "BGSOUND", "BIG", "BLINK", "BLOCKQUOTE", "BODY", "BR", "BUTTON", 
  "CAPTION", "CENTER", "CITE", "CODE", "COL", "COLGROUP", 
  "DD", "DEL", "DFN", "DIR", "DIV", "DL", "DT", 
  "EM", "EMBED", 
  "FIELDSET", "FONT", "FORM", "FRAME", "FRAMESET", 
  "H1", "H2", "H3", "H4", "H5", "H6", "HEAD", "HR", "HTML", 
  "I", "IFRAME", "ILAYER", "IMG", "INPUT", "INS", "ISINDEX", 
  "KBD", "KEYGEN", 
  "LABEL", "LAYER", "LEGEND", "LI", "LINK", "LISTING", 
  "MAP", "MENU", "META", "MULTICOL", 
  "NOBR", "NOEMBED", "NOFRAMES", "NOLAYER", "NOSCRIPT", 
  "OBJECT", "OL", "OPTGROUP", "OPTION", 
  "P", "PARAM", "PLAINTEXT", "PRE", 
  "Q", 
  "S","SAMP","SCRIPT","SELECT","SERVER","SMALL","SOUND","SPACER","SPAN","STRIKE","STRONG","STYLE","SUB","SUP", 
  "TABLE", "TBODY", "TD", "TEXTAREA", "TFOOT", "TH", "THEAD", "TITLE", "TR", "TT", 
  "U", "UL", 
  "VAR", 
  "WBR", 
  "XMP",
  0
};
static char gCWD[1025];
static char gPrevFile[128];
static char gThisFile[128];
static char gNextFile[128];
static int  gFileIndex=0;


int findTag(const char* aTagName) {
  int low = 0;
  int high = 107;
  while (low <= high) {
    int middle = (low + high) >> 1;
    int result = stricmp(aTagName, tagTable[middle]);
    if (result == 0)
      return middle;
    if (result < 0)
      high = middle - 1; 
    else
      low = middle + 1; 
  }
  return -1;
}


/**
 * Call this to find a tag that closely resembles the given tag.
 * Note that we match based on the first char.
 * @update	gess12/23/98
 * @param 
 * @return
 */
int findNearestTag(char* aTag){
  int result=-1;
  if(aTag){
    char  theChar=toupper(aTag[0]);
    int theIndex=-1;
    while(tagTable[++theIndex]){
      if(toupper(tagTable[theIndex][0])==theChar) {
        return theIndex;
      }
    }
  }
  if(toupper(aTag[0])<'A')
    result=0;
  else result=107;
  return result;
}

char* getNthTagAfter(int aRangeIndex,char* aStartTag){
  int theIndex=findTag(aStartTag);
  if(-1==theIndex){
    theIndex=findNearestTag(aStartTag);
  }
  if(-1<theIndex){
    //now that you have the tag, 
    return tagTable[theIndex+aRangeIndex];
  }
  return 0; 
}

/**
 * 
 * @update	gess12/20/98
 * @param 
 * @return
 */
class CMacros {
public:
        CMacros() {
          mCount=0;
          mFilename[0]=0;
          memset(mKeys,0,sizeof(mKeys));
          memset(mRanges,0,sizeof(mRanges));
          memset(mIndex,0,sizeof(mIndex));
          mBorrowedKeys=false;
        }

        CMacros(const CMacros& aCopy) {
          mBorrowedKeys=true;
          mCount=aCopy.mCount;
          strcpy(mFilename,aCopy.mFilename);
          memcpy(mKeys,aCopy.mKeys,sizeof(mKeys));
          memcpy(mIndex,aCopy.mIndex,sizeof(mIndex));
        }

        ~CMacros(){
          if(!mBorrowedKeys) {
            for(int i=0;i<mCount;i++){
              delete [] mKeys[i];
              mKeys[i]=0;
            }
          }
        }

  bool  addMacro(char* aString) {
    
          //copy a macro name...
          //start by stripping off the macro key (everything up to the equal sign...
          if(aString){
            int sLen=strlen(aString);
            if(sLen>0){
              if((strchr(aString,',')) || (strchr(aString,'-'))) {
                mRanges[mCount]=new char[sLen+1];
                strcpy(mRanges[mCount++],aString);
              }
              else {
                mKeys[mCount]=new char[sLen+1];
                strcpy(mKeys[mCount++],aString);
              }
              return true;
            }
          }
          return false;
        }

  int   getCount() {return mCount;}
  
  char* getMacro(int anIndex) {
          if(anIndex<mCount)
            return mKeys[anIndex];
          return 0;
        }

  char* getFilename(void) {return mFilename;}

  char* getRangeValueAt(char* aRange,int aRangeIndex) {
          static char theBuffer[100];
          char* result=0;
          char* p1=aRange;
          char* p2=0;
          int   offset=0;
          int   count=-1;
            
          theBuffer[0]=0;
          if(p2=strchr(&aRange[offset],',')) {
              //in this case, our keys are taken 1 at a time from the range itself.
            while((++count<aRangeIndex) && (p1) && (p2)) {  
              p1=p2+1;
              p2=strchr(p1,',');
            }
            if(p1 && (count==aRangeIndex)) {
              strncat(theBuffer,p1,p2-p1);
            }
            result=theBuffer;
          }
          else if(p2=strchr(&aRange[offset],'-')) {
              //in this case, our keys are computed based on HTMLTags within the given range (exclusive).
            strncat(theBuffer,p1,p2-p1); 
            char* tag=getNthTagAfter(aRangeIndex,theBuffer);
            p2++; //p2 now points at the last item...
            if(tag){
              int end=findNearestTag(p2);
              char* endTag=tagTable[end];
              if(endTag){
                int icmp=stricmp(tag,endTag);
                if(icmp<=0){
                  strcpy(theBuffer,tag);
                }
                else theBuffer[0]=0;
                result=theBuffer;
              }
              else result=0;
            }
          }
          return result;
        }

  bool  first(void) {
          bool result=true;
          for(int i=0;i<mCount;i++){
            mIndex[i]=0;
            if(mRanges[i]){
              char* key=getRangeValueAt(mRanges[i],mIndex[i]);
              if(key) {
                if(!mKeys[i])
                  mKeys[i]=new char[100];
                strcpy(mKeys[i],key);
              }
            }
            //otherwise key is already set...
          }
          sprintf(gThisFile,"out%i.html",gFileIndex++);
          sprintf(gNextFile,"out%i.html",gFileIndex);
          return result;
        }

  bool  next(void) {

          //the trick here is to find the rightmost range and increment it.
          //if it wraps, then reset it to 0, and increment the next range to the left.
          int rangeIndex=mCount;
          bool done=false;
          while((0<=--rangeIndex) && (!done)) {
            if(mRanges[rangeIndex]) {
              //ok, now we know the right most range object.
              //let's see if it can be incremented...
              char* key=getRangeValueAt(mRanges[rangeIndex],++mIndex[rangeIndex]);
              if((0==key)  || (0==key[0])){
                mIndex[rangeIndex]=0;
                char* key=getRangeValueAt(mRanges[rangeIndex],mIndex[rangeIndex]);
              }//if
              else done=true;
              mKeys[rangeIndex][0]=0;
              if(key)
                strcpy(mKeys[rangeIndex],key);
            }//if
          }//while
          strcpy(gPrevFile,gThisFile);
          sprintf(gThisFile,"out%i.html",gFileIndex++);
          sprintf(gNextFile,"out%i.html",gFileIndex);
          return done;
        }

  /**
   * 
   * @update	gess12/23/98
   * @param 
   * @return
   */
  bool  consume(istrstream& aStream){
          char theBuffer[100];
          bool readDefs=false; 

          while(!aStream.eof()){
            aStream >> theBuffer;
            if(!stricmp(theBuffer,"-F")){
              //copy the filename...
              aStream >> theBuffer;
              strcpy(mFilename,theBuffer);
              readDefs=false;
            }
#if 0
            else if(!stricmp(theBuffer,"-D")){
              readDefs=true;
            }
#endif
            else if(!stricmp(theBuffer,"-O")){
              aStream >> theBuffer;
              readDefs=false;
            }
            else {
              if(theBuffer[0]){
                addMacro(theBuffer);
                theBuffer[0]=0;
              }
            }
          }
          return true;
        }

  void  buildArgBuffer(char* aBuffer) {
          aBuffer[0]=0;
          if(mFilename[0]) {
            sprintf(aBuffer,"-o %s -f %s ",gThisFile,mFilename);
          }
          for(int i=0;i<mCount;i++){
            if(mKeys[i]) {
              strcat(aBuffer,mKeys[i]);
              strcat(aBuffer," ");
            }
          }
        }

  void  dump(void) {
          for(int i=0;i<mCount;i++){
            cout << mKeys[i] << " ";
          }
          cout << endl;
        }

protected:
  int   mCount;
  char* mKeys[100];
  char* mRanges[100];
  int   mIndex[100];
  char  mFilename[125];
  bool  mBorrowedKeys;
};

/**
 * 
 * @update	gess12/20/98
 * @param 
 * @return
 */
void expandKeywords(char* aBuffer,CMacros& aMacroSet){
  char  temp[1024];
  char  theWord[100];

  temp[0]=0;
  if(strchr(aBuffer,'@')){

    int pos=-1;
    while(aBuffer[++pos]==' ')
      strcat(temp," ");

    istrstream iStr(aBuffer);
    while(!iStr.eof()){
        theWord[0]=0;
        iStr>>theWord;
        char* thePos=strchr(theWord,'@');
        if(thePos){
          strncat(temp,theWord,thePos-theWord);
          if(!strnicmp(thePos,"@file",5)){
            strcat(temp,gThisFile);
            thePos+=5;
          }
          else if(!strnicmp(thePos,"@nextfile",9)){
            strcat(temp,gNextFile);
            thePos+=9;
          }
          else if(!strnicmp(thePos,"@prevfile",9)){
            strcat(temp,gPrevFile);
            thePos+=9;
          }
          strcat(temp,thePos);
        }
        else strcat(temp,theWord);
        strcat(temp," ");
    }
    strcpy(aBuffer,temp);
  }
}

/**
 * 
 * @update	gess12/20/98
 * @param 
 * @return
 */
void expandMacros(char* aBuffer,CMacros& aMacroSet){
  char  temp[1024];
  int rPos=-1;
  int wPos=0;

  if(aBuffer){
    while(aBuffer[++rPos]){
      if('$'==aBuffer[rPos]){
        temp[wPos]=0;
        ++rPos; //skip the $...        
        int theIndex=aBuffer[rPos]-'0';
        char* theMacro=aMacroSet.getMacro(theIndex);
        if(theMacro){
          strcat(temp,theMacro);
          wPos=strlen(temp);
        }
      }
      else temp[wPos++]=aBuffer[rPos];
    }
    temp[wPos]=0;
    strcpy(aBuffer,temp);
  }
}

/**
 * 
 * @update	gess12/20/98
 * @param 
 * @return
 */
int processFile(char* aDir,CMacros& aMacroList,fstream& anOutputStream,fstream& anInputStream){
  int result=0;

  if(anInputStream.is_open()){
    bool  done=false;
    char  theBuffer[1024];
    char* p=0;

    while((!done) && (0==result)){
    
      anInputStream.getline(theBuffer,sizeof(theBuffer)-1);
      if(anInputStream.gcount()){

          //before doing anything else, expand the macros and keywords...
        expandMacros(theBuffer,aMacroList);
        expandKeywords(theBuffer,aMacroList);

        //Now process each line:
        p=strstr(theBuffer,"@import");
        if(p) {

          //First, see if the line is an htmlgen statement; if so, recurse to read new file...
          char theFilename[1024];

          strcpy(theFilename,"htmlgen -F ");
          p+=8;
          strcat(theFilename,p);

          //If you're here, we found an htmlgen statement. 
          //  To handle this, we have to:
          //    1. strip off the @htmlgen
          //    2. grab the filename and collect the args,
          //    3. and recurse...
          
          fstream theInStream(p,ios::in);
          result=processFile(aDir,aMacroList,anOutputStream,theInStream);
        }
        else anOutputStream << theBuffer << endl;
      }
      else done=true;
    }
  }
  return result;
}


/**
 * This is where the different combinations of arguments
 * gets constructed and passed on for processing.
 * Note that this is even called when the args have only 1 value.
 * @return error code
 */
int iterate(istrstream& aInputArgs){
  int result=0;

  CMacros theArgs;
  theArgs.consume(aInputArgs);

  char theFilename[1024];

  bool done=!theArgs.first();
  while((!done) && (0==result)){
    CMacros theTempArgs(theArgs);

    //theTempArgs.buildArgBuffer(theBuffer);
    //    istrstream theArgStream(theBuffer);

    sprintf(theFilename,"%s\\%s",gCWD,gThisFile);
    fstream theOutStream(theFilename,ios::trunc);    

    char* theInFile=theArgs.getFilename();
    fstream theInStream(theInFile,ios::in);

    result=processFile(gCWD,theTempArgs,theOutStream,theInStream);
    theArgs.dump();
    done=!theArgs.next();
  }
  return result;
}

/**
 * 
 * @update	gess12/20/98
 * @param 
 * @return
 */
int main(int argc,char* argv[]){
  int result=0;

  gPrevFile[0]=gThisFile[0]=gNextFile[0];
  if(argc>1){
    char theBuffer[1024];
    theBuffer[0]=0;

    for(int i=1;i<argc;i++){
      strcat(theBuffer,argv[i]);
      strcat(theBuffer," ");
    }

    _getcwd(gCWD,sizeof(gCWD)-1);
    istrstream aStrStream(theBuffer);
    result=iterate(aStrStream);
  }
  else cout<<endl <<"HTMLGenerator usage -f filename [yyy | xxx,yyy | xxx-yyy]" << endl;
  return result;
}
