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
  


#include "nsParser.h"
#include "nsIContentSink.h" 
#include "nsString.h"
#include "nsCRT.h" 
#include "nsScanner.h"
#include "prenv.h"  //this is here for debug reasons...
#include "plstr.h"
#include <fstream.h>
#include "nsIParserFilter.h"
#include "nshtmlpars.h"
#include "CNavDTD.h"
#include "nsWellFormedDTD.h"
#include "nsViewSourceHTML.h" //uncomment this to partially enable viewsource...

#undef rickgdebug
#ifdef  rickgdebug
#include "CRtfDTD.h"
#endif

 
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);                 
static NS_DEFINE_IID(kClassIID, NS_PARSER_IID); 
static NS_DEFINE_IID(kIParserIID, NS_IPARSER_IID);
static NS_DEFINE_IID(kIStreamListenerIID, NS_ISTREAMLISTENER_IID);

static const char* kNullURL = "Error: Null URL given";
static const char* kOnStartNotCalled = "Error: OnStartBinding() must be called before OnDataAvailable()";
static const char* kBadListenerInit  = "Error: Parser's IStreamListener API was not setup correctly in constructor.";

 
class CTokenDeallocator: public nsDequeFunctor{
public:
  virtual void* operator()(void* anObject) {
    CToken* aToken = (CToken*)anObject;
    delete aToken;
    return 0;
  }
};


class CDTDDeallocator: public nsDequeFunctor{
public:
  virtual void* operator()(void* anObject) {
    nsIDTD* aDTD =(nsIDTD*)anObject;
    NS_RELEASE(aDTD);
    return 0;
  }
};

class CDTDFinder: public nsDequeFunctor{
public:
  CDTDFinder(nsIDTD* aDTD) {
    mTargetDTD=aDTD;
  }
  virtual ~CDTDFinder() {
  }
  virtual void* operator()(void* anObject) {
    nsIDTD* theDTD=(nsIDTD*)anObject;
    if(theDTD->GetMostDerivedIID().Equals(mTargetDTD->GetMostDerivedIID()))
      return anObject;
    return 0;
  }
  nsIDTD* mTargetDTD;
};

class CSharedParserObjects {
public:

  CSharedParserObjects() : mDTDDeque(new CDTDDeallocator()) {

    nsIDTD* theDTD;

    NS_NewWellFormed_DTD(&theDTD);
    mDTDDeque.Push(theDTD);

    NS_NewNavHTMLDTD(&theDTD);    //do this as the default HTML DTD...
    mDTDDeque.Push(theDTD);

    NS_NewViewSourceHTML(&theDTD);  //do this so all html files can be viewed...
    mDTDDeque.Push(theDTD);
  }

  ~CSharedParserObjects() {
  }

  void RegisterDTD(nsIDTD* aDTD){
    if(aDTD) {
      NS_ADDREF(aDTD);
      CDTDFinder theFinder(aDTD);
      if(!mDTDDeque.FirstThat(theFinder)) {
        nsIDTD* theDTD;
        aDTD->CreateNewInstance(&theDTD);
        mDTDDeque.Push(theDTD);
      }
      NS_RELEASE(aDTD);
    }
  }
  
  nsIDTD*  FindDTD(nsIDTD* aDTD){
    return 0;
  }

  nsDeque mDTDDeque;
};

//static CSharedParserObjects* gSharedParserObjects=0;

nsString nsParser::gHackMetaCharset = "";
nsString nsParser::gHackMetaCharsetURL = "";

//----------------------------------------

CSharedParserObjects& GetSharedObjects() {
  static CSharedParserObjects gSharedParserObjects;
  return gSharedParserObjects;
}

/** 
 *  default constructor
 *   
 *  @update  gess 01/04/99
 *  @param   
 *  @return   
 */
nsParser::nsParser(nsITokenObserver* anObserver) : mCommand(""), mUnusedInput("") {
  NS_INIT_REFCNT();
  mParserFilter = 0;
  mObserver = 0;
  mSink=0;
  mParserContext=0;
  mTokenObserver=anObserver;
  mStreamStatus=0;
  mDTDVerification=PR_FALSE;
}

 
/**
 *  Default destructor
 *  
 *  @update  gess 01/04/99
 *  @param   
 *  @return  
 */
nsParser::~nsParser() {
  NS_IF_RELEASE(mObserver);
  NS_IF_RELEASE(mSink);

  //don't forget to add code here to delete 
  //what may be several contexts...
  delete mParserContext;
}


NS_IMPL_ADDREF(nsParser)
NS_IMPL_RELEASE(nsParser)
//NS_IMPL_ISUPPORTS(nsParser,NS_IHTML_HTMLPARSER_IID)


/**
 *  This method gets called as part of our COM-like interfaces.
 *  Its purpose is to create an interface to parser object
 *  of some type.
 *  
 *  @update   gess 01/04/99
 *  @param    nsIID  id of object to discover
 *  @param    aInstancePtr ptr to newly discovered interface
 *  @return   NS_xxx result code
 */
nsresult nsParser::QueryInterface(const nsIID& aIID, void** aInstancePtr)  
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
  else if(aIID.Equals(kIStreamListenerIID)) {  //do IStreamListener base class...
    *aInstancePtr = (nsIStreamListener*)(this);                                        
  }
  else if(aIID.Equals(kClassIID)) {  //do this class...
    *aInstancePtr = (nsParser*)(this);                                        
  }                 
  else {
    *aInstancePtr=0;
    return NS_NOINTERFACE;
  }
  NS_ADDREF_THIS();
  return NS_OK;                                                        
}


/**
 * 
 * @update	gess 01/04/99
 * @param 
 * @return
 */
nsIParserFilter * nsParser::SetParserFilter(nsIParserFilter * aFilter)
{
  nsIParserFilter* old=mParserFilter;
  if(old)
    NS_RELEASE(old);
  if(aFilter) {
    mParserFilter=aFilter;
    NS_ADDREF(aFilter);
  }
  return old;
}


/**
 *  Call this method once you've created a parser, and want to instruct it
 *  about the command which caused the parser to be constructed. For example,
 *  this allows us to select a DTD which can do, say, view-source.
 *  
 *  @update  gess 01/04/99
 *  @param   aContentSink -- ptr to content sink that will receive output
 *  @return	 ptr to previously set contentsink (usually null)  
 */
void nsParser::SetCommand(const char* aCommand){
  mCommand=aCommand;
}

/**
 *  This method gets called in order to set the content
 *  sink for this parser to dump nodes to.
 *  
 *  @update  gess 01/04/99
 *  @param   nsIContentSink interface for node receiver
 *  @return  
 */
nsIContentSink* nsParser::SetContentSink(nsIContentSink* aSink) {
  NS_PRECONDITION(0!=aSink,"sink cannot be null!");
  nsIContentSink* old=mSink;

  NS_IF_RELEASE(old);
  if(aSink) {
    mSink=aSink;
    NS_ADDREF(aSink);
    mSink->SetParser(this);
  }
  return old;
}

/**
 * retrive the sink set into the parser 
 * @update	gess5/11/98
 * @param   aSink is the new sink to be used by parser
 * @return  old sink, or NULL
 */
nsIContentSink* nsParser::GetContentSink(void){
  return mSink;
}

/**
 *  Call this method when you want to
 *  register your dynamic DTD's with the parser.
 *  
 *  @update  gess 01/04/99
 *  @param   aDTD  is the object to be registered.
 *  @return  nothing.
 */
void nsParser::RegisterDTD(nsIDTD* aDTD){
  CSharedParserObjects& theShare=GetSharedObjects();
  theShare.RegisterDTD(aDTD);
}

/**
 *  Retrieve scanner from topmost parsecontext
 *  
 *  @update  gess 01/04/99
 *  @return  ptr to internal scanner
 */
nsScanner* nsParser::GetScanner(void){
  if(mParserContext)
    return mParserContext->mScanner;
  return 0;
}


/**
 *  Retrieve parsemode from topmost parser context
 *  
 *  @update  gess 01/04/99
 *  @return  parsemode
 */
eParseMode nsParser::GetParseMode(void){
  if(mParserContext)
    return mParserContext->mParseMode;
  return eParseMode_unknown;
}



/**
 *  
 *  
 *  @update  gess 5/13/98
 *  @param   
 *  @return  
 */
static
PRBool FindSuitableDTD( CParserContext& aParserContext,nsString& aCommand,nsString& aBuffer) {

    //Let's start by tring the defaultDTD, if one exists...
  if(aParserContext.mDTD)
    if(aParserContext.mDTD->CanParse(aParserContext.mSourceType,aCommand,aBuffer,0))
      return PR_TRUE;

  CSharedParserObjects& gSharedObjects=GetSharedObjects();
  nsDequeIterator b=gSharedObjects.mDTDDeque.Begin(); 
  nsDequeIterator e=gSharedObjects.mDTDDeque.End(); 

  aParserContext.mAutoDetectStatus=eUnknownDetect;
  nsIDTD* theBestDTD=0;
  while((b<e) && (aParserContext.mAutoDetectStatus!=ePrimaryDetect)){
    nsIDTD* theDTD=(nsIDTD*)b.GetCurrent();
    if(theDTD) {
      aParserContext.mAutoDetectStatus=theDTD->CanParse(aParserContext.mSourceType,aCommand,aBuffer,0);
      if((eValidDetect==aParserContext.mAutoDetectStatus) || (ePrimaryDetect==aParserContext.mAutoDetectStatus)) {
        theBestDTD=theDTD;
      }
    }
    b++;
  } 

  if(theBestDTD) {
    theBestDTD->CreateNewInstance(&aParserContext.mDTD);
    return PR_TRUE;
  }
  return PR_FALSE;
}


/**
 *  This is called when it's time to find out 
 *  what mode the parser/DTD should run for this document.
 *  (Each parsercontext can have it's own mode).
 *  
 *  @update  gess 5/13/98
 *  @return  parsermode (define in nsIParser.h)
 */
static
eParseMode DetermineParseMode(nsParser& aParser) {
  const char* theModeStr= PR_GetEnv("PARSE_MODE");
  const char* other="other";
  
  nsScanner* theScanner=aParser.GetScanner();
  if(theScanner){
    nsString& theBuffer=theScanner->GetBuffer();
    PRInt32 theIndex=theBuffer.Find("HTML 4.0");
    if(kNotFound==theIndex)
      theIndex=theBuffer.Find("html 4.0");
    if(kNotFound<theIndex)
      return eParseMode_raptor;
    else {
      PRInt32 theIndex=theBuffer.Find("noquirks");
      if(kNotFound==theIndex)
        theIndex=theBuffer.Find("NOQUIRKS");
      if(kNotFound<theIndex)
        return eParseMode_noquirks;
    }
  }

  if(theModeStr) 
    if(0==nsCRT::strcasecmp(other,theModeStr))
      return eParseMode_other;    
  return eParseMode_navigator;
}


/**
 * This gets called just prior to the model actually
 * being constructed. It's important to make this the
 * last thing that happens right before parsing, so we
 * can delay until the last moment the resolution of
 * which DTD to use (unless of course we're assigned one).
 *
 * @update	gess5/18/98
 * @param 
 * @return  error code -- 0 if ok, non-zero if error.
 */
nsresult nsParser::WillBuildModel(nsString& aFilename,nsIDTD* aDefaultDTD){

  nsresult result=NS_OK;

  if(mParserContext){
    if(eOnStart==mParserContext->mStreamListenerState) {  
      mMajorIteration=-1; 
      mMinorIteration=-1; 
      if(eUnknownDetect==mParserContext->mAutoDetectStatus) {
        mParserContext->mDTD=aDefaultDTD;
        if(PR_TRUE==FindSuitableDTD(*mParserContext,mCommand,mParserContext->mScanner->GetBuffer())) {
          mParserContext->mParseMode=DetermineParseMode(*this);  
          mParserContext->mStreamListenerState=eOnDataAvail;
          mParserContext->mDTD->WillBuildModel( aFilename,
                                                PRBool(0==mParserContext->mPrevContext),
                                                mParserContext->mSourceType,
                                                mSink);
        }//if        
      }//if
    }//if
  } 
  else result=kInvalidParserContext;    
  return result;
}

/**
 * This gets called when the parser is done with its input.
 * Note that the parser may have been called recursively, so we
 * have to check for a prev. context before closing out the DTD/sink.
 * @update	gess5/18/98
 * @param 
 * @return  error code -- 0 if ok, non-zero if error.
 */
nsresult nsParser::DidBuildModel(nsresult anErrorCode) {
  //One last thing...close any open containers.
  nsresult result=anErrorCode;

  if(mParserContext->mParserEnabled) {
    if((!mParserContext->mPrevContext) && (mParserContext->mDTD)) {
      result=mParserContext->mDTD->DidBuildModel(anErrorCode,PRBool(0==mParserContext->mPrevContext),this,mSink);
    }
  }//if

  return result;
}


/**
 * This method adds a new parser context to the list,
 * pushing the current one to the next position.
 * @update	gess7/22/98
 * @param   ptr to new context
 * @return  nada
 */
void nsParser::PushContext(CParserContext& aContext) {
  aContext.mPrevContext=mParserContext;  
  mParserContext=&aContext;
}

/**
 * This method pops the topmost context off the stack,
 * returning it to the user. The next context  (if any)
 * becomes the current context.
 * @update	gess7/22/98
 * @return  prev. context
 */
CParserContext* nsParser::PopContext() {
  CParserContext* oldContext=mParserContext;
  if(oldContext) {
    mParserContext=oldContext->mPrevContext;
  }
  return oldContext;
}

/**
 *  Call this when you want control whether or not the parser will parse
 *  and tokenize input (TRUE), or whether it just caches input to be 
 *  parsed later (FALSE).
 *  
 *  @update  gess 1/29/99
 *  @param   aState determines whether we parse/tokenize or just cache.
 *  @return  current state
 */
void nsParser::SetUnusedInput(nsString& aBuffer) {
  mUnusedInput=aBuffer;
}


/**
 *  Call this when you want control whether or not the parser will parse
 *  and tokenize input (TRUE), or whether it just caches input to be 
 *  parsed later (FALSE).
 *  
 *  @update  gess 1/29/99
 *  @param   aState determines whether we parse/tokenize or just cache.
 *  @return  current state
 */
PRBool nsParser::EnableParser(PRBool aState){
  nsIParser* me = nsnull;

  // If the stream has already finished, there's a good chance
  // that we might start closing things down when the parser
  // is reenabled. To make sure that we're not deleted across
  // the reenabling process, hold a reference to ourselves.
  if (eOnStop == mParserContext->mStreamListenerState) {
    me = this;
    NS_ADDREF(me);
  }    

  // If we're reenabling the parser
  mParserContext->mParserEnabled=aState;
  nsresult result=(aState) ? ResumeParse() : NS_OK;

  // Release reference if we added one at the top of this routine
  NS_IF_RELEASE(me);

  return aState;
}


/**
 *  This is the main controlling routine in the parsing process. 
 *  Note that it may get called multiple times for the same scanner, 
 *  since this is a pushed based system, and all the tokens may 
 *  not have been consumed by the scanner during a given invocation 
 *  of this method. 
 *
 *  @update  gess 01/04/99
 *  @param   aFilename -- const char* containing file to be parsed.
 *  @return  error code -- 0 if ok, non-zero if error.
 */
nsresult nsParser::Parse(nsIURL* aURL,nsIStreamObserver* aListener,PRBool aVerifyEnabled) {
  NS_PRECONDITION(0!=aURL,kNullURL);

  nsAutoString charset;
  nsCharsetSource charsetSource;

  // XXXX get HTTP charset here
  // charset =
  // charsetSource = kCharsetFromHTTPHeader;

  // XXXX get User Prefernce charset here
  // charset =
  // charsetSource = kCharsetFromUserDefault;

  // XXXX get Doc Type Default (e.g. UTF-8 for XML)

  // XXX We should really put if doc == html for the following line
  charset = "ISO-8859-1";
  charsetSource = kCharsetFromDocTypeDefault;



  nsresult result=kBadURL;
  mDTDVerification=aVerifyEnabled;
  if(aURL) {
    const char* spec;
    nsresult rv = aURL->GetSpec(&spec);
    if (rv != NS_OK) return rv;
    nsAutoString theName(spec);

    // XXX temp hack to make parser use UTF-8 as default charset for XML, RDF, XUL
    // XXX This should be removed once we have the SetDefaultCharset in the nsIParser interface
	nsString last4;
	theName.Right(last4, 4);
	if(last4.EqualsIgnoreCase(".xul") || last4.EqualsIgnoreCase(".xml") || last4.EqualsIgnoreCase(".rdf") ) 
	{
		charset = "UTF-8";
	}

    // XXX begin of meta tag charset hack

	if(theName.Equals(nsParser::gHackMetaCharsetURL) && (! nsParser::gHackMetaCharset.Equals("")))
	{
		charset = nsParser::gHackMetaCharset;
		charsetSource = kCharsetFromMetaTag;
	}
	nsParser::gHackMetaCharsetURL = theName;
	nsParser::gHackMetaCharset = "";
    // XXX end of meta tag charset hack

    CParserContext* pc=new CParserContext(new nsScanner(theName,PR_FALSE, charset, charsetSource),aURL,aListener);
    if(pc) {
      pc->mMultipart=PR_TRUE;
      pc->mContextType=CParserContext::eCTURL;
      PushContext(*pc);
      result=NS_OK;
    }
  }
  return result;
}


/**
 * Cause parser to parse input from given stream 
 * @update	vidur 12/11/98
 * @param   aStream is the i/o source
 * @return  error code -- 0 if ok, non-zero if error.
 */
nsresult nsParser::Parse(nsIInputStream& aStream,PRBool aVerifyEnabled){

  mDTDVerification=aVerifyEnabled;
  nsresult  result=NS_ERROR_OUT_OF_MEMORY;

  nsAutoString charset;
  nsCharsetSource charsetSource;

  // XXXX get HTTP charset here
  // charset =
  // charsetSource = kCharsetFromHTTPHeader;

  // XXXX get User Prefernce charset here
  // charset =
  // charsetSource = kCharsetFromUserDefault;

  // XXXX get Doc Type Default (e.g. UTF-8 for XML)

  // XXX We should really put if doc == html for the following line
  charset = "ISO-8859-1";
  charsetSource = kCharsetFromDocTypeDefault;
  
  //ok, time to create our tokenizer and begin the process
  nsAutoString theUnknownFilename("unknown");

  // XXX begin of meta tag charset hack
  if(theUnknownFilename.Equals(nsParser::gHackMetaCharsetURL) && (! nsParser::gHackMetaCharset.Equals("")))
  {
		charset = nsParser::gHackMetaCharset;
		charsetSource = kCharsetFromMetaTag;
  }
  nsParser::gHackMetaCharsetURL = theUnknownFilename;
  nsParser::gHackMetaCharset = "";
  // XXX end of meta tag charset hack

	nsInputStream input(&aStream);
  CParserContext* pc=new CParserContext(new nsScanner(theUnknownFilename, input, charset, charsetSource,PR_FALSE),&aStream,0);
  if(pc) {
    PushContext(*pc);
    pc->mSourceType=kHTMLTextContentType;
    pc->mStreamListenerState=eOnStart;  
    pc->mMultipart=PR_FALSE;
    pc->mContextType=CParserContext::eCTStream;
    mParserContext->mScanner->Eof();
    result=ResumeParse();
    pc=PopContext();
    delete pc;
  }
  return result;
}


/**
 * Call this method if all you want to do is parse 1 string full of HTML text.
 * In particular, this method should be called by the DOM when it has an HTML
 * string to feed to the parser in real-time.
 *
 * @update	gess5/11/98
 * @param   aSourceBuffer contains a string-full of real content
 * @param   aContentType tells us what type of content to expect in the given string
 * @return  error code -- 0 if ok, non-zero if error.
 */
nsresult nsParser::Parse(nsString& aSourceBuffer,void* aKey,const nsString& aContentType,PRBool aEnableVerify,PRBool aLastCall){
 
#ifdef _rickgdebug
  {
    fstream out("c:/temp/parseout.file",ios::trunc);
    aSourceBuffer.DebugDump(out);
  }
#endif

  nsAutoString charset;
  nsCharsetSource charsetSource;

  // XXXX get HTTP charset here
  // charset =
  // charsetSource = kCharsetFromHTTPHeader;

  // XXXX get User Prefernce charset here
  // charset =
  // charsetSource = kCharsetFromUserDefault;

  // XXX temp hack to make parser use UTF-8 as default charset for XML, RDF, XUL
  // XXX This should be removed once we have the SetDefaultCharset in the nsIParser interface
  if(aContentType.EqualsIgnoreCase("text/xul") || aContentType.EqualsIgnoreCase("text/xml") || aContentType.EqualsIgnoreCase("text/rdf") ) 
  {
	charset = "UTF-8";
  } else {
    charset = "ISO-8859-1";
  }
   charsetSource = kCharsetFromDocTypeDefault;

  // XXX begin of meta tag charset hack
  nsAutoString theFakeURL("fromString");
  if(theFakeURL.Equals(nsParser::gHackMetaCharsetURL) && (! nsParser::gHackMetaCharset.Equals("")))
  {
		charset = nsParser::gHackMetaCharset;
		charsetSource = kCharsetFromMetaTag;
  }
  nsParser::gHackMetaCharsetURL = theFakeURL;
  nsParser::gHackMetaCharset = "";
  // XXX end of meta tag charset hack

  //NOTE: Make sure that updates to this method don't cause 
  //      bug #2361 to break again!

  nsresult result=NS_OK;
  nsParser* me = this;
  // Maintain a reference to ourselves so we don't go away
  // till we're completely done.
  NS_ADDREF(me);
  if(aSourceBuffer.Length() || mUnusedInput.Length()) {
    mDTDVerification=aEnableVerify;
    CParserContext* pc=0; 

    if((!mParserContext) || (mParserContext->mKey!=aKey))  {
      //only make a new context if we dont have one, OR if we do, but has a different context key...
      pc=new CParserContext(new nsScanner(mUnusedInput, charset, charsetSource),aKey, 0);
      if(pc) {
        PushContext(*pc);
        pc->mStreamListenerState=eOnStart;  
        pc->mContextType=CParserContext::eCTString;
        pc->mSourceType=aContentType; 
        mUnusedInput.Truncate(0);
      } 
      else {
        NS_RELEASE(me);
        return NS_ERROR_OUT_OF_MEMORY;
      }
    }
    else {
      pc=mParserContext;
      pc->mScanner->Append(mUnusedInput);
    }
    pc->mScanner->Append(aSourceBuffer);
    pc->mMultipart=!aLastCall;
    result=ResumeParse();
    if(aLastCall) {
      pc->mScanner->CopyUnusedData(mUnusedInput);
      pc=PopContext(); 
      delete pc;
    }//if
  }//if
  NS_RELEASE(me);
  return result;
}

/**
 *
 *  @update  gess 04/01/99
 *  @param   
 *  @return  
 */
PRBool nsParser::IsValidFragment(nsString& aSourceBuffer,nsTagStack& aStack,nsHTMLTag aTag,const nsString& aContentType){
  PRBool result=PR_FALSE;
  return result;
}

/**
 *
 *  @update  gess 04/01/99
 *  @param   
 *  @return  
 */
PRBool nsParser::ParseFragment(nsString& aSourceBuffer,void* aKey,nsTagStack& aStack,nsHTMLTag aTag,const nsString& aContentType){
  PRBool result=PR_FALSE;
  return result;
}
 
/**
 *  This routine is called to cause the parser to continue
 *  parsing it's underlying stream. This call allows the
 *  parse process to happen in chunks, such as when the
 *  content is push based, and we need to parse in pieces.
 *  
 *  @update  gess 01/04/99
 *  @param   
 *  @return  error code -- 0 if ok, non-zero if error.
 */
nsresult nsParser::ResumeParse(nsIDTD* aDefaultDTD) {
  
  nsresult result=NS_OK;
  if(mParserContext->mParserEnabled) {
    result=WillBuildModel(mParserContext->mScanner->GetFilename(),aDefaultDTD);
    if(mParserContext->mDTD) {
      mParserContext->mDTD->WillResumeParse();
      if(NS_OK==result) {
     
        result=Tokenize();
        if(eOnStop==mParserContext->mStreamListenerState){
          nsITokenizer* theTokenizer=mParserContext->mDTD->GetTokenizer();
          mParserContext->mDTD->EmitMisplacedContent(theTokenizer);
        }
        result=BuildModel();

        if((!mParserContext->mMultipart) || ((eOnStop==mParserContext->mStreamListenerState) && (NS_OK==result))){
          DidBuildModel(mStreamStatus);
        }
        else {
          mParserContext->mDTD->WillInterruptParse();
        // If we're told to block the parser, we disable
        // all further parsing (and cache any data coming
        // in) until the parser is enabled.
          PRUint32 b1=NS_ERROR_HTMLPARSER_BLOCK;
          if(NS_ERROR_HTMLPARSER_BLOCK==result) {
            EnableParser(PR_FALSE);
            result=NS_OK;
          }
        }//if
      }//if
    }//if
    else result=NS_ERROR_HTMLPARSER_UNRESOLVEDDTD;
  }//if
  return result;
}

/**
 *  This is where we loop over the tokens created in the 
 *  tokenization phase, and try to make sense out of them. 
 *
 *  @update  gess 01/04/99
 *  @param   
 *  @return  error code -- 0 if ok, non-zero if error.
 */
nsresult nsParser::BuildModel() {
  
  //nsDequeIterator e=mParserContext->mTokenDeque.End(); 

//  if(!mParserContext->mCurrentPos)
//    mParserContext->mCurrentPos=new nsDequeIterator(mParserContext->mTokenDeque.Begin());

    //Get the root DTD for use in model building...
  CParserContext* theRootContext=mParserContext;
  nsITokenizer* theTokenizer=mParserContext->mDTD->GetTokenizer();
  while(theRootContext->mPrevContext) {
    theRootContext=theRootContext->mPrevContext;
  }

  nsIDTD* theRootDTD=theRootContext->mDTD;
  nsresult result=NS_OK;
  if(theRootDTD) {
    result=theRootDTD->BuildModel(this,theTokenizer,mTokenObserver,mSink);
  }

  return result;
}


/**
 * 
 * @update	gess1/22/99
 * @param 
 * @return
 */
nsITokenizer* nsParser::GetTokenizer(void) {
  nsITokenizer* theTokenizer=0;
  if(mParserContext && mParserContext->mDTD) {
    theTokenizer=mParserContext->mDTD->GetTokenizer();
  }
  return theTokenizer;
}

/*******************************************************************
  These methods are used to talk to the netlib system...
 *******************************************************************/

/**
 *  
 *  
 *  @update  gess 5/12/98
 *  @param   
 *  @return  error code -- 0 if ok, non-zero if error.
 */
nsresult nsParser::GetBindInfo(nsIURL* aURL, nsStreamBindingInfo* aInfo){
  nsresult result=0;
  return result;
}

/**
 *  
 *  
 *  @update  gess 5/12/98
 *  @param   
 *  @return  error code -- 0 if ok, non-zero if error.
 */
nsresult
nsParser::OnProgress(nsIURL* aURL, PRUint32 aProgress, PRUint32 aProgressMax)
{
  nsresult result=0;
  if (nsnull != mObserver) {
    mObserver->OnProgress(aURL, aProgress, aProgressMax);
  }
  return result;
}

/**
 *  
 *  
 *  @update  gess 5/12/98
 *  @param   
 *  @return  error code -- 0 if ok, non-zero if error.
 */
nsresult
nsParser::OnStatus(nsIURL* aURL, const PRUnichar* aMsg)
{
  nsresult result=0;
  if (nsnull != mObserver) {
    mObserver->OnStatus(aURL, aMsg);
  }
  return result;
}

#ifdef rickgdebug
  fstream* gDumpFile;
#endif

/**
 *  
 *  
 *  @update  gess 5/12/98
 *  @param   
 *  @return  error code -- 0 if ok, non-zero if error.
 */
nsresult nsParser::OnStartBinding(nsIURL* aURL, const char *aSourceType){
  NS_PRECONDITION((eNone==mParserContext->mStreamListenerState),kBadListenerInit);

  if (nsnull != mObserver) {
    mObserver->OnStartBinding(aURL, aSourceType);
  }
  mParserContext->mStreamListenerState=eOnStart;
  mParserContext->mAutoDetectStatus=eUnknownDetect;
  mParserContext->mDTD=0;
  mParserContext->mSourceType=aSourceType;

#ifdef rickgdebug
  gDumpFile = new fstream("c:/temp/out.file",ios::trunc);
#endif

  return NS_OK;
}

/**
 *  
 *  
 *  @update  gess 1/4/99
 *  @param   pIStream contains the input chars
 *  @param   length is the number of bytes waiting input
 *  @return  error code (usually 0)
 */
nsresult nsParser::OnDataAvailable(nsIURL* aURL, nsIInputStream *pIStream, PRUint32 aLength){
/*  if (nsnull != mListener) {
      //Rick potts removed this.
      //Does it need to be here?
    mListener->OnDataAvailable(pIStream, aLength);
  }
*/
  NS_PRECONDITION(((eOnStart==mParserContext->mStreamListenerState)||(eOnDataAvail==mParserContext->mStreamListenerState)),kOnStartNotCalled);

  if(eInvalidDetect==mParserContext->mAutoDetectStatus) {
    if(mParserContext->mScanner) {
      mParserContext->mScanner->GetBuffer().Truncate();
    }
  }

  PRInt32 newLength=(aLength>mParserContext->mTransferBufferSize) ? aLength : mParserContext->mTransferBufferSize;
  if(!mParserContext->mTransferBuffer) {
    mParserContext->mTransferBufferSize=newLength;
    mParserContext->mTransferBuffer=new char[newLength+20];
  }
  else if(aLength>mParserContext->mTransferBufferSize){
    delete [] mParserContext->mTransferBuffer;
    mParserContext->mTransferBufferSize=newLength;
    mParserContext->mTransferBuffer=new char[newLength+20];
  }

  PRUint32  theTotalRead=0; 
  PRUint32  theNumRead=1;   //init to a non-zero value
  int       theStartPos=0;
  nsresult result=NS_OK;

  while ((theNumRead>0) && (aLength>theTotalRead) && (NS_OK==result)) {
    result = pIStream->Read(mParserContext->mTransferBuffer, aLength, &theNumRead);
    if((result == NS_OK) && (theNumRead>0)) {
      theTotalRead+=theNumRead;
      if(mParserFilter)
         mParserFilter->RawBuffer(mParserContext->mTransferBuffer, &theNumRead);

#ifdef  NS_DEBUG
      int index=0;
      for(index=0;index<10;index++)
        mParserContext->mTransferBuffer[theNumRead+index]=0;
#endif

      mParserContext->mScanner->Append(mParserContext->mTransferBuffer,theNumRead);
      nsString& theBuffer=mParserContext->mScanner->GetBuffer();
      theBuffer.ToUCS2(theStartPos);

#ifdef rickgdebug
      (*gDumpFile) << mParserContext->mTransferBuffer;
#endif

    } //if
    theStartPos+=theNumRead;
  }//while

  result=ResumeParse(); 
  return result;
}

/**
 *  This is called by the networking library once the last block of data
 *  has been collected from the net.
 *  
 *  @update  gess 04/01/99
 *  @param   
 *  @return  
 */
nsresult nsParser::OnStopBinding(nsIURL* aURL, nsresult status, const PRUnichar* aMsg){
  mParserContext->mStreamListenerState=eOnStop;
  mStreamStatus=status;
  nsresult result=ResumeParse();
  // If the parser isn't enabled, we don't finish parsing till
  // it is reenabled.


  // XXX Should we wait to notify our observers as well if the
  // parser isn't yet enabled?
  if (nsnull != mObserver) {
    mObserver->OnStopBinding(aURL, status, aMsg);
  }

#ifdef rickgdebug
  if(gDumpFile){
    gDumpFile->close();
    delete gDumpFile;
  }
#endif

  return result;
}


/*******************************************************************
  Here comes the tokenization methods...
 *******************************************************************/


/**
 *  Part of the code sandwich, this gets called right before
 *  the tokenization process begins. The main reason for
 *  this call is to allow the delegate to do initialization.
 *  
 *  @update  gess 01/04/99
 *  @param   
 *  @return  TRUE if it's ok to proceed
 */
PRBool nsParser::WillTokenize(){
  PRBool result=PR_TRUE;
  return result;
}


/**
 *  This is the primary control routine to consume tokens. 
 *	It iteratively consumes tokens until an error occurs or 
 *	you run out of data.
 *  
 *  @update  gess 01/04/99
 *  @return  error code -- 0 if ok, non-zero if error.
 */
nsresult nsParser::Tokenize(){

  nsresult result=NS_OK;

  ++mMajorIteration; 

  WillTokenize();
  nsITokenizer* theTokenizer=mParserContext->mDTD->GetTokenizer();
  while(NS_SUCCEEDED(result)) {
    mParserContext->mScanner->Mark();
    ++mMinorIteration;
    result=theTokenizer->ConsumeToken(*mParserContext->mScanner);
    if(!NS_SUCCEEDED(result)) {
      mParserContext->mScanner->RewindToMark();
      if(kEOF==result){
        result=NS_OK;
        break;
      }
    }
  } 
  DidTokenize();
  return result;
}

/**
 *  This is the tail-end of the code sandwich for the
 *  tokenization process. It gets called once tokenziation
 *  has completed for each phase.
 *  
 *  @update  gess 01/04/99
 *  @param   
 *  @return  TRUE if all went well
 */
PRBool nsParser::DidTokenize(){
  PRBool result=PR_TRUE;

  if(mTokenObserver) {
    nsITokenizer* theTokenizer=mParserContext->mDTD->GetTokenizer();
    if(theTokenizer) {
      PRInt32 theCount=theTokenizer->GetCount();
      PRInt32 theIndex;
      for(theIndex=0;theIndex<theCount;theIndex++){
        if((*mTokenObserver)(theTokenizer->GetTokenAt(theIndex))){
          //add code here to pull unwanted tokens out of the stack...
        }
      }//for
    }//if
  }//if
  return result;
}

void nsParser::DebugDumpSource(ostream& aStream) {
  PRInt32 theIndex=-1;
  nsITokenizer* theTokenizer=mParserContext->mDTD->GetTokenizer();
  CToken* theToken;
  while(nsnull != (theToken=theTokenizer->GetTokenAt(++theIndex))) {
    // theToken->DebugDumpToken(out);
    theToken->DebugDumpSource(aStream);
  }
}


