/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
  
//#define __INCREMENTAL 1

#include "nsHTMLParser.h"
#include "nsIContentSink.h" 
#include "nsString.h"
#include "nsCRT.h"
#include "COtherDTD.h"
#include "CNavDTD.h"
#include "nsScanner.h"
#include "prenv.h"  //this is here for debug reasons...
#include "plstr.h"
#include "prstrm.h"
#include <fstream.h>
#include "nsIInputStream.h"

/* UNCOMMENT THIS IF STUFF STOPS WORKING...
#ifdef XP_PC
#include <direct.h> //this is here for debug reasons...
#endif
#include <time.h>
#include "prmem.h"
*/

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);                 
static NS_DEFINE_IID(kClassIID, NS_IHTML_PARSER_IID); 
static NS_DEFINE_IID(kIParserIID, NS_IPARSER_IID);

static const char* kNullURL = "Error: Null URL given";
static const char* kNullFilename= "Error: Null filename given";
static const char* kNullTokenizer = "Error: Unable to construct tokenizer";

static char*      gVerificationOutputDir=0;
static PRBool     gRecordingStatistics=PR_TRUE;
static const int  gTransferBufferSize=4096;  //size of the buffer used in moving data from iistream
static char*      gURLRef=0;

#define DEBUG_SAVE_SOURCE_DOC 1
#ifdef DEBUG_SAVE_SOURCE_DOC
fstream* gTempStream=0;
#endif


extern "C" NS_EXPORT void SetVerificationDirectory(char * verify_dir)
{
	gVerificationOutputDir = verify_dir;
}


extern "C" NS_EXPORT void SetRecordStatistics(PRBool bval)
{
	gRecordingStatistics = bval;
}

/**
 *  This method is defined in nsIParser. It is used to 
 *  cause the COM-like construction of an nsHTMLParser.
 *  
 *  @update  gess 3/25/98
 *  @param   nsIParser** ptr to newly instantiated parser
 *  @return  NS_xxx error result
 */
NS_HTMLPARS nsresult NS_NewHTMLParser(nsIParser** aInstancePtrResult)
{
  nsHTMLParser *it = new nsHTMLParser();

  if (it == 0) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return it->QueryInterface(kIParserIID, (void **) aInstancePtrResult);
}

class CTokenDeallocator: public nsDequeFunctor{
public:
  virtual void operator()(void* anObject) {
    CToken* aToken = (CToken*)anObject;
    delete aToken;
  }
};

CTokenDeallocator gTokenKiller;

/**
 *  default constructor
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
nsHTMLParser::nsHTMLParser() : mTokenDeque(gTokenKiller) {
  NS_INIT_REFCNT();
  mListener = nsnull;
  mTransferBuffer=0;
  mSink=0;
  mCurrentPos=0;
  mMarkPos=0;
  mParseMode=eParseMode_unknown;
  mDTD=0;
}


/**
 *  Default destructor
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
nsHTMLParser::~nsHTMLParser() {
  if (gURLRef)
  {
    PL_strfree(gURLRef);
    gURLRef = 0;
  }
  NS_IF_RELEASE(mListener);
  if(mTransferBuffer)
    delete [] mTransferBuffer;
  mTransferBuffer=0;
  NS_RELEASE(mSink);
  if(mCurrentPos)
    delete mCurrentPos;
  mCurrentPos=0;
  if(mDTD)
    delete mDTD;    
  mDTD=0;
  if(mScanner)
    delete mScanner;
  mScanner=0;
}


NS_IMPL_ADDREF(nsHTMLParser)
NS_IMPL_RELEASE(nsHTMLParser)
//NS_IMPL_ISUPPORTS(nsHTMLParser,NS_IHTML_PARSER_IID)


/**
 *  This method gets called as part of our COM-like interfaces.
 *  Its purpose is to create an interface to parser object
 *  of some type.
 *  
 *  @update   gess 3/25/98
 *  @param    nsIID  id of object to discover
 *  @param    aInstancePtr ptr to newly discovered interface
 *  @return   NS_xxx result code
 */
nsresult nsHTMLParser::QueryInterface(const nsIID& aIID, void** aInstancePtr)  
{                                                                        
  if (NULL == aInstancePtr) {                                            
    return NS_ERROR_NULL_POINTER;                                        
  }                                                                      

  if(aIID.Equals(kISupportsIID))    {  //do IUnknown...
    *aInstancePtr = (nsIParser*)(this);                                        
  }
  else if(aIID.Equals(kIParserIID)) {  //do IParser base class...
    *aInstancePtr = (nsIParser*)(this);                                        
  }
  else if(aIID.Equals(kClassIID)) {  //do this class...
    *aInstancePtr = (nsHTMLParser*)(this);                                        
  }                 
  else {
    *aInstancePtr=0;
    return NS_NOINTERFACE;
  }
  ((nsISupports*) *aInstancePtr)->AddRef();
  return NS_OK;                                                        
}

/**
 *  This method gets called in order to set the content
 *  sink for this parser to dump nodes to.
 *  
 *  @update  gess 3/25/98
 *  @param   nsIContentSink interface for node receiver
 *  @return  
 */
nsIContentSink* nsHTMLParser::SetContentSink(nsIContentSink* aSink) {
  NS_PRECONDITION(0!=aSink,"sink cannot be null!");
  nsIContentSink* old=mSink;
  if(old)
    NS_RELEASE(old);
  if(aSink) {
    mSink=aSink;
    NS_ADDREF(aSink);
  }
  return old;
}


/**
 *  
 *  
 *  @update  gess 6/9/98
 *  @param   
 *  @return  
 */
void nsHTMLParser::SetDTD(nsIDTD* aDTD) {
  mDTD=aDTD;
}

/**
 *  
 *  
 *  @update  gess 6/9/98
 *  @param   
 *  @return  
 */
CScanner* nsHTMLParser::GetScanner(void){
  return mScanner;
}

/**
 *  This is where we loop over the tokens created in the 
 *  tokenization phase, and try to make sense out of them. 
 *
 *  @update  gess 3/25/98
 *  @param   
 *  @return  PR_TRUE if parse succeeded, PR_FALSE otherwise.
 */
PRInt32 nsHTMLParser::IterateTokens() {
  nsDequeIterator e=mTokenDeque.End(); 
  nsDequeIterator theMarkPos(e);

  if(!mCurrentPos)
    mCurrentPos=new nsDequeIterator(mTokenDeque.Begin());

  PRInt32 result=kNoError;

  while((kNoError==result) && ((*mCurrentPos<e))){
    mMinorIteration++;
    CToken* theToken=(CToken*)mCurrentPos->GetCurrent();

    theMarkPos=*mCurrentPos;
    result=mDTD->HandleToken(theToken);
    ++(*mCurrentPos);
  }

  if(kInterrupted==result)
    *mCurrentPos=theMarkPos;

  return result;
}


/**
 *  
 *  
 *  @update  gess 5/13/98
 *  @param   
 *  @return  
 */
eParseMode DetermineParseMode() {
  const char* theModeStr= PR_GetEnv("PARSE_MODE");
  const char* other="other";
  eParseMode  result=eParseMode_navigator;

  if(theModeStr) 
    if(0==nsCRT::strcasecmp(other,theModeStr))
      result=eParseMode_other;    
  return result;
}


/**
 *  
 *  
 *  @update  gess 5/13/98
 *  @param   
 *  @return  
 */
nsIDTD* GetDTD(eParseMode aMode) {
  nsIDTD* aDTD=0;
  switch(aMode) {
    case eParseMode_navigator:
      aDTD=new CNavDTD(); break;
    case eParseMode_other:
      aDTD=new COtherDTD(); break;
    default:
      break;
  }
  return aDTD;
}


/**
 * 
 * @update	gess5/18/98
 * @param 
 * @return
 */
PRInt32 nsHTMLParser::WillBuildModel(void) {
  mMajorIteration=-1;
  mMinorIteration=-1;

  if(mDTD)
    mDTD->WillBuildModel();

#ifdef DEBUG_SAVE_SOURCE_DOC
#if defined(XP_UNIX) && defined(IRIX)
  /* XXX: IRIX does not support ios::binary */
  gTempStream =new fstream("c:/temp/out.html",ios::out);
#else
  gTempStream = new fstream("c:/temp/out.html",ios::out|ios::binary);
#endif
#endif

  return kNoError;
}

/**
 * 
 * @update	gess5/18/98
 * @param 
 * @return
 */
PRInt32 nsHTMLParser::DidBuildModel(PRInt32 anErrorCode) {
  //One last thing...close any open containers.
  PRInt32 result=anErrorCode;
  if(mDTD) {
    result=mDTD->DidBuildModel(anErrorCode);
  }

#ifdef DEBUG_SAVE_SOURCE_DOC
  if(gTempStream) {
    gTempStream->close();
    delete gTempStream;
    gTempStream=0;
  }
#endif

  return anErrorCode;
}

/**
 *  This DEBUG ONLY method is used to simulate a network-based
 *  i/o model where data comes in incrementally.
 *  
 *  @update  gess 5/13/98
 *  @param   aFilename is the name of the disk file to use for testing.
 *  @return  error code (kNoError means ok)
 */
PRInt32 nsHTMLParser::ParseFileIncrementally(const char* aFilename){
  PRInt32   result=kBadFilename;
  fstream*  theFileStream;
  nsString  theBuffer;
  const int kLocalBufSize=10;

  if (gURLRef)
     PL_strfree(gURLRef);
  if (aFilename)
     gURLRef = PL_strdup(aFilename);

  mMajorIteration=-1;
  mMinorIteration=-1;

#if defined(XP_UNIX) && defined(IRIX)
  /* XXX: IRIX does not support ios::binary */
  theFileStream=new fstream(aFilename,ios::in);
#else
  theFileStream=new fstream(aFilename,ios::in|ios::binary);
#endif
  if(theFileStream) {
    result=kNoError;
    while((kNoError==result) || (kInterrupted==result)) {
      //read some data from the file...

      char buf[kLocalBufSize];
      buf[kLocalBufSize]=0;

      if(theFileStream) {
        theFileStream->read(buf,kLocalBufSize);
        PRInt32 numread=theFileStream->gcount();
        if(numread>0) {
          buf[numread]=0;
          theBuffer.Truncate();
          theBuffer.Append(buf);
          mScanner->Append(theBuffer);
          result=ResumeParse();
        }
        else break;
      }

    }
    theFileStream->close();
    delete theFileStream;
  }
  return result;
}

/**
 *  This is the main controlling routine in the parsing process. 
 *  Note that it may get called multiple times for the same scanner, 
 *  since this is a pushed based system, and all the tokens may 
 *  not have been consumed by the scanner during a given invocation 
 *  of this method. 
 *
 *  @update  gess 3/25/98
 *  @param   aFilename -- const char* containing file to be parsed.
 *  @return  PR_TRUE if parse succeeded, PR_FALSE otherwise.
 */
PRBool nsHTMLParser::Parse(const char* aFilename,PRBool aIncremental){
  NS_PRECONDITION(0!=aFilename,kNullFilename);
  PRInt32 status=kBadFilename;
  mIncremental=aIncremental;

  if(aFilename) {

    if (gURLRef)
       PL_strfree(gURLRef);
    gURLRef = PL_strdup(aFilename);

    mParseMode=DetermineParseMode();  
    mDTD=(0==mDTD) ? GetDTD(mParseMode) : mDTD;
    if(mDTD) {
      mDTD->SetParser(this);
      mDTD->SetContentSink(mSink);
    }

    WillBuildModel();

    //ok, time to create our tokenizer and begin the process
    if(aIncremental) {
      mScanner=new CScanner(mParseMode);
      status=ParseFileIncrementally(aFilename);
    }
    else {
      //ok, time to create our tokenizer and begin the process
      mScanner=new CScanner(aFilename,mParseMode);
      status=ResumeParse();
    }
    DidBuildModel(status);

  }
  return status;
}

/**
 *  This is the main controlling routine in the parsing process. 
 *  Note that it may get called multiple times for the same scanner, 
 *  since this is a pushed based system, and all the tokens may 
 *  not have been consumed by the scanner during a given invocation 
 *  of this method. 
 *
 *  @update  gess 3/25/98
 *  @param   aFilename -- const char* containing file to be parsed.
 *  @return  PR_TRUE if parse succeeded, PR_FALSE otherwise.
 */
PRInt32 nsHTMLParser::Parse(nsIURL* aURL,
                            nsIStreamListener* aListener,
                            PRBool aIncremental) {
  NS_PRECONDITION(0!=aURL,kNullURL);

  PRInt32 status=kBadURL;

    //set the rickGDebug flag to 1 if you want to try incrementally
    //loading your document from a text file (given below).
  static int rickGDebug=0;
  if(rickGDebug)
    return Parse("c:/temp/temp.html",PR_TRUE);

  NS_IF_RELEASE(mListener);
  mListener = aListener;
  NS_IF_ADDREF(aListener);

  mIncremental=aIncremental;

  if(aURL) {

     if (gURLRef)
     {
        PL_strfree(gURLRef);
        gURLRef = 0;
     }
     if (aURL->GetSpec())
        gURLRef = PL_strdup(aURL->GetSpec());

    mParseMode=DetermineParseMode();  
    mDTD=(0==mDTD) ? GetDTD(mParseMode) : mDTD;
    if(mDTD) {
      mDTD->SetParser(this);
      mDTD->SetContentSink(mSink);
    }

    WillBuildModel();

    //ok, time to create our tokenizer and begin the process
    if(mIncremental) {  
      mScanner=new CScanner(mParseMode);
      status=aURL->Open(this);
    }
    else {
      mScanner=new CScanner(aURL,mParseMode);
      WillBuildModel();
      status=ResumeParse();
      DidBuildModel(status);
    }

  }
  return status;
}

/**
 * Call this method if all you want to do is parse 1 string full of HTML text.
 *
 * @update	gess5/11/98
 * @param   anHTMLString contains a string-full of real HTML
 * @param   appendTokens tells us whether we should insert tokens inline, or append them.
 * @return  TRUE if all went well -- FALSE otherwise
 */
PRInt32 nsHTMLParser::Parse(nsString& aSourceBuffer,PRBool appendTokens){
  PRInt32 result=kNoError;
  
  WillBuildModel();
  mScanner->Append(aSourceBuffer);
  result=ResumeParse();
  DidBuildModel(result);
  
  return result;
}

/**
 *  This routine is called to cause the parser to continue
 *  parsing it's underling stream. This call allows the
 *  parse process to happen in chunks, such as when the
 *  content is push based, and we need to parse in pieces.
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  PR_TRUE if parsing concluded successfully.
 */
PRInt32 nsHTMLParser::ResumeParse() {
  PRInt32 result=kNoError;

  mDTD->WillResumeParse();
  if(kNoError==result) {
    result=Tokenize();
    if(kInterrupted==result)
      mDTD->WillInterruptParse();
    IterateTokens();
  }
  return result;
}


/**
 * Retrieve the attributes for this node, and add then into
 * the node.
 *
 * @update  gess4/22/98
 * @param   aNode is the node you want to collect attributes for
 * @param   aCount is the # of attributes you're expecting
 * @return error code (should be 0)
 */
PRInt32 nsHTMLParser::CollectAttributes(nsCParserNode& aNode,PRInt32 aCount){
  nsDequeIterator end=mTokenDeque.End();

  int attr=0;
  for(attr=0;attr<aCount;attr++) {
    if(*mCurrentPos<end) {
      CToken* tkn=(CToken*)(++(*mCurrentPos));
      if(tkn){
        if(eToken_attribute==eHTMLTokenTypes(tkn->GetTokenType())){
          aNode.AddAttribute(tkn);
        } 
        else (*mCurrentPos)--;
      }
      else return kInterrupted;
    }
    else return kInterrupted;
  }
  return kNoError;
}


/**
 * 
 * @update  gess4/22/98
 * @param 
 * @return
 */
PRInt32 nsHTMLParser::CollectSkippedContent(nsCParserNode& aNode){
  eHTMLTokenTypes   subtype=eToken_attribute;
  nsDequeIterator   end=mTokenDeque.End();
  PRInt32           count=0;

  while((*mCurrentPos!=end) && (eToken_attribute==subtype)) {
    CToken* tkn=(CToken*)(++(*mCurrentPos));
    subtype=eHTMLTokenTypes(tkn->GetTokenType());
    if(eToken_skippedcontent==subtype) {
      aNode.SetSkippedContent(tkn);
      count++;
    } 
    else (*mCurrentPos)--;
  }
  return count;
}

/**
 *  
 *  
 *  @update  gess 5/12/98
 *  @param   
 *  @return  
 */
nsresult nsHTMLParser::GetBindInfo(void){
  nsresult result=0;
  return result;
}

/**
 *  
 *  
 *  @update  gess 5/12/98
 *  @param   
 *  @return  
 */
nsresult
nsHTMLParser::OnProgress(PRInt32 aProgress, PRInt32 aProgressMax,
                         const nsString& aMsg)
{
  nsresult result=0;
  if (nsnull != mListener) {
    mListener->OnProgress(aProgress, aProgressMax, aMsg);
  }
  return result;
}

/**
 *  
 *  
 *  @update  gess 5/12/98
 *  @param   
 *  @return  
 */
nsresult nsHTMLParser::OnStartBinding(const char *aContentType){
  if (nsnull != mListener) {
    mListener->OnStartBinding(aContentType);
  }
  nsresult result=WillBuildModel();
  if(!mTransferBuffer) {
    mTransferBuffer=new char[gTransferBufferSize+1];
  }
  return result;
}

/**
 *  
 *  
 *  @update  gess 5/12/98
 *  @param   pIStream contains the input chars
 *  @param   length is the number of bytes waiting input
 *  @return  error code (usually 0)
 */
nsresult nsHTMLParser::OnDataAvailable(nsIInputStream *pIStream, PRInt32 length){
  if (nsnull != mListener) {
    mListener->OnDataAvailable(pIStream, length);
  }

  int len=0;
  int offset=0;

  do {
      PRInt32 err;
      len = pIStream->Read(&err, mTransferBuffer, 0, gTransferBufferSize);
      if(len>0) {

        #ifdef DEBUG_SAVE_SOURCE_DOC
          if(gTempStream) {
            gTempStream->write(mTransferBuffer,len);
          }
        #endif

        mScanner->Append(&mTransferBuffer[offset],len);

      } //if
  } while (len > 0);

  nsresult result=ResumeParse();
  return result;
}

/**
 *  
 *  
 *  @update  gess 5/12/98
 *  @param   
 *  @return  
 */
nsresult nsHTMLParser::OnStopBinding(PRInt32 status, const nsString& aMsg){
  nsresult result=DidBuildModel(status);
  if (nsnull != mListener) {
    mListener->OnStopBinding(status, aMsg);
  }
  return result;
}


/*******************************************************************
  Here comes the tokenization methods...
 *******************************************************************/

/**
 *  Cause the tokenizer to consume the next token, and 
 *  return an error result.
 *  
 *  @update  gess 3/25/98
 *  @param   anError -- ref to error code
 *  @return  new token or null
 */
PRInt32 nsHTMLParser::ConsumeToken(CToken*& aToken) {
  PRInt32 result=mDTD->ConsumeToken(aToken);
  return result;
}


/**
 *  Part of the code sandwich, this gets called right before
 *  the tokenization process begins. The main reason for
 *  this call is to allow the delegate to do initialization.
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  TRUE if it's ok to proceed
 */
PRBool nsHTMLParser::WillTokenize(PRBool aIncremental){
  PRBool result=PR_TRUE;
  return result;
}

/**
 *  
 *  @update  gess 3/25/98
 *  @return  TRUE if it's ok to proceed
 */
PRInt32 nsHTMLParser::Tokenize(nsString& aSourceBuffer,PRBool appendTokens){
  CToken* theToken=0;
  PRInt32 result=kNoError;
  
  WillTokenize(PR_TRUE);

  while(kNoError==result) {
    result=ConsumeToken(theToken);
    if(theToken && (kNoError==result)) {

#ifdef VERBOSE_DEBUG
        theToken->DebugDumpToken(cout);
#endif
      mTokenDeque.Push(theToken);
    }
  } 
  if(kEOF==result)
    result=kNoError;
  DidTokenize(PR_TRUE);
  return result;
}

/**
 *  This is the primary control routine. It iteratively
 *  consumes tokens until an error occurs or you run out
 *  of data.
 *  
 *  @update  gess 3/25/98
 *  @return  error code 
 */
PRInt32 nsHTMLParser::Tokenize(void) {
  CToken* theToken=0;
  PRInt32 result=kNoError;
  PRBool  done=(0==mMajorIteration) ? (!WillTokenize(PR_TRUE)) : PR_FALSE;
  

  while((PR_FALSE==done) && (kNoError==result)) {
    mScanner->Mark();
    result=ConsumeToken(theToken);
    if(kNoError==result) {
      if(theToken) {

  #ifdef VERBOSE_DEBUG
          theToken->DebugDumpToken(cout);
  #endif
        mTokenDeque.Push(theToken);
      }

    }
    else {
      if(theToken)
        delete theToken;
      mScanner->RewindToMark();
    }
  } 
  if((PR_TRUE==done)  && (kInterrupted!=result))
    DidTokenize(PR_TRUE);
  return result;
}

/**
 *  This is the tail-end of the code sandwich for the
 *  tokenization process. It gets called once tokenziation
 *  has completed.
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  TRUE if all went well
 */
PRBool nsHTMLParser::DidTokenize(PRBool aIncremental) {
  PRBool result=PR_TRUE;

#ifdef VERBOSE_DEBUG
    DebugDumpTokens(cout);
#endif

  return result;
}

/**
 *  This debug routine is used to cause the tokenizer to
 *  iterate its token list, asking each token to dump its
 *  contents to the given output stream.
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
void nsHTMLParser::DebugDumpTokens(ostream& out) {
  nsDequeIterator b=mTokenDeque.Begin();
  nsDequeIterator e=mTokenDeque.End();

  CToken* theToken;
  while(b!=e) {
    theToken=(CToken*)(b++);
    theToken->DebugDumpToken(out);
  }
}


/**
 *  This debug routine is used to cause the tokenizer to
 *  iterate its token list, asking each token to dump its
 *  contents to the given output stream.
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
void nsHTMLParser::DebugDumpSource(ostream& out) {
  nsDequeIterator b=mTokenDeque.Begin();
  nsDequeIterator e=mTokenDeque.End();

  CToken* theToken;
  while(b!=e) {
    theToken=(CToken*)(b++);
    theToken->DebugDumpSource(out);
  }

}


