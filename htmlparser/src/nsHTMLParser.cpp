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
#include "nsHTMLContentSink.h" 
#include "nsTokenizer.h"
#include "nsHTMLTokens.h"
#include "nsString.h"
#include "nsIURL.h"
#include "nsCRT.h"
#include "COtherDelegate.h"
#include "COtherDTD.h"
#include "CNavDelegate.h"
#include "CNavDTD.h"
#include "prenv.h"  //this is here for debug reasons...
#include "prio.h"
#include "plstr.h"
#include <fstream.h>
#include "nsIInputStream.h"
#ifdef XP_PC
#include <direct.h> //this is here for debug reasons...
#endif

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);                 
static NS_DEFINE_IID(kClassIID, NS_IHTML_PARSER_IID); 
static NS_DEFINE_IID(kIParserIID, NS_IPARSER_IID);

static const char* kNullURL = "Error: Null URL given";
static const char* kNullFilename= "Error: Null filename given";
static const char* kNullTokenizer = "Error: Unable to construct tokenizer";
static const char* kNullToken = "Error: Null token given";
static const char* kInvalidTagStackPos = "Error: invalid tag stack position";

static char*  gVerificationOutputDir=0;
static char*  gURLRef=0;
static int    rickGDebug=0;
static const int gTransferBufferSize=4096;  //size of the buffer used in moving data from iistream


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


/**
 *  This big dispatch method is used to route token handler calls to the right place.
 *  What's wrong with it? This table, and the dispatch methods themselves need to be 
 *  moved over to the delegate. Ah, so much to do...
 *  
 *  @update  gess 5/21/98
 *  @param   aType
 *  @param   aToken
 *  @param   aParser
 *  @return  
 */
PRInt32 DispatchTokenHandler(eHTMLTokenTypes aType,CToken* aToken,nsHTMLParser* aParser){
  PRInt32 result=0;

  if(aToken && aParser) {
    switch(aType) {
      case eToken_start:
        result=aParser->HandleStartToken(aToken); break;
      case eToken_end:
        result=aParser->HandleEndToken(aToken); break;
      case eToken_comment:
        result=aParser->HandleCommentToken(aToken); break;
      case eToken_entity:
        result=aParser->HandleEntityToken(aToken); break;
      case eToken_whitespace:
        result=aParser->HandleStartToken(aToken); break;
      case eToken_newline:
        result=aParser->HandleStartToken(aToken); break;
      case eToken_text:
        result=aParser->HandleStartToken(aToken); break;
      case eToken_attribute:
        result=aParser->HandleAttributeToken(aToken); break;
      case eToken_style:
        result=aParser->HandleStyleToken(aToken); break;
      case eToken_skippedcontent:
        result=aParser->HandleSkippedContentToken(aToken); break;
      default:
        result=0;
    }//switch
  }//if
  return result;
}

/**
 *  init the set of default token handlers...
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
void nsHTMLParser::InitializeDefaultTokenHandlers() {
  AddTokenHandler(new CTokenHandler(DispatchTokenHandler,eToken_start));

  AddTokenHandler(new CTokenHandler(DispatchTokenHandler,eToken_end));
  AddTokenHandler(new CTokenHandler(DispatchTokenHandler,eToken_comment));
  AddTokenHandler(new CTokenHandler(DispatchTokenHandler,eToken_entity));

  AddTokenHandler(new CTokenHandler(DispatchTokenHandler,eToken_whitespace));
  AddTokenHandler(new CTokenHandler(DispatchTokenHandler,eToken_newline));
  AddTokenHandler(new CTokenHandler(DispatchTokenHandler,eToken_text));
  
  AddTokenHandler(new CTokenHandler(DispatchTokenHandler,eToken_attribute));
//  AddTokenHandler(new CTokenHandler(DispatchTokenHandler,eToken_script));
  AddTokenHandler(new CTokenHandler(DispatchTokenHandler,eToken_style));
  AddTokenHandler(new CTokenHandler(DispatchTokenHandler,eToken_skippedcontent));
}

/**
 *  default constructor
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
nsHTMLParser::nsHTMLParser() {
  NS_INIT_REFCNT();
  mListener = nsnull;
  mTransferBuffer=0;
  mSink=0;
  mContextStackPos=0;
  mCurrentPos=0;
  mMarkPos=0;
  mParseMode=eParseMode_unknown;
  nsCRT::zero(mContextStack,sizeof(mContextStack));
  nsCRT::zero(mTokenHandlers,sizeof(mTokenHandlers));
  mDTD=0;
  mHasOpenForm=PR_FALSE;
  InitializeDefaultTokenHandlers();
  gVerificationOutputDir = PR_GetEnv("VERIFY_PARSER");
  gURLRef = 0;
}


/**
 *  Default destructor
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
nsHTMLParser::~nsHTMLParser() {
  DeleteTokenHandlers();
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
  if(mTokenizer)
    delete mTokenizer;
  if(mDTD)
    delete mDTD;
  mTokenizer=0;
  mDTD=0;
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
 *  This method allows the caller to determine if a form
 *  element is currently open.
 *  
 *  @update  gess 4/2/98
 *  @param   
 *  @return  
 */
PRBool nsHTMLParser::HasOpenContainer(PRInt32 aContainer) const {
  PRBool result=PR_FALSE;

  switch((eHTMLTags)aContainer) {
    case eHTMLTag_form:
      result=mHasOpenForm; break;

    default:
      result=(kNotFound!=GetTopmostIndex((eHTMLTags)aContainer)); break;
  }
  return result;
}

/**
 *  This method retrieves the HTMLTag type of the topmost
 *  container on the stack.
 *  
 *  @update  gess 4/2/98
 *  @param   
 *  @return  
 */
eHTMLTags nsHTMLParser::GetTopNode() const {
  if(mContextStackPos) 
    return (eHTMLTags)mContextStack[mContextStackPos-1];
  return eHTMLTag_unknown;
}


/**
 *  Determine whether the given tag is open anywhere
 *  in our context stack.
 *  
 *  @update  gess 4/2/98
 *  @param   eHTMLTags tag to be searched for in stack
 *  @return  topmost index of tag on stack
 */
PRInt32 nsHTMLParser::GetTopmostIndex(eHTMLTags aTag) const {
  int i=0;
  for(i=mContextStackPos-1;i>=0;i--){
    if(mContextStack[i]==aTag)
      return i;
  }
  return kNotFound;
}

/**
 *  Finds a tag handler for the given tag type, given in string.
 *  
 *  @update  gess 4/2/98
 *  @param   aString contains name of tag to be handled
 *  @return  valid tag handler (if found) or null
 */
nsHTMLParser& nsHTMLParser::DeleteTokenHandlers(void) {
  int i=0;
  for(i=eToken_unknown;i<eToken_last;i++){
    delete mTokenHandlers[i];
    mTokenHandlers[i]=0;
  }
  return *this;
}


/**
 *  Finds a tag handler for the given tag type.
 *  
 *  @update  gess 4/2/98
 *  @param   aTagType type of tag to be handled
 *  @return  valid tag handler (if found) or null
 */
CTokenHandler* nsHTMLParser::GetTokenHandler(eHTMLTokenTypes aType) const {
  CTokenHandler* result=0;
  if((aType>0) && (aType<eToken_last)) {
    result=mTokenHandlers[aType];
  } 
  else {
  }
  return result;
}


/**
 *  Register a handler.
 *  
 *  @update  gess 4/2/98
 *  @param   
 *  @return  
 */
CTokenHandler* nsHTMLParser::AddTokenHandler(CTokenHandler* aHandler) {
  NS_ASSERTION(0!=aHandler,"Error: Null handler argument");
  
  if(aHandler)  {
    eHTMLTokenTypes type=aHandler->GetTokenType();
    if(type<eToken_last) {
      CTokenHandler* old=mTokenHandlers[type];
      mTokenHandlers[type]=aHandler;
    }
    else {
      //add code here to handle dynamic tokens...
    }
  }
  return 0;
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
    mSink=(nsHTMLContentSink*)(aSink);
    NS_ADDREF(aSink);
  }
  return old;
}


/**
 * This debug method allows us to determine whether or not 
 * we've seen (and can handle) the given context vector.
 *
 * @update  gess4/22/98
 * @param   tags is an array of eHTMLTags
 * @param   count represents the number of items in the tags array
 * @param   aDTD is the DTD we plan to ask for verification
 * @return  TRUE if we know how to handle it, else false
 */
PRBool VerifyContextVector(PRInt32 aTags[],PRInt32 count,nsIDTD* aDTD) {

  PRBool  result=PR_TRUE;

  if(0!=gVerificationOutputDir) {
  
#ifdef XP_PC
      char    path[_MAX_PATH+1];
      strcpy(path,gVerificationOutputDir);
#endif

      int i=0;      
      for(i=0;i<count;i++){

#ifdef NS_WIN32
        strcat(path,"/");
        const char* name=GetTagName(aTags[i]);
        strcat(path,name);
        mkdir(path);
#endif
      }
      //ok, now see if we understand this vector
      result=aDTD->VerifyContextVector(aTags,count);

	  if(PR_FALSE==result){
#ifdef NS_WIN32
      // save file to directory indicated by bad context vector
      int iCount = 1;
      char filename[_MAX_PATH];
      do {
         sprintf(filename,"%s/html%04d.dbg", path, iCount++);
      } while (PR_Access(filename,PR_ACCESS_EXISTS) == PR_SUCCESS);
      PRFileDesc * debugFile = PR_Open(filename,PR_CREATE_FILE|PR_RDWR,0);
      if (debugFile) {
         PR_Write(debugFile,gURLRef,PL_strlen(gURLRef));
         PR_Write(debugFile,"\n",PL_strlen("\n"));
         PR_Close(debugFile);
      }
#endif
      //add debugging code here to record the fact that we just encountered
      //a context vector we don't know how to handle.
    }
  }

  return result;
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
  nsDeque& deque=mTokenizer->GetDeque();
  nsDequeIterator e=deque.End(); 
  nsDequeIterator theMarkPos(e);

  if(!mCurrentPos)
    mCurrentPos=new nsDequeIterator(deque.Begin());

  PRInt32 result=kNoError;

  while((kNoError==result) && ((*mCurrentPos<e))){

    CToken* theToken=(CToken*)mCurrentPos->GetCurrent();
    
    eHTMLTokenTypes type=eHTMLTokenTypes(theToken->GetTokenType());
    CTokenHandler* aHandler=GetTokenHandler(type);

    if(aHandler) {
      theMarkPos=*mCurrentPos;
      result=(*aHandler)(theToken,this);
      VerifyContextVector(mContextStack,mContextStackPos,mDTD);
    }
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
void GetDelegateAndDTD(eParseMode aMode,ITokenizerDelegate*& aDelegate,nsIDTD*& aDTD) {
  switch(aMode) {
    case eParseMode_navigator:
      aDelegate=new CNavDelegate(); break;
    case eParseMode_other:
      aDelegate=new COtherDelegate(); break;
    default:
      break;
  }
  if(aDelegate)
    aDTD=aDelegate->GetDTD();
}


/**
 * 
 * @update	gess5/18/98
 * @param 
 * @return
 */
PRInt32 nsHTMLParser::WillBuildModel(void) {
  mIteration=-1;
  mHasSeenOpenTag=PR_FALSE;
  if(mSink)
    mSink->WillBuildModel();
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
  if((kNoError==anErrorCode) && (mContextStackPos>0)) {
    CloseContainersTo(0);
  }
  if(mSink) {
    mSink->DidBuildModel();
  }
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
  fstream*  mFileStream;
  nsString  theBuffer;
  const int kLocalBufSize=10;

  if (gURLRef)
     PL_strfree(gURLRef);
  if (aFilename)
     gURLRef = PL_strdup(aFilename);

  mIteration=-1;

#if defined(XP_UNIX) && defined(IRIX)
  /* XXX: IRIX does not support ios::binary */
  mFileStream=new fstream(aFilename,ios::in);
#else
  mFileStream=new fstream(aFilename,ios::in|ios::binary);
#endif
  if(mFileStream) {
    result=kNoError;
    while((kNoError==result) || (kInterrupted==result)) {
      //read some data from the file...

      char buf[kLocalBufSize];
      buf[kLocalBufSize]=0;

      if(mFileStream) {
        mFileStream->read(buf,kLocalBufSize);
        PRInt32 numread=mFileStream->gcount();
        if(numread>0) {
          buf[numread]=0;
          theBuffer.Truncate();
          theBuffer.Append(buf);
          mTokenizer->Append(theBuffer);
          result=ResumeParse();
        }
        else break;
      }

    }
    mFileStream->close();
    delete mFileStream;
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
  mParseMode=DetermineParseMode();  

  if(aFilename) {

    if (gURLRef)
       PL_strfree(gURLRef);
    gURLRef = PL_strdup(aFilename);
    GetDelegateAndDTD(mParseMode,mDelegate,mDTD);
    if(mDelegate) {

      if(mDTD)
        mDTD->SetParser(this);

      WillBuildModel();

      //ok, time to create our tokenizer and begin the process
      if(aIncremental) {
        mTokenizer=new CTokenizer(mDelegate,mParseMode);
        status=ParseFileIncrementally(aFilename);
      }
      else {
        //ok, time to create our tokenizer and begin the process
        mTokenizer=new CTokenizer(aFilename,mDelegate,mParseMode);
        status=ResumeParse();
      }
      DidBuildModel(status);
    }//if
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

  if(rickGDebug)
    return Parse("c:/temp/temp.html",PR_TRUE);

  NS_IF_RELEASE(mListener);
  mListener = aListener;
  NS_IF_ADDREF(aListener);

  mIncremental=aIncremental;
  mParseMode=DetermineParseMode();   

  if(aURL) {

     if (gURLRef)
     {
        PL_strfree(gURLRef);
        gURLRef = 0;
     }
     if (aURL->GetSpec())
        gURLRef = PL_strdup(aURL->GetSpec());

    GetDelegateAndDTD(mParseMode,mDelegate,mDTD);
    if(mDelegate) {

      if(mDTD)
        mDTD->SetParser(this);

      WillBuildModel();

      //ok, time to create our tokenizer and begin the process
      if(mIncremental) {  
        mTokenizer=new CTokenizer(mDelegate,mParseMode);   
        status=aURL->Open(this);
      }
      else {
        mTokenizer=new CTokenizer(aURL,mDelegate,mParseMode);
        WillBuildModel();
        status=ResumeParse();
        DidBuildModel(status);
      }
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
  mTokenizer->Append(aSourceBuffer);
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

  mSink->WillResume();
  if(kNoError==result) {
    result=mTokenizer->Tokenize(++mIteration);
    if(kInterrupted==result)
      mSink->WillInterrupt();
    IterateTokens();
  }
  return result;
}

/**
 * 
 * @update  gess4/22/98
 * @param 
 * @return
 */
PRInt32 nsHTMLParser::GetStack(PRInt32* aStackPtr) {
  aStackPtr=&mContextStack[0];
  return mContextStackPos;
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
  nsDeque&        deque=mTokenizer->GetDeque();
  nsDequeIterator end=deque.End();

  int attr=0;
  for(attr=0;attr<aCount;attr++) {
    if(*mCurrentPos<end) {
      CHTMLToken* tkn=(CHTMLToken*)(++(*mCurrentPos));
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
  eHTMLTokenTypes subtype=eToken_attribute;
  nsDeque&         deque=mTokenizer->GetDeque();
  nsDequeIterator  end=deque.End();
  PRInt32         count=0;

  while((*mCurrentPos!=end) && (eToken_attribute==subtype)) {
    CHTMLToken* tkn=(CHTMLToken*)(++(*mCurrentPos));
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
 *  This method gets called when a start token has been 
 *  encountered in the parse process. If the current container
 *  can contain this tag, then add it. Otherwise, you have
 *  two choices: 1) create an implicit container for this tag
 *                  to be stored in
 *               2) close the top container, and add this to
 *                  whatever container ends up on top.
 *  
 *  @update  gess 3/25/98
 *  @param   aToken -- next (start) token to be handled
 *  @param   aNode -- CParserNode representing this start token
 *  @return  PR_TRUE if all went well; PR_FALSE if error occured
 */
PRInt32 nsHTMLParser::HandleDefaultStartToken(CToken* aToken,eHTMLTags aChildTag,nsCParserNode& aNode) {
  NS_PRECONDITION(0!=aToken,kNullToken);

  eHTMLTags parentTag=(eHTMLTags)GetTopNode();
  PRInt32   result=kNoError;
  PRBool    contains=mDTD->CanContain(parentTag,aChildTag);

  if(PR_FALSE==contains){
    result=CreateContextStackFor(aChildTag);
    if(kNoError!=result) {
      //if you're here, then the new topmost container can't contain aToken.
      //You must determine what container hierarchy you need to hold aToken,
      //and create that on the parsestack.
      result=ReduceContextStackFor(aChildTag);
      if(PR_FALSE==mDTD->CanContain(GetTopNode(),aChildTag)) {
        //we unwound too far; now we have to recreate a valid context stack.
        result=CreateContextStackFor(aChildTag);
      }
    }
  }

  if(mDTD->IsContainer(aChildTag)){
    result=OpenContainer(aNode);
  }
  else {
    result=AddLeaf(aNode);
  }
  return result;
}

/**
 *  This method gets called when a start token has been 
 *  encountered in the parse process. If the current container
 *  can contain this tag, then add it. Otherwise, you have
 *  two choices: 1) create an implicit container for this tag
 *                  to be stored in
 *               2) close the top container, and add this to
 *                  whatever container ends up on top.
 *  
 *  @update  gess 3/25/98
 *  @param   aToken -- next (start) token to be handled
 *  @param   aNode -- CParserNode representing this start token
 *  @return  PR_TRUE if all went well; PR_FALSE if error occured
 */
PRInt32 nsHTMLParser::HandleStartToken(CToken* aToken) {
  NS_PRECONDITION(0!=aToken,kNullToken);

  CStartToken*  st= (CStartToken*)(aToken);
  eHTMLTags     tokenTagType=st->GetHTMLTag();

  //Begin by gathering up attributes...
  nsCParserNode attrNode((CHTMLToken*)aToken);
  PRInt16       attrCount=aToken->GetAttributeCount();
  PRInt32       result=(0==attrCount) ? kNoError : CollectAttributes(attrNode,attrCount);

  if(kNoError==result) {
      //now check to see if this token should be omitted...
    if(PR_FALSE==mDTD->CanOmit(GetTopNode(),tokenTagType)) {
  
      switch(tokenTagType) {

        case eHTMLTag_html:
          result=OpenHTML(attrNode); break;

        case eHTMLTag_title:
          {
            nsCParserNode theNode(st);
            result=OpenHead(theNode); //open the head...
            if(kNoError==result) {
              CollectSkippedContent(attrNode);
              mSink->SetTitle(attrNode.GetSkippedContent());
              result=CloseHead(theNode); //close the head...
            }
          }
          break;

        case eHTMLTag_textarea:
          {
            CollectSkippedContent(attrNode);
            result=AddLeaf(attrNode);
          }
          break;

        case eHTMLTag_form:
          result = OpenForm(attrNode);
          break;

        case eHTMLTag_meta:
        case eHTMLTag_link:
          {
            nsCParserNode theNode((CHTMLToken*)aToken);
            result=OpenHead(theNode);
            if(kNoError==result)
              result=AddLeaf(theNode);
            if(kNoError==result)
              result=CloseHead(theNode);
          }
          break;

        case eHTMLTag_style:
          {
            nsCParserNode theNode((CHTMLToken*)aToken);
            result=OpenHead(theNode);
            if(kNoError==result) {
              CollectSkippedContent(attrNode);
              if(kNoError==result) {
                result=AddLeaf(attrNode);
                if(kNoError==result)
                  result=CloseHead(theNode);
              }
            }
          }
          break;

        case eHTMLTag_script:
          result=HandleScriptToken(st); break;
      

        case eHTMLTag_head:
          break; //ignore head tags...

        case eHTMLTag_base:
          result=OpenHead(attrNode);
          if(kNoError==result) {
            result=AddLeaf(attrNode);
            if(kNoError==result)
              result=CloseHead(attrNode);
          }
          break;

        case eHTMLTag_nobr:
          result=PR_TRUE;

        case eHTMLTag_map:
          result=PR_TRUE;

        default:
          result=HandleDefaultStartToken(aToken,tokenTagType,attrNode);
          break;
      } //switch
    } //if
  } //if
  return result;
}

/**
 *  This method gets called when an end token has been 
 *  encountered in the parse process. If the end tag matches
 *  the start tag on the stack, then simply close it. Otherwise,
 *  we have a erroneous state condition. This can be because we
 *  have a close tag with no prior open tag (user error) or because
 *  we screwed something up in the parse process. I'm not sure
 *  yet how to tell the difference.
 *  
 *  @update  gess 3/25/98
 *  @param   aToken -- next (start) token to be handled
 *  @return  PR_TRUE if all went well; PR_FALSE if error occured
 */
PRInt32 nsHTMLParser::HandleEndToken(CToken* aToken) {
  NS_PRECONDITION(0!=aToken,kNullToken);

  PRInt32     result=kNoError;
  CEndToken*  et = (CEndToken*)(aToken);
  eHTMLTags   tokenTagType=et->GetHTMLTag();

    //now check to see if this token should be omitted...
  if(PR_TRUE==mDTD->CanOmitEndTag(GetTopNode(),tokenTagType)) {
    return result;
  }

  nsCParserNode theNode((CHTMLToken*)aToken);
  switch(tokenTagType) {

    case eHTMLTag_style:
    case eHTMLTag_link:
    case eHTMLTag_meta:
    case eHTMLTag_textarea:
    case eHTMLTag_title:
    case eHTMLTag_head:
    case eHTMLTag_script:
      break;

    case eHTMLTag_map:
      result=CloseContainer(theNode);
      break;

    case eHTMLTag_form:
      {
        nsCParserNode aNode((CHTMLToken*)aToken);
        result=CloseForm(aNode);
      }
      break;

    default:
      if(mDTD->IsContainer(tokenTagType)){
        result=CloseContainersTo(tokenTagType); 
      }
      //
      break;
  }
  return result;
}

/**
 *  This method gets called when an entity token has been 
 *  encountered in the parse process. 
 *  
 *  @update  gess 3/25/98
 *  @param   aToken -- next (start) token to be handled
 *  @return  PR_TRUE if all went well; PR_FALSE if error occured
 */
PRInt32 nsHTMLParser::HandleEntityToken(CToken* aToken) {
  NS_PRECONDITION(0!=aToken,kNullToken);

  CEntityToken* et = (CEntityToken*)(aToken);
  PRInt32       result=kNoError;
  eHTMLTags     tokenTagType=et->GetHTMLTag();

  if(PR_FALSE==mDTD->CanOmit(GetTopNode(),tokenTagType)) {
    nsCParserNode aNode((CHTMLToken*)aToken);
    result=AddLeaf(aNode);
  }
  return result;
}

/**
 *  This method gets called when a comment token has been 
 *  encountered in the parse process. After making sure
 *  we're somewhere in the body, we handle the comment
 *  in the same code that we use for text.
 *  
 *  @update  gess 3/25/98
 *  @param   aToken -- next (start) token to be handled
 *  @return  PR_TRUE if all went well; PR_FALSE if error occured
 */
PRInt32 nsHTMLParser::HandleCommentToken(CToken* aToken) {
  NS_PRECONDITION(0!=aToken,kNullToken);
  return kNoError;
}

/**
 *  This method gets called when a skippedcontent token has 
 *  been encountered in the parse process. After verifying 
 *  that the topmost container can contain text, we call 
 *  AddLeaf to store this token in the top container.
 *  
 *  @update  gess 3/25/98
 *  @param   aToken -- next (start) token to be handled
 *  @return  PR_TRUE if all went well; PR_FALSE if error occured
 */
PRInt32 nsHTMLParser::HandleSkippedContentToken(CToken* aToken) {
  NS_PRECONDITION(0!=aToken,kNullToken);

  PRInt32 result=kNoError;

  if(HasOpenContainer(eHTMLTag_body)) {
    nsCParserNode aNode((CHTMLToken*)aToken);
    result=AddLeaf(aNode);
  }
  return result;
}

/**
 *  This method gets called when an attribute token has been 
 *  encountered in the parse process. This is an error, since
 *  all attributes should have been accounted for in the prior
 *  start or end tokens
 *  
 *  @update  gess 3/25/98
 *  @param   aToken -- next (start) token to be handled
 *  @return  PR_TRUE if all went well; PR_FALSE if error occured
 */
PRInt32 nsHTMLParser::HandleAttributeToken(CToken* aToken) {
  NS_PRECONDITION(0!=aToken,kNullToken);
  NS_ERROR("attribute encountered -- this shouldn't happen!");

  CAttributeToken*  at = (CAttributeToken*)(aToken);
  PRInt32 result=kNoError;
  return result;
}

/**
 *  This method gets called when a script token has been 
 *  encountered in the parse process. 
 *  
 *  @update  gess 3/25/98
 *  @param   aToken -- next (start) token to be handled
 *  @return  PR_TRUE if all went well; PR_FALSE if error occured
 */
PRInt32 nsHTMLParser::HandleScriptToken(CToken* aToken) {
  NS_PRECONDITION(0!=aToken,kNullToken);

  CScriptToken*   st = (CScriptToken*)(aToken);

  eHTMLTokenTypes subtype=eToken_attribute;
  nsDeque&        deque=mTokenizer->GetDeque();
  nsDequeIterator end=deque.End();

  if(*mCurrentPos!=end) {
    CHTMLToken* tkn=(CHTMLToken*)(++(*mCurrentPos));
    subtype=eHTMLTokenTypes(tkn->GetTokenType());
    if(eToken_skippedcontent==subtype) {
      //WE INTENTIONALLY DROP THE TOKEN ON THE FLOOR!
      //LATER, we'll pass this onto the javascript system.
      return kNoError;
    } 
    else (*mCurrentPos)--;
  }
  return kInterrupted;

}

/**
 *  This method gets called when a style token has been 
 *  encountered in the parse process. 
 *  
 *  @update  gess 3/25/98
 *  @param   aToken -- next (start) token to be handled
 *  @return  PR_TRUE if all went well; PR_FALSE if error occured
 */
PRInt32 nsHTMLParser::HandleStyleToken(CToken* aToken){
  NS_PRECONDITION(0!=aToken,kNullToken);

  CStyleToken*  st = (CStyleToken*)(aToken);
  PRInt32 result=kNoError;
  return result;
}


/**
 * This method does two things: 1st, help construct
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 * 
 * @update  gess4/22/98
 * @param   aNode -- next node to be added to model
 * @return  TRUE if ok, FALSE if error
 */
PRInt32 nsHTMLParser::OpenHTML(const nsIParserNode& aNode){
  NS_PRECONDITION(mContextStackPos >= 0, kInvalidTagStackPos);

  PRInt32 result=mSink->OpenHTML(aNode); 
  mContextStack[mContextStackPos++]=(eHTMLTags)aNode.GetNodeType();
  return result;
}

/**
 * This method does two things: 1st, help construct
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 *
 * @update  gess4/6/98
 * @param   aNode -- next node to be removed from our model
 * @return  TRUE if ok, FALSE if error
 */
PRInt32 nsHTMLParser::CloseHTML(const nsIParserNode& aNode){
  NS_PRECONDITION(mContextStackPos > 0, kInvalidTagStackPos);
  PRInt32 result=mSink->CloseHTML(aNode); 
  mContextStack[--mContextStackPos]=eHTMLTag_unknown;
  return result;
}


/**
 * This method does two things: 1st, help construct
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 * @update  gess4/6/98
 * @param   aNode -- next node to be added to model
 * @return  TRUE if ok, FALSE if error
 */
PRInt32 nsHTMLParser::OpenHead(const nsIParserNode& aNode){
  mContextStack[mContextStackPos++]=eHTMLTag_head;
  PRInt32 result=mSink->OpenHead(aNode); 
  return result;
}

/**
 * This method does two things: 1st, help construct
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 * @update  gess4/6/98
 * @param   aNode -- next node to be removed from our model
 * @return  TRUE if ok, FALSE if error
 */
PRInt32 nsHTMLParser::CloseHead(const nsIParserNode& aNode){
  PRInt32 result=mSink->CloseHead(aNode); 
  mContextStack[--mContextStackPos]=eHTMLTag_unknown;
  return result;
}

/**
 * This method does two things: 1st, help construct
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 * @update  gess4/6/98
 * @param   aNode -- next node to be added to model
 * @return  TRUE if ok, FALSE if error
 */
PRInt32 nsHTMLParser::OpenBody(const nsIParserNode& aNode){
  NS_PRECONDITION(mContextStackPos >= 0, kInvalidTagStackPos);

  PRInt32 result=kNoError;
  eHTMLTags topTag=(eHTMLTags)nsHTMLParser::GetTopNode();

  if(eHTMLTag_html!=topTag) {
    
    //ok, there are two cases:
    //  1. Nobody opened the html container
    //  2. Someone left the head (or other) open
    PRInt32 pos=GetTopmostIndex(eHTMLTag_html);
    if(kNotFound!=pos) {
      //if you're here, it means html is open, 
      //but some other tag(s) are in the way.
      //So close other tag(s).
      result=CloseContainersTo(pos+1);
    } else {
      //if you're here, it means that there is
      //no HTML tag in document. Let's open it.

      result=CloseContainersTo(0);  //close current stack containers.

      nsAutoString  empty;
      CHTMLToken    token(empty);
      nsCParserNode htmlNode(&token);

      token.SetHTMLTag(eHTMLTag_html);  //open the html container...
      result=OpenHTML(htmlNode);
    }
  }

  if(kNoError==result) {
    result=mSink->OpenBody(aNode); 
    mContextStack[mContextStackPos++]=(eHTMLTags)aNode.GetNodeType();
  }
  return result;
}

/**
 * This method does two things: 1st, help close
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 * @update  gess4/6/98
 * @param   aNode -- next node to be removed from our model
 * @return  TRUE if ok, FALSE if error
 */
PRInt32 nsHTMLParser::CloseBody(const nsIParserNode& aNode){
  NS_PRECONDITION(mContextStackPos >= 0, kInvalidTagStackPos);
  PRInt32 result=mSink->CloseBody(aNode); 
  mContextStack[--mContextStackPos]=eHTMLTag_unknown;
  return result;
}

/**
 * This method does two things: 1st, help construct
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 * @update  gess4/6/98
 * @param   aNode -- next node to be added to model
 * @return  TRUE if ok, FALSE if error
 */
PRInt32 nsHTMLParser::OpenForm(const nsIParserNode& aNode){
  if(mHasOpenForm)
    CloseForm(aNode);
  PRInt32 result=mSink->OpenForm(aNode);
  if(kNoError==result)
    mHasOpenForm=PR_TRUE;
  return result;
}

/**
 * This method does two things: 1st, help construct
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 * @update  gess4/6/98
 * @param   aNode -- next node to be removed from our model
 * @return  TRUE if ok, FALSE if error
 */
PRInt32 nsHTMLParser::CloseForm(const nsIParserNode& aNode){
  PRInt32 result=kNoError;
  if(mHasOpenForm) {
    mHasOpenForm=PR_FALSE;
    result=mSink->CloseForm(aNode); 
  }
  return result;
}

/**
 * This method does two things: 1st, help construct
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 * @update  gess4/6/98
 * @param   aNode -- next node to be added to model
 * @return  TRUE if ok, FALSE if error
 */
PRInt32 nsHTMLParser::OpenFrameset(const nsIParserNode& aNode){
  NS_PRECONDITION(mContextStackPos >= 0, kInvalidTagStackPos);
  PRInt32 result=mSink->OpenFrameset(aNode); 
  mContextStack[mContextStackPos++]=(eHTMLTags)aNode.GetNodeType();
  return result;
}

/**
 * This method does two things: 1st, help construct
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 * @update  gess4/6/98
 * @param   aNode -- next node to be removed from our model
 * @return  TRUE if ok, FALSE if error
 */
PRInt32 nsHTMLParser::CloseFrameset(const nsIParserNode& aNode){
  NS_PRECONDITION(mContextStackPos > 0, kInvalidTagStackPos);
  PRInt32 result=mSink->CloseFrameset(aNode); 
  mContextStack[--mContextStackPos]=eHTMLTag_unknown;
  return result;
}

/**
 * This method does two things: 1st, help construct
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 * @update  gess4/6/98
 * @param   aNode -- next node to be added to model
 * @return  TRUE if ok, FALSE if error
 */
PRInt32 nsHTMLParser::OpenContainer(const nsIParserNode& aNode){
  NS_PRECONDITION(mContextStackPos > 0, kInvalidTagStackPos);
  PRInt32 result=kNoError; //was false
  
  //XXX Hack! We know this is wrong, but it works
  //for the general case until we get it right.
  switch(aNode.GetNodeType()) {

    case eHTMLTag_html:
      result=OpenHTML(aNode); break;

    case eHTMLTag_body:
      result=OpenBody(aNode); break;

    case eHTMLTag_style:
    case eHTMLTag_textarea:
    case eHTMLTag_head:
    case eHTMLTag_title:
      break;

    case eHTMLTag_form:
      result=OpenForm(aNode); break;

    default:
      result=mSink->OpenContainer(aNode); 
      mContextStack[mContextStackPos++]=(eHTMLTags)aNode.GetNodeType();
      break;
  }
  return result;
}

/**
 * This method does two things: 1st, help construct
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 * @update  gess4/6/98
 * @param   aNode -- next node to be removed from our model
 * @return  TRUE if ok, FALSE if error
 */
PRInt32 nsHTMLParser::CloseContainer(const nsIParserNode& aNode){
  NS_PRECONDITION(mContextStackPos > 0, kInvalidTagStackPos);
  PRInt32 result=kNoError; //was false
  
  //XXX Hack! We know this is wrong, but it works
  //for the general case until we get it right.
  switch(aNode.GetNodeType()) {

    case eHTMLTag_html:
      result=CloseHTML(aNode); break;

    case eHTMLTag_style:
    case eHTMLTag_textarea:
      break;

    case eHTMLTag_head:
      //result=CloseHead(aNode); 
      break;

    case eHTMLTag_body:
      result=CloseBody(aNode); break;

    case eHTMLTag_form:
      result=CloseForm(aNode); break;

    case eHTMLTag_title:
    default:
      result=mSink->CloseContainer(aNode); 
      mContextStack[--mContextStackPos]=eHTMLTag_unknown;
      break;
  }
  return result;
}

/**
 * This method does two things: 1st, help construct
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 * @update  gess4/6/98
 * @param   
 * @return  TRUE if ok, FALSE if error
 */
PRInt32 nsHTMLParser::CloseContainersTo(PRInt32 anIndex){
  NS_PRECONDITION(mContextStackPos > 0, kInvalidTagStackPos);
  PRInt32 result=kNoError;

  nsAutoString empty;
  CEndToken aToken(empty);
  nsCParserNode theNode(&aToken);

  if((anIndex<mContextStackPos) && (anIndex>=0)) {
    while(mContextStackPos>anIndex) {
      aToken.SetHTMLTag((eHTMLTags)mContextStack[mContextStackPos-1]);
      result=CloseContainer(theNode);
    }
  }
  return result;
}

/**
 * This method does two things: 1st, help construct
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 * @update  gess4/6/98
 * @param   
 * @return  TRUE if ok, FALSE if error
 */
PRInt32 nsHTMLParser::CloseContainersTo(eHTMLTags aTag){
  NS_PRECONDITION(mContextStackPos > 0, kInvalidTagStackPos);

  PRInt32 pos=GetTopmostIndex(aTag);
  PRInt32 result=kNoError;

  if(kNotFound!=pos) {
    //the tag is indeed open, so close it.
    result=CloseContainersTo(pos);
  }
  else {
    eHTMLTags theParentTag=(eHTMLTags)mDTD->GetDefaultParentTagFor(aTag);
    pos=GetTopmostIndex(theParentTag);
    if(kNotFound!=pos) {
      //the parent container is open, so close it instead
      result=CloseContainersTo(pos+1);
    }
    else {
      //XXX HACK! This is a real problem -- the unhandled case.!
      result=kUnknownError;
    }
  }
  return result;
}

/**
 * This method causes the topmost container on the stack
 * to be closed. 
 * @update  gess4/6/98
 * @see     CloseContainer()
 * @param   
 * @return  TRUE if ok, FALSE if error
 */
PRInt32 nsHTMLParser::CloseTopmostContainer(){
  NS_PRECONDITION(mContextStackPos > 0, kInvalidTagStackPos);

  nsAutoString empty;
  CEndToken aToken(empty);
  aToken.SetHTMLTag((eHTMLTags)mContextStack[mContextStackPos-1]);
  nsCParserNode theNode(&aToken);
  PRInt32 result=CloseContainer(theNode);
  return result;
}

/**
 * This method does two things: 1st, help construct
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 * @update  gess4/6/98
 * @param   aNode -- next node to be added to model
 * @return  TRUE if ok, FALSE if error
 */
PRInt32 nsHTMLParser::AddLeaf(const nsIParserNode& aNode){
  PRInt32 result=mSink->AddLeaf(aNode); 
  return result;
}

/**
 *  This method gets called to create a valid context stack
 *  for the given child. We compare the current stack to the
 *  default needs of the child, and push new guys onto the
 *  stack until the child can be properly placed.
 *
 *  @update  gess 4/8/98
 *  @param   aChildTag is the child for whom we need to 
 *           create a new context vector
 *  @return  true if we succeeded, otherwise false
 */
PRInt32 nsHTMLParser::CreateContextStackFor(PRInt32 aChildTag){
  nsAutoString  theVector;
  
  PRInt32 result=kNoError;
  PRInt32 pos=0;
  PRInt32 cnt=0;
  PRInt32 theTop=GetTopNode();
  
  if(PR_TRUE==mDTD->ForwardPropagate(theVector,theTop,aChildTag)){
    //add code here to build up context stack based on forward propagated context vector...
    pos=0;
    cnt=theVector.Length()-1;
    if(mContextStack[mContextStackPos-1]==theVector[cnt])
      result=kNoError;
    else result=kContextMismatch;
  }
  else {
    PRBool tempResult;
    if(eHTMLTag_unknown!=theTop) {
      tempResult=mDTD->BackwardPropagate(theVector,theTop,aChildTag);
      if(eHTMLTag_html!=theTop)
        mDTD->BackwardPropagate(theVector,eHTMLTag_html,theTop);
    }
    else tempResult=mDTD->BackwardPropagate(theVector,eHTMLTag_html,aChildTag);

    if(PR_TRUE==tempResult) {

      //propagation worked, so pop unwanted containers, push new ones, then exit...
      pos=0;
      cnt=theVector.Length();
      result=kNoError;
      while(pos<mContextStackPos) {
        if(mContextStack[pos]==theVector[cnt-1-pos]) {
          pos++;
        }
        else {
          //if you're here, you have something on the stack
          //that doesn't match your needed tags order.
          result=CloseContainersTo(pos);
          break;
        }
      } //while
    } //elseif
    else result=kCantPropagate;
  } //elseif

    //now, build up the stack according to the tags 
    //you have that aren't in the stack...
  if(kNoError==result){
    nsAutoString  empty;
    int i=0;
    for(i=pos;i<cnt;i++) {
      CStartToken* st=new CStartToken((eHTMLTags)theVector[cnt-1-i]);
      HandleStartToken(st);
    }
  }
  return result;
}


/**
 *  This method gets called to ensure that the context
 *  stack is properly set up for the given child. 
 *  We pop containers off the stack (all the way down 
 *  html) until we get a container that can contain
 *  the given child.
 *  
 *  @update  gess 4/8/98
 *  @param   
 *  @return  
 */
PRInt32 nsHTMLParser::ReduceContextStackFor(PRInt32 aChildTag){
  PRInt32    result=kNoError;
  eHTMLTags topTag=(eHTMLTags)nsHTMLParser::GetTopNode();

  while( (topTag!=kNotFound) && 
         (PR_FALSE==mDTD->CanContain(topTag,aChildTag)) &&
         (PR_FALSE==mDTD->CanContainIndirect(topTag,aChildTag))) {
    CloseTopmostContainer();
    topTag=(eHTMLTags)nsHTMLParser::GetTopNode();
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

        //Ok -- here's the problem.
        //Just because someone throws you some data, doesn't mean that it's
        //actually GOOD data. Recently, I encountered a problem where netlib
        //was prepending an otherwise valid buffer with a few garbage characters.
        //To solve this, I'm adding some debug code here that protects us from
        //propagating the bad data upwards.

        mTransferBuffer[len]=0;
        if(PR_FALSE==mHasSeenOpenTag) {
          for(offset=0;offset<len;offset++) {
            if(kLessThan==mTransferBuffer[offset]){
              mHasSeenOpenTag=PR_TRUE;
              break;
            } 
          }
        }

        if(len-offset)
          mTokenizer->Append(&mTransferBuffer[offset], len - offset);
      }
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

