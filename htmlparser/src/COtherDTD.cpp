#if 1
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */  
/*                               
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of 
 * the License at http://www.mozilla.org/NPL/
 *        
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or 
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *  
 * The Original Code is mozilla.org code. 
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved. 
 * 
 * Contributor(s):   
 */          
    
//#define ENABLE_CRC  
//#define RICKG_DEBUG    

      
#include "nsDebug.h" 
#include "nsIDTDDebug.h"  
#include "COtherDTD.h" 
#include "nsHTMLTokens.h"
#include "nsCRT.h"    
#include "nsParser.h"  
#include "nsIParser.h"
#include "nsIHTMLContentSink.h"  
#include "nsScanner.h"
#include "nsIDTDDebug.h"
#include "prenv.h"  //this is here for debug reasons...
#include "prtypes.h"  //this is here for debug reasons...
#include "prio.h"
#include "plstr.h"  
#include "nsDTDUtils.h"
#include "nsTagHandler.h" 
#include "nsHTMLTokenizer.h"
#include "nsTime.h"
#include "nsIElementObserver.h"
#include "nsViewSourceHTML.h" 
#include "nsParserNode.h"
#include "nsHTMLEntities.h"
#include "nsLinebreakConverter.h"

#include "prmem.h"


static NS_DEFINE_IID(kIHTMLContentSinkIID, NS_IHTML_CONTENT_SINK_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);                 
static NS_DEFINE_IID(kIDTDIID,      NS_IDTD_IID);
static NS_DEFINE_IID(kClassIID,     NS_IOTHERHTML_DTD_IID); 
 
static const  char* kNullToken = "Error: Null token given";
static const  char* kInvalidTagStackPos = "Error: invalid tag stack position";
static char*        kVerificationDir = "c:/temp";


#ifdef  ENABLE_CRC
static char gShowCRC;
#endif

   
 
#ifdef MOZ_PERF_METRICS 
#  define START_TIMER()                    \
    if(mParser) MOZ_TIMER_START(mParser->mParseTime); \
    if(mParser) MOZ_TIMER_START(mParser->mDTDTime); 

#  define STOP_TIMER()                     \
    if(mParser) MOZ_TIMER_STOP(mParser->mParseTime); \
    if(mParser) MOZ_TIMER_STOP(mParser->mDTDTime); 
#else
#  define STOP_TIMER() 
#  define START_TIMER()
#endif


#include "COtherElements.h"


/************************************************************************
  And now for the main class -- COtherDTD...
 ************************************************************************/

/**
 *  This method gets called as part of our COM-like interfaces.
 *  Its purpose is to create an interface to parser object
 *  of some type.
 *  
 *  @update   gess 4/8/98
 *  @param    nsIID  id of object to discover
 *  @param    aInstancePtr ptr to newly discovered interface
 *  @return   NS_xxx result code 
 */  
nsresult COtherDTD::QueryInterface(const nsIID& aIID, void** aInstancePtr)  
{                                                                         
  if (NULL == aInstancePtr) {                                            
    return NS_ERROR_NULL_POINTER;                                        
  }                                                                      

  if(aIID.Equals(kISupportsIID))    {  //do IUnknown...
    *aInstancePtr = (nsIDTD*)(this);                                        
  }
  else if(aIID.Equals(kIDTDIID)) {  //do IParser base class...
    *aInstancePtr = (nsIDTD*)(this);                                        
  }
  else if(aIID.Equals(kClassIID)) {  //do this class...
    *aInstancePtr = (COtherDTD*)(this);                                        
  }                 
  else {
    *aInstancePtr=0;
    return NS_NOINTERFACE;
  }
  NS_ADDREF_THIS();
  return NS_OK;                                                        
}

NS_IMPL_ADDREF(COtherDTD)
NS_IMPL_RELEASE(COtherDTD)

/**
 *  Default constructor
 *   
 *  @update  gess 4/9/98
 *  @param   
 *  @return   
 */ 
COtherDTD::COtherDTD() : nsIDTD(), mMisplacedContent(0), mSkippedContent(0), mSharedNodes(0) {
  NS_INIT_REFCNT();
  mSink = 0; 
  mParser=0;       
  mDTDDebug=0;   
  mLineNumber=1;  
  mHasOpenBody=PR_FALSE;
  mHasOpenHead=0;
  mHasOpenForm=PR_FALSE;
  mHasOpenMap=PR_FALSE;
  mHeadContext=new nsDTDContext();
  mBodyContext=new nsDTDContext();
  mFormContext=0;
  mTokenizer=0;
  mComputedCRC32=0;
  mExpectedCRC32=0;
  mDTDState=NS_OK;
  mDocType=ePlainText;

  char* theEnvString = PR_GetEnv("ENABLE_STRICT"); 
  mEnableStrict=PRBool(theEnvString!=0);

  if(!gElementTable) {
    gElementTable = new CElementTable();
  }

#ifdef  RICKG_DEBUG
  //DebugDumpContainmentRules2(*this,"c:/temp/DTDRules.new","New COtherDTD Containment Rules");
  nsHTMLElement::DebugDumpContainment("c:/temp/contain.new","ElementTable Rules");
  nsHTMLElement::DebugDumpMembership("c:/temp/membership.out");
  nsHTMLElement::DebugDumpContainType("c:/temp/ctnrules.out");
#endif

#ifdef NS_DEBUG
  gNodeCount=0;
#endif
}


/**
 * This method creates a new parser node. It tries to get one from
 * the recycle list before allocating a new one.
 * @update  gess1/8/99
 * @param 
 * @return valid node*
 */
nsCParserNode* COtherDTD::CreateNode(void) {

  nsCParserNode* result=0;
  if(0<mSharedNodes.GetSize()) {
    result=(nsCParserNode*)mSharedNodes.Pop();
  } 
  else{
    result=new nsCParserNode();
#ifdef NS_DEBUG 
#if 1
    gNodeCount++; 
#endif
#endif
  }
  return result;
} 


/**
 * This method recycles a given node
 * @update  gess1/8/99
 * @param 
 * @return  
 */
void COtherDTD::RecycleNode(nsCParserNode* aNode) {
  if(aNode && (!aNode->mUseCount)) {

    if(aNode->mToken) {
      if(!aNode->mToken->mUseCount) { 
        mTokenRecycler->RecycleToken(aNode->mToken); 
      }
    } 

    CToken* theToken=0;
    while((theToken=(CToken*)aNode->PopAttributeToken())){
      if(!theToken->mUseCount) { 
        mTokenRecycler->RecycleToken(theToken); 
      }
    }

    mSharedNodes.Push(aNode);
  }
}

/**
 * This method recycles the nodes on a nodestack.
 * NOTE: Unlike recycleNode(), we force the usecount
 *       to 0 of all nodes, then force them to recycle.
 * @update  gess1/8/99
 * @param   aNodeStack
 * @return  nothing
 */
void COtherDTD::RecycleNodes(nsEntryStack *aNodeStack) {
  if(aNodeStack) {
    PRInt32 theCount=aNodeStack->mCount;
    PRInt32 theIndex=0;

    for(theIndex=0;theIndex<theCount;theIndex++) {
      nsCParserNode* theNode=(nsCParserNode*)aNodeStack->NodeAt(theIndex);
      if(theNode) {

        theNode->mUseCount=0;
        if(theNode->mToken) {
          theNode->mToken->mUseCount=0;
          mTokenRecycler->RecycleToken(theNode->mToken); 
        } 

        CToken* theToken=0;
        while((theToken=(CToken*)theNode->PopAttributeToken())){
          theNode->mToken->mUseCount=0;
          mTokenRecycler->RecycleToken(theToken); 
        }

        mSharedNodes.Push(theNode);
      } //if
    } //while
  } //if
}

/**
 * 
 * @update  gess1/8/99
 * @param 
 * @return
 */
const nsIID& COtherDTD::GetMostDerivedIID(void)const {
  return kClassIID;
}
 
/**
 *  Default destructor
 *  
 *  @update  gess 4/9/98
 *  @param   
 *  @return  
 */
COtherDTD::~COtherDTD(){
  delete mHeadContext;
  delete mBodyContext;

  NS_IF_RELEASE(mTokenizer);

  nsCParserNode* theNode=0;

#ifdef NS_DEBUG
#if 0
  PRInt32 count=gNodeCount-mSharedNodes.GetSize();
  if(count) {
    printf("%i of %i nodes leaked!\n",count,gNodeCount);
  }
#endif
#endif

#if 1
  while((theNode=(nsCParserNode*)mSharedNodes.Pop())){
    delete theNode;
  }
#endif

#ifdef  NS_DEBUG
  gNodeCount=0;
#endif

  NS_IF_RELEASE(mSink);
  NS_IF_RELEASE(mDTDDebug);
}
 
/**
 *  This method is defined in nsIParser. It is used to 
 *  cause the COM-like construction of an nsParser.
 *  
 *  @update  gess 4/8/98
 *  @param   nsIParser** ptr to newly instantiated parser
 *  @return  NS_xxx error result
 */
NS_HTMLPARS nsresult NS_NewOtherHTMLDTD(nsIDTD** aInstancePtrResult) {
  COtherDTD* it = new COtherDTD();

  if (it == 0) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return it->QueryInterface(kClassIID, (void **) aInstancePtrResult);
}

/**
 * Call this method if you want the DTD to construct a fresh 
 * instance of itself. 
 * @update  gess7/23/98
 * @param 
 * @return
 */
nsresult COtherDTD::CreateNewInstance(nsIDTD** aInstancePtrResult){
  return NS_NewOtherHTMLDTD(aInstancePtrResult);
}

/**
 * Called by the parser to initiate dtd verification of the
 * internal context stack.
 * @update  gess 7/23/98
 * @param 
 * @return
 */
PRBool COtherDTD::Verify(nsString& aURLRef,nsIParser* aParser){
  PRBool result=PR_TRUE;

  /*
   * Disable some DTD debugging code in the parser that 
   * breaks on some compilers because of some broken 
   * streams code in prstrm.cpp.
   */
#if !defined(MOZ_DISABLE_DTD_DEBUG)
  if(!mDTDDebug){
    nsresult rval = NS_NewDTDDebug(&mDTDDebug);
    if (NS_OK != rval) {
      fputs("Cannot create parser debugger.\n", stdout);
      result=-PR_FALSE;
    }
    else mDTDDebug->SetVerificationDirectory(kVerificationDir);
  }
#endif

  if(mDTDDebug) {
    // mDTDDebug->Verify(this,aParser,mBodyContext->GetCount(),mBodyContext->mStack,aURLRef);
  }
  return result;
}

/**
 * This method is called to determine if the given DTD can parse
 * a document in a given source-type. 
 * NOTE: Parsing always assumes that the end result will involve
 *       storing the result in the main content model.
 * @update  gess6/24/98
 * @param   
 * @return  TRUE if this DTD can satisfy the request; FALSE otherwise.
 */
eAutoDetectResult COtherDTD::CanParse(CParserContext& aParserContext,nsString& aBuffer, PRInt32 aVersion) {
  eAutoDetectResult result=eUnknownDetect;

  if(mEnableStrict) {
    if(eViewSource==aParserContext.mParserCommand) {
      if(PR_TRUE==aParserContext.mMimeType.EqualsWithConversion(kPlainTextContentType)) {
        result=eValidDetect;
      }
      else if(aParserContext.mMimeType.EqualsWithConversion(kRTFTextContentType)){ 
        result=eValidDetect;
      }
    }
    else {
      if(PR_TRUE==aParserContext.mMimeType.EqualsWithConversion(kHTMLTextContentType)) {
        result=(eParseMode_strict==aParserContext.mParseMode) ? ePrimaryDetect : eValidDetect;
      }
      else if(PR_TRUE==aParserContext.mMimeType.EqualsWithConversion(kPlainTextContentType)) {
        result=eValidDetect;
      }
      else {
        //otherwise, look into the buffer to see if you recognize anything...
        PRBool theBufHasXML=PR_FALSE;
        if(BufferContainsHTML(aBuffer,theBufHasXML)){
          result = eValidDetect ;
          if(0==aParserContext.mMimeType.Length()) {
            aParserContext.SetMimeType(NS_ConvertToString(kHTMLTextContentType));
            if(!theBufHasXML) {
              result=(eParseMode_strict==aParserContext.mParseMode) ? ePrimaryDetect : eValidDetect;
            }
            else result=eValidDetect;
          }
        }
      }
    }
    return result;
  }
  return result;
}

 
/**
  * The parser uses a code sandwich to wrap the parsing process. Before
  * the process begins, WillBuildModel() is called. Afterwards the parser
  * calls DidBuildModel(). 
  * @update	rickg 03.20.2000
  * @param	aParserContext
  * @param	aSink
  * @return	error code (almost always 0)
  */
nsresult COtherDTD::WillBuildModel(  const CParserContext& aParserContext,nsIContentSink* aSink){
  nsresult result=NS_OK;

  mFilename=aParserContext.mScanner->GetFilename();
  mHasOpenBody=PR_FALSE;
  mHadBody=PR_FALSE;
  mHadFrameset=PR_FALSE;
  mLineNumber=1;
  mHasOpenScript=PR_FALSE;
  mParseMode=aParserContext.mParseMode;
  mParserCommand=aParserContext.mParserCommand;

  if((!aParserContext.mPrevContext) && (aSink)) {

    STOP_TIMER();
    MOZ_TIMER_DEBUGLOG(("Stop: Parse Time: COtherDTD::WillBuildModel(), this=%p\n", this));

    mTokenRecycler=0;

    mDocType=aParserContext.mDocType;
    if(aSink && (!mSink)) {
      result=aSink->QueryInterface(kIHTMLContentSinkIID, (void **)&mSink);
    }

    if(result==NS_OK) {
      result = aSink->WillBuildModel();

      MOZ_TIMER_DEBUGLOG(("Start: Parse Time: COtherDTD::WillBuildModel(), this=%p\n", this));
      START_TIMER();

      mSkipTarget=eHTMLTag_unknown;
      mComputedCRC32=0;
      mExpectedCRC32=0;
    }
  }

  return result;
}


/**
  * This is called when it's time to read as many tokens from the tokenizer
  * as you can. Not all tokens may make sense, so you may not be able to
  * read them all (until more come in later).
  *
  * @update gess5/18/98
  * @param  aParser is the parser object that's driving this process
  * @return error code (almost always NS_OK)
  */
nsresult COtherDTD::BuildModel(nsIParser* aParser,nsITokenizer* aTokenizer,nsITokenObserver* anObserver,nsIContentSink* aSink) {
  nsresult result=NS_OK;

  if(aTokenizer) {
    nsITokenizer*  oldTokenizer=mTokenizer;
    mTokenizer=aTokenizer;
    mParser=(nsParser*)aParser;

    if(mTokenizer) {

      mTokenRecycler=(CTokenRecycler*)mTokenizer->GetTokenRecycler();
      if(mSink) {
    
        if(!mBodyContext->GetCount()) {
            //if the content model is empty, then begin by opening <html>...
          CStartToken *theToken=(CStartToken*)mTokenRecycler->CreateTokenOfType(eToken_start,eHTMLTag_html, NS_ConvertToString("html"));
          HandleStartToken(theToken); //this token should get pushed on the context stack, don't recycle it.
        }

        while(NS_SUCCEEDED(result)){

#if 0
          int n=aTokenizer->GetCount(); 
          if(n>50) n=50;
          for(int i=0;i<n;i++){
            CToken* theToken=aTokenizer->GetTokenAt(i);
            printf("\nToken[%i],%p",i,theToken);
          }
          printf("\n");
#endif

          if(mDTDState!=NS_ERROR_HTMLPARSER_STOPPARSING) {
            CToken* theToken=mTokenizer->PopToken();
            if(theToken) { 
              result=HandleToken(theToken,aParser);
            }
            else break;
          }
          else {
            result=mDTDState;
            break;
          }
        }//while
        mTokenizer=oldTokenizer;
      }
    }
  }
  else result=NS_ERROR_HTMLPARSER_BADTOKENIZER;
  return result;
}

/**
 * 
 * @update  gess5/18/98
 * @param 
 * @return 
 */
nsresult COtherDTD::DidBuildModel(nsresult anErrorCode,PRBool aNotifySink,nsIParser* aParser,nsIContentSink* aSink){
  nsresult result=NS_OK;

  if(aSink) { 

    if((NS_OK==anErrorCode) && (!mHadBody) && (!mHadFrameset)) { 

      mSkipTarget=eHTMLTag_unknown; //clear this in case we were searching earlier.

      if(ePlainText==mDocType) {
        CStartToken *theToken=(CStartToken*)mTokenRecycler->CreateTokenOfType(eToken_start,eHTMLTag_pre, NS_ConvertToString("pre"));
        mTokenizer->PushTokenFront(theToken); //this token should get pushed on the context stack, don't recycle it 
      }

      CStartToken *theToken=(CStartToken*)mTokenRecycler->CreateTokenOfType(eToken_start,eHTMLTag_body, NS_ConvertToString("body"));
      mTokenizer->PushTokenFront(theToken); //this token should get pushed on the context stack, don't recycle it 

      result=BuildModel(aParser,mTokenizer,0,aSink);
    } 

    if(aParser && (NS_OK==result)){ 
      if(aNotifySink){ 
        if((NS_OK==anErrorCode) && (mBodyContext->GetCount()>0)) {

          while(mBodyContext->GetCount() > 0) {  
            eHTMLTags theTarget = mBodyContext->Last(); 
            CElement* theElement=gElementTable->mElements[theTarget];
            if(theElement) {
              theElement->HandleEndToken(0,theTarget,mBodyContext,mSink);
            }
          }    
  
        }      
        else {
          //If you're here, then an error occured, but we still have nodes on the stack.
          //At a minimum, we should grab the nodes and recycle them.
          //Just to be correct, we'll also recycle the nodes. 
  
          while(mBodyContext->GetCount() > 0) {  
 
            nsEntryStack *theChildStyles=0;
            nsCParserNode* theNode=(nsCParserNode*)mBodyContext->Pop(theChildStyles);
            theNode->mUseCount=0;
            RecycleNode(theNode);
            if(theChildStyles) {
              delete theChildStyles;
            } 
          }    
        }    
  
      } 
    } //if aparser  

      //No matter what, you need to call did build model.
    result=aSink->DidBuildModel(0); 

  } //if asink
  return result; 
}  

/** 
 *  This big dispatch method is used to route token handler calls to the right place.
 *  What's wrong with it? This table, and the dispatch methods themselves need to be 
 *  moved over to the delegate. Ah, so much to do...
 *  
 *  @update  gess 12/1/99
 *  @param   aToken 
 *  @param   aParser   
 *  @return    
 */
nsresult COtherDTD::HandleToken(CToken* aToken,nsIParser* aParser){
  nsresult  result=NS_OK;

  if(aToken) {
    CHTMLToken*     theToken= (CHTMLToken*)(aToken);
    eHTMLTokenTypes theType=eHTMLTokenTypes(theToken->GetTokenType());
    eHTMLTags       theTag=(eHTMLTags)theToken->GetTypeID();
    PRBool          execSkipContent=PR_FALSE;

    theToken->mUseCount=0;  //assume every token coming into this system needs recycling.


    mParser=(nsParser*)aParser;

    switch(theType) {
      case eToken_text:
      case eToken_start:
      case eToken_whitespace: 
      case eToken_newline:
        result=HandleStartToken(theToken); break;

      case eToken_end:
        result=HandleEndToken(theToken); break;
       
      case eToken_cdatasection:
      case eToken_comment:
        result=HandleCommentToken(theToken); break;

      case eToken_entity:
        result=HandleEntityToken(theToken); break;

      case eToken_attribute:
        result=HandleAttributeToken(theToken); break;

      case eToken_instruction:
        result=HandleProcessingInstructionToken(theToken); break;

      case eToken_doctypeDecl:
        result=HandleDocTypeDeclToken(theToken); break;

      default: 
        break; 
    }//switch


    if(NS_SUCCEEDED(result) || (NS_ERROR_HTMLPARSER_BLOCK==result)) {
      if(0>=theToken->mUseCount)
        mTokenRecycler->RecycleToken(theToken);
    }
    else if(result==NS_ERROR_HTMLPARSER_STOPPARSING)
      mDTDState=result; 
    else return NS_OK;

  }//if
  return result;
}

 
/**
 * This gets called after we've handled a given start tag.
 * It's a generic hook to let us to post processing.
 * @param   aToken contains the tag in question
 * @param   aChildTag is the tag itself.
 * @return  status
 */
nsresult COtherDTD::DidHandleStartTag(nsCParserNode& aNode,eHTMLTags aChildTag){
  nsresult result=NS_OK;

  switch(aChildTag){

    case eHTMLTag_pre:
    case eHTMLTag_listing:
      {
        CToken* theNextToken=mTokenizer->PeekToken();
        if(theNextToken)  {
          eHTMLTokenTypes theType=eHTMLTokenTypes(theNextToken->GetTokenType());
          if(eToken_newline==theType){
            mLineNumber++;
            mTokenizer->PopToken();  //skip 1st newline inside PRE and LISTING
          }//if
        }//if
      }
      break;

    case eHTMLTag_plaintext:
    case eHTMLTag_xmp:
      //grab the skipped content and dump it out as text...
      {        
        STOP_TIMER()
        MOZ_TIMER_DEBUGLOG(("Stop: Parse Time: COtherDTD::DidHandleStartTag(), this=%p\n", this));
        const nsString& theString=aNode.GetSkippedContent();
        if(0<theString.Length()) {
          CViewSourceHTML::WriteText(theString,*mSink,PR_TRUE,PR_FALSE);
        }
        MOZ_TIMER_DEBUGLOG(("Start: Parse Time: COtherDTD::DidHandleStartTag(), this=%p\n", this));
        START_TIMER()
      }
      break;
    default:
      break;
  }//switch 

  return result;
} 
 

#ifdef  RICKG_DEBUG
void WriteTokenToLog(CToken* aToken) {

  static nsFileSpec fileSpec("c:\\temp\\tokenlog.html");
  static nsOutputFileStream outputStream(fileSpec);
  aToken->DebugDumpSource(outputStream); //write token without close bracket...
}
#endif
 
/**
 * This gets called before we've handled a given start tag.
 * It's a generic hook to let us do pre processing.
 * @param   aToken contains the tag in question
 * @param   aChildTag is the tag itself.
 * @param   aNode is the node (tag) with associated attributes.
 * @return  TRUE if tag processing should continue; FALSE if the tag has been handled.
 */
nsresult COtherDTD::WillHandleStartTag(CToken* aToken,eHTMLTags aTag,nsCParserNode& aNode){
  nsresult result=NS_OK; 
  PRInt32 theAttrCount  = aNode.GetAttributeCount(); 

    //first let's see if there's some skipped content to deal with... 
#if 0
  if(*gElementTable->mElements[aTag].mSkipTarget) { 
    result=CollectSkippedContent(aNode,theAttrCount); 
  } 
#endif
  
  STOP_TIMER()
  MOZ_TIMER_DEBUGLOG(("Stop: Parse Time: COtherDTD::WillHandleStartTag(), this=%p\n", this));

  if(mParser) {

    switch(aTag) {
      case eHTMLTag_newline:
        mLineNumber++;
        break;
      default:
        break;
    }

    CObserverService* theService=mParser->GetObserverService();
    if(theService) {
      CParserContext*   pc=mParser->PeekContext();
      void*             theDocID=(pc)? pc->mKey:0;
    
      result=theService->Notify(aTag,aNode,theDocID, NS_ConvertToString(kHTMLTextContentType), mParser);
    }
  }

  MOZ_TIMER_DEBUGLOG(("Start: Parse Time: COtherDTD::WillHandleStartTag(), this=%p\n", this));
  START_TIMER()

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
 *  @update  gess 1/04/99
 *  @param   aToken -- next (start) token to be handled
 *  @param   aNode -- CParserNode representing this start token
 *  @return  PR_TRUE if all went well; PR_FALSE if error occured
 */      
nsresult COtherDTD::HandleStartToken(CToken* aToken) {
  NS_PRECONDITION(0!=aToken,kNullToken);
 
  #ifdef  RICKG_DEBUG
    WriteTokenToLog(aToken);
  #endif

  //Begin by gathering up attributes... 

  nsCParserNode* theNode=CreateNode();
  theNode->Init(aToken,mLineNumber,mTokenRecycler);
  
  eHTMLTags     theChildTag=(eHTMLTags)aToken->GetTypeID();
  PRInt16       attrCount=aToken->GetAttributeCount();
  eHTMLTags     theParent=mBodyContext->Last();
  nsresult      result=(0==attrCount) ? NS_OK : CollectAttributes(*theNode,theChildTag,attrCount);

  if(NS_OK==result) {
    result=WillHandleStartTag(aToken,theChildTag,*theNode);
    if(NS_OK==result) {
 
      mLineNumber += aToken->mNewlineCount;

      PRBool theTagWasHandled=PR_FALSE;

      switch(theChildTag) {   

        case eHTMLTag_html:
          if(!HasOpenContainer(theChildTag)) {
            result=OpenContainer(theNode,theChildTag,PR_FALSE,0);
          }
          theTagWasHandled=PR_TRUE;
          break;

        case eHTMLTag_area:
          if(!mHasOpenMap) theTagWasHandled=PR_TRUE;

          STOP_TIMER();
          MOZ_TIMER_DEBUGLOG(("Stop: Parse Time: COtherDTD::HandleStartToken(), this=%p\n", this));
          
          if (mHasOpenMap && mSink) {
            result=mSink->AddLeaf(*theNode);
            theTagWasHandled=PR_TRUE;
          }
          
          MOZ_TIMER_DEBUGLOG(("Start: Parse Time: COtherDTD::HandleStartToken(), this=%p\n", this));
          START_TIMER();
          break; 
 
        //case eHTMLTag_userdefined:
        case eHTMLTag_noscript:     //HACK XXX! Throw noscript on the floor for now.
          theTagWasHandled=PR_TRUE;
          break;
  
        case eHTMLTag_script: 
          mHasOpenScript=PR_TRUE;
          break;  
   
        case eHTMLTag_body:   
          mHadBody=PR_TRUE;  
          //fall thru...    
     
        default:  
          CElement* theElement=gElementTable->mElements[theParent];
          if(theElement) {
            result=theElement->HandleStartToken(theNode,theChildTag,mBodyContext,mSink);  
            theTagWasHandled=PR_TRUE;  
          }   
          break;  
      }//switch        
    
      if(theTagWasHandled) {
        DidHandleStartTag(*theNode,theChildTag); 
      }
   
    } //if     
  }//if    
     
  RecycleNode(theNode);         
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
nsresult COtherDTD::HandleEndToken(CToken* aToken) {
  NS_PRECONDITION(0!=aToken,kNullToken);

  nsresult    result=NS_OK;
  eHTMLTags   theChildTag=(eHTMLTags)aToken->GetTypeID();
 
  #ifdef  RICKG_DEBUG 
    WriteTokenToLog(aToken); 
  #endif 
 
  switch(theChildTag) {

    case eHTMLTag_body: //we intentionally don't let the user close HTML or BODY
    case eHTMLTag_html:  
      break;

    case eHTMLTag_script:
      mHasOpenScript=PR_FALSE;

    default: 
      eHTMLTags theParent=mBodyContext->Last();
      CElement* theElement=gElementTable->mElements[theParent];
      if(theElement) {
        nsCParserNode theNode((CHTMLToken*)aToken,mLineNumber);
        result=theElement->HandleEndToken(&theNode,theChildTag,mBodyContext,mSink);  
      }   
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
nsresult COtherDTD::HandleEntityToken(CToken* aToken) { 
  NS_PRECONDITION(0!=aToken,kNullToken);

  nsresult  result=NS_OK;  
  eHTMLTags theParentTag=mBodyContext->Last(); 

  nsCParserNode* theNode=CreateNode();
  if(theNode) { 
    theNode->Init(aToken,mLineNumber,0);
  
    #ifdef  RICKG_DEBUG 
      WriteTokenToLog(aToken); 
    #endif

    result=AddLeaf(theNode); 
    RecycleNode(theNode);
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
nsresult COtherDTD::HandleCommentToken(CToken* aToken) {
  NS_PRECONDITION(0!=aToken,kNullToken);
  
  nsresult  result=NS_OK;

  nsString& theComment=aToken->GetStringValueXXX();
  mLineNumber += (theComment).CountChar(kNewLine);

  nsCParserNode* theNode=CreateNode();
  if(theNode) {
    theNode->Init(aToken,mLineNumber,0);

  #ifdef  RICKG_DEBUG
    WriteTokenToLog(aToken);
  #endif

    STOP_TIMER();
    MOZ_TIMER_DEBUGLOG(("Stop: Parse Time: COtherDTD::HandleCommentToken(), this=%p\n", this));

    result=(mSink) ? mSink->AddComment(*theNode) : NS_OK;  

    RecycleNode(theNode);

    MOZ_TIMER_DEBUGLOG(("Start: Parse Time: COtherDTD::HandleCommentToken(), this=%p\n", this));
    START_TIMER();
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
nsresult COtherDTD::HandleAttributeToken(CToken* aToken) {
  NS_PRECONDITION(0!=aToken,kNullToken);
  NS_ERROR("attribute encountered -- this shouldn't happen!");

  return NS_OK;
}


/**
 *  This method gets called when a script token has been 
 *  encountered in the parse process. n
 *  
 *  @update  gess 3/25/98
 *  @param   aToken -- next (start) token to be handled
 *  @return  PR_TRUE if all went well; PR_FALSE if error occured
 */
nsresult COtherDTD::HandleScriptToken(const nsIParserNode *aNode) {
  // PRInt32 attrCount=aNode.GetAttributeCount(PR_TRUE);

  STOP_TIMER();
  MOZ_TIMER_DEBUGLOG(("Stop: Parse Time: COtherDTD::HandleScriptToken(), this=%p\n", this));

  nsresult result=AddLeaf(aNode);

  MOZ_TIMER_DEBUGLOG(("Start: Parse Time: COtherDTD::HandleScriptToken(), this=%p\n", this));
  START_TIMER();

  return result;
}


/**
 *  This method gets called when an "instruction" token has been 
 *  encountered in the parse process. 
 *  
 *  @update  gess 3/25/98
 *  @param   aToken -- next (start) token to be handled
 *  @return  PR_TRUE if all went well; PR_FALSE if error occured
 */
nsresult COtherDTD::HandleProcessingInstructionToken(CToken* aToken){
  NS_PRECONDITION(0!=aToken,kNullToken);

  nsresult  result=NS_OK;

  nsCParserNode* theNode=CreateNode();
  if(theNode) {
    theNode->Init(aToken,mLineNumber,0);

  #ifdef  RICKG_DEBUG
    WriteTokenToLog(aToken);
  #endif

    STOP_TIMER();
    MOZ_TIMER_DEBUGLOG(("Stop: Parse Time: COtherDTD::HandleProcessingInstructionToken(), this=%p\n", this));

    result=(mSink) ? mSink->AddProcessingInstruction(*theNode) : NS_OK; 

    RecycleNode(theNode);

    MOZ_TIMER_DEBUGLOG(("Start: Parse Time: COtherDTD::HandleProcessingInstructionToken(), this=%p\n", this));
    START_TIMER();

  }
  return result;
}

/**
 *  This method gets called when a DOCTYPE token has been 
 *  encountered in the parse process. 
 *  
 *  @update  harishd 09/02/99
 *  @param   aToken -- The very first token to be handled
 *  @return  PR_TRUE if all went well; PR_FALSE if error occured
 */
nsresult COtherDTD::HandleDocTypeDeclToken(CToken* aToken){
  NS_PRECONDITION(0!=aToken,kNullToken);

  nsresult result=NS_OK;

  #ifdef  RICKG_DEBUG
    WriteTokenToLog(aToken);
  #endif

  nsString& docTypeStr=aToken->GetStringValueXXX();
  mLineNumber += (docTypeStr).CountChar(kNewLine);
  docTypeStr.Trim("<!>");

  nsCParserNode* theNode=CreateNode();
  if(theNode) {
    theNode->Init(aToken,mLineNumber,0);

  STOP_TIMER();
  MOZ_TIMER_DEBUGLOG(("Stop: Parse Time: COtherDTD::HandleDocTypeDeclToken(), this=%p\n", this));
  
    result = (mSink)? mSink->AddDocTypeDecl(*theNode,mParseMode):NS_OK;
    RecycleNode(theNode);
  
  MOZ_TIMER_DEBUGLOG(("Start: Parse Time: COtherDTD::HandleDocTypeDeclToken(), this=%p\n", this));
  START_TIMER();

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
nsresult COtherDTD::CollectAttributes(nsCParserNode& aNode,eHTMLTags aTag,PRInt32 aCount){
  int attr=0;

  nsresult result=NS_OK;
  int theAvailTokenCount=mTokenizer->GetCount() + mSkippedContent.GetSize();
  if(aCount<=theAvailTokenCount) {
    CToken* theToken=0; 
    eHTMLTags theSkipTarget=gElementTable->mElements[aTag]->GetSkipTarget();
    for(attr=0;attr<aCount;attr++){  
      if((eHTMLTag_unknown!=theSkipTarget) && mSkippedContent.GetSize())
        theToken=(CToken*)mSkippedContent.PopFront();
      else theToken=mTokenizer->PopToken(); 
      if(theToken)  {
        // Sanitize the key for it might contain some non-alpha-non-digit characters
        // at its end.  Ex. <OPTION SELECTED/> - This will be tokenized as "<" "OPTION",
        // "SELECTED/", and ">". In this case the "SELECTED/" key will be sanitized to
        // a legitimate "SELECTED" key.
        ((CAttributeToken*)theToken)->SanitizeKey();
 
  #ifdef  RICKG_DEBUG
    WriteTokenToLog(theToken);
  #endif
 
        aNode.AddAttribute(theToken); 
      }
    }
  }
  else { 
    result=kEOF; 
  }
  return result; 
} 


/**
 * Causes the next skipped-content token (if any) to
 * be consumed by this node.
 * @update  gess5/11/98 
 * @param   node to consume skipped-content
 * @param   holds the number of skipped content elements encountered
 * @return  Error condition.
 */
nsresult COtherDTD::CollectSkippedContent(nsCParserNode& aNode,PRInt32 &aCount) {

  eHTMLTags       theNodeTag=(eHTMLTags)aNode.GetNodeType();

  int aIndex=0; 
  int aMax=mSkippedContent.GetSize();
  
  // XXX rickg This linefeed conversion stuff should be moved out of
  // the parser and into the form element code
  PRBool aMustConvertLinebreaks = PR_FALSE; 
  
  mScratch.SetLength(0);    
  aNode.SetSkippedContent(mScratch); 
     
  for(aIndex=0;aIndex<aMax;aIndex++){
    CHTMLToken* theNextToken=(CHTMLToken*)mSkippedContent.PopFront();

    eHTMLTokenTypes theTokenType=(eHTMLTokenTypes)theNextToken->GetTokenType();
  
    mScratch.Truncate();
    // Dont worry about attributes here because it's already stored in 
    // the start token as mTrailing content and will get appended in 
    // start token's GetSource();
    if(eToken_attribute!=theTokenType) { 
      if (eToken_entity==theTokenType) {
        if((eHTMLTag_textarea==theNodeTag) || (eHTMLTag_title==theNodeTag)) {
          ((CEntityToken*)theNextToken)->TranslateToUnicodeStr(mScratch);
          // since this is an entity, we know that it's only one character.
          // check to see if it's a CR, in which case we'll need to do line
          // termination conversion at the end.
          aMustConvertLinebreaks |= (mScratch[0] == kCR);
        }
      }
      else theNextToken->GetSource(mScratch);
  
      aNode.mSkippedContent->Append(mScratch);
    }
    mTokenRecycler->RecycleToken(theNextToken); 
  }
    
  // if the string contained CRs (hence is either CR, or CRLF terminated)
  // we need to convert line breaks
  if (aMustConvertLinebreaks)
  {
    /*
    PRInt32  offset;
    while ((offset = aNode.mSkippedContent.Find("\r\n")) != kNotFound)
      aNode.mSkippedContent.Cut(offset, 1);		// remove the CR
  
    // now replace remaining CRs with LFs
    aNode.mSkippedContent.ReplaceChar("\r", kNewLine);
    */
#if 1
    nsLinebreakConverter::ConvertStringLineBreaks(*aNode.mSkippedContent,
         nsLinebreakConverter::eLinebreakAny, nsLinebreakConverter::eLinebreakContent);
#endif
  }
  
  // Let's hope that this does not hamper the  PERFORMANCE!!
  mLineNumber += aNode.mSkippedContent->CountChar(kNewLine);
  return NS_OK;
}
            
 /***********************************************************************************
   The preceeding tables determine the set of elements each tag can contain...
  ***********************************************************************************/
     
/** 
 *  This method is called to determine whether or not a tag
 *  of one type can contain a tag of another type.
 *  
 *  @update  gess 4/8/98
 *  @param   aParent -- tag enum of parent container
 *  @param   aChild -- tag enum of child container
 *  @return  PR_TRUE if parent can contain child
 */
PRBool COtherDTD::CanContain(PRInt32 aParent,PRInt32 aChild) const {
  PRBool result=PR_FALSE;
 //result=gHTMLElements[aParent].CanContain((eHTMLTags)aChild);
  return result;
} 

/** 
 * Give rest of world access to our tag enums, so that CanContain(), etc,
 * become useful.
 */ 
NS_IMETHODIMP COtherDTD::StringTagToIntTag(nsString &aTag, PRInt32* aIntTag) const {
  *aIntTag = nsHTMLTags::LookupTag(aTag);
  return NS_OK;
}

NS_IMETHODIMP COtherDTD::IntTagToStringTag(PRInt32 aIntTag, nsString& aTag) const {
  aTag.AssignWithConversion(nsHTMLTags::GetStringValue((nsHTMLTag)aIntTag));
  return NS_OK;
}  

NS_IMETHODIMP COtherDTD::ConvertEntityToUnicode(const nsString& aEntity, PRInt32* aUnicode) const {
  *aUnicode = nsHTMLEntities::EntityToUnicode(aEntity);
  return NS_OK;
}
 
     
/**
 *  This method gets called to determine whether a given 
 *  tag is itself a container
 *   
 *  @update  gess 4/8/98
 *  @param   aTag -- tag to test as a container
 *  @return  PR_TRUE if given tag can contain other tags
 */
PRBool COtherDTD::IsContainer(PRInt32 aTag) const {
  return gElementTable->mElements[eHTMLTags(aTag)]->IsContainer();
}

/**
 *  This method allows the caller to determine if a given container
 *  is currently open
 *  
 *  @update  gess 11/9/98 
 *  @param   
 *  @return  
 */
PRBool COtherDTD::HasOpenContainer(eHTMLTags aContainer) const {
  PRBool result=PR_FALSE;

  switch(aContainer) {
    case eHTMLTag_form:
      result=mHasOpenForm; break;
    case eHTMLTag_map: 
      result=mHasOpenMap; break; 
    default:
      result=mBodyContext->HasOpenContainer(aContainer);
      break;
  }
  return result;
}

/**
 *  This method allows the caller to determine if a any member
 *  in a set of tags is currently open
 *  
 *  @update  gess 11/9/98
 *  @param   
 *  @return  
 */
PRBool COtherDTD::HasOpenContainer(const eHTMLTags aTagSet[],PRInt32 aCount) const {

  int theIndex; 
  int theTopIndex=mBodyContext->GetCount()-1;

  for(theIndex=theTopIndex;theIndex>0;theIndex--){
    if(FindTagInSet((*mBodyContext)[theIndex],aTagSet,aCount))
      return PR_TRUE;
  }
  return PR_FALSE;
}


/*********************************************
  Here comes code that handles the interface
  to our content sink.
 *********************************************/
 

/**
 * This method does two things: 1st, help construct
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 * 
 * @update  gess4/22/98
 * @param   aNode -- next node to be added to model
 */
nsresult COtherDTD::OpenHTML(const nsIParserNode *aNode){
  NS_PRECONDITION(mBodyContext->GetCount() >= 0, kInvalidTagStackPos);

  STOP_TIMER();
  MOZ_TIMER_DEBUGLOG(("Stop: Parse Time: COtherDTD::OpenHTML(), this=%p\n", this));

  nsresult result=(mSink) ? mSink->OpenHTML(*aNode) : NS_OK; 

  MOZ_TIMER_DEBUGLOG(("Start: Parse Time: COtherDTD::OpenHTML(), this=%p\n", this));
  START_TIMER();

  mBodyContext->Push(aNode);
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
nsresult COtherDTD::CloseHTML(const nsIParserNode *aNode){

  STOP_TIMER();
  MOZ_TIMER_DEBUGLOG(("Stop: Parse Time: COtherDTD::CloseHTML(), this=%p\n", this));

  nsresult result=(mSink) ? mSink->CloseHTML(*aNode) : NS_OK; 

  MOZ_TIMER_DEBUGLOG(("Start: Parse Time: COtherDTD::CloseHTML(), this=%p\n", this));
  START_TIMER();

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
nsresult COtherDTD::OpenHead(const nsIParserNode *aNode){

  nsresult result=NS_OK;

  STOP_TIMER();
  MOZ_TIMER_DEBUGLOG(("Stop: Parse Time: COtherDTD::OpenHead(), this=%p\n", this));
 
  if(!mHasOpenHead++) {
    result=(mSink) ? mSink->OpenHead(*aNode) : NS_OK;
  }

  MOZ_TIMER_DEBUGLOG(("Start: Parse Time: COtherDTD::OpenHead(), this=%p\n", this));
  START_TIMER();

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
nsresult COtherDTD::CloseHead(const nsIParserNode *aNode){
  nsresult result=NS_OK;

  if(mHasOpenHead) {
    if(0==--mHasOpenHead){

      STOP_TIMER();
      MOZ_TIMER_DEBUGLOG(("Stop: Parse Time: COtherDTD::CloseHead(), this=%p\n", this));

      result=(mSink) ? mSink->CloseHead(*aNode) : NS_OK; 

      MOZ_TIMER_DEBUGLOG(("Start: Parse Time: COtherDTD::CloseHead(), this=%p\n", this));
      START_TIMER();

    }
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
nsresult COtherDTD::OpenBody(const nsIParserNode *aNode){
  NS_PRECONDITION(mBodyContext->GetCount() >= 0, kInvalidTagStackPos);

  nsresult result=NS_OK;

  mHadBody=PR_TRUE;

  PRBool theBodyIsOpen=HasOpenContainer(eHTMLTag_body);
  if(!theBodyIsOpen){
    //body is not already open, but head may be so close it
    PRInt32 theHTMLPos=mBodyContext->LastOf(eHTMLTag_html);
    result=CloseContainersTo(theHTMLPos+1,eHTMLTag_body,PR_TRUE);  //close current stack containers.
  }

  if(NS_OK==result) {

    STOP_TIMER();
    MOZ_TIMER_DEBUGLOG(("Stop: Parse Time: COtherDTD::OpenBody(), this=%p\n", this));

    result=(mSink) ? mSink->OpenBody(*aNode) : NS_OK; 

    MOZ_TIMER_DEBUGLOG(("Start: Parse Time: COtherDTD::OpenBody(), this=%p\n", this));
    START_TIMER();

    if(!theBodyIsOpen) {
      mBodyContext->Push(aNode);
      mTokenizer->PrependTokens(mMisplacedContent);
    }
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
nsresult COtherDTD::CloseBody(const nsIParserNode *aNode){
//  NS_PRECONDITION(mBodyContext->GetCount() >= 0, kInvalidTagStackPos);

  STOP_TIMER();
  MOZ_TIMER_DEBUGLOG(("Stop: Parse Time: COtherDTD::CloseBody(), this=%p\n", this));

  nsresult result=(mSink) ? mSink->CloseBody(*aNode) : NS_OK; 

  MOZ_TIMER_DEBUGLOG(("Start: Parse Time: COtherDTD::CloseBody(), this=%p\n", this));
  START_TIMER();

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
nsresult COtherDTD::OpenForm(const nsIParserNode *aNode){
  if(mHasOpenForm)
    CloseForm(aNode);

  STOP_TIMER();
  MOZ_TIMER_DEBUGLOG(("Stop: Parse Time: COtherDTD::OpenForm(), this=%p\n", this));

  nsresult result=(mSink) ? mSink->OpenForm(*aNode) : NS_OK; 

  MOZ_TIMER_DEBUGLOG(("Start: Parse Time: COtherDTD::OpenForm(), this=%p\n", this));
  START_TIMER();

  if(NS_OK==result)
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
nsresult COtherDTD::CloseForm(const nsIParserNode *aNode){
//  NS_PRECONDITION(mBodyContext->GetCount() > 0, kInvalidTagStackPos);
  nsresult result=NS_OK;
  if(mHasOpenForm) {
    mHasOpenForm=PR_FALSE;

    STOP_TIMER();
    MOZ_TIMER_DEBUGLOG(("Stop: Parse Time: COtherDTD::CloseForm(), this=%p\n", this));

    result=(mSink) ? mSink->CloseForm(*aNode) : NS_OK; 

    MOZ_TIMER_DEBUGLOG(("Start: Parse Time: COtherDTD::CloseForm(), this=%p\n", this));
    START_TIMER();

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
nsresult COtherDTD::OpenMap(const nsIParserNode *aNode){
  if(mHasOpenMap)
    CloseMap(aNode);

  STOP_TIMER();
  MOZ_TIMER_DEBUGLOG(("Stop: Parse Time: COtherDTD::OpenMap(), this=%p\n", this));

  nsresult result=(mSink) ? mSink->OpenMap(*aNode) : NS_OK; 

  MOZ_TIMER_DEBUGLOG(("Start: Parse Time: COtherDTD::OpenMap(), this=%p\n", this));
  START_TIMER();

  if(NS_OK==result) {
    mBodyContext->Push(aNode);
    mHasOpenMap=PR_TRUE;
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
nsresult COtherDTD::CloseMap(const nsIParserNode *aNode){
//  NS_PRECONDITION(mBodyContext->GetCount() > 0, kInvalidTagStackPos);
  nsresult result=NS_OK;
  if(mHasOpenMap) {
    mHasOpenMap=PR_FALSE;

    STOP_TIMER();
    MOZ_TIMER_DEBUGLOG(("Stop: Parse Time: COtherDTD::CloseMap(), this=%p\n", this));

    result=(mSink) ? mSink->CloseMap(*aNode) : NS_OK; 

    MOZ_TIMER_DEBUGLOG(("Start: Parse Time: COtherDTD::CloseMap(), this=%p\n", this));
    START_TIMER();

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
nsresult COtherDTD::OpenFrameset(const nsIParserNode *aNode){
  NS_PRECONDITION(mBodyContext->GetCount() >= 0, kInvalidTagStackPos);

  mHadFrameset=PR_TRUE;

  STOP_TIMER();
  MOZ_TIMER_DEBUGLOG(("Stop: Parse Time: COtherDTD::OpenFrameset(), this=%p\n", this));

  nsresult result=(mSink) ? mSink->OpenFrameset(*aNode) : NS_OK; 

  MOZ_TIMER_DEBUGLOG(("Start: Parse Time: COtherDTD::OpenFrameset(), this=%p\n", this));
  START_TIMER();

  mBodyContext->Push(aNode);
  mHadFrameset=PR_TRUE;
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
nsresult COtherDTD::CloseFrameset(const nsIParserNode *aNode){
//  NS_PRECONDITION(mBodyContext->GetCount() > 0, kInvalidTagStackPos);

  STOP_TIMER();
  MOZ_TIMER_DEBUGLOG(("Stop: Parse Time: COtherDTD::CloseFrameset(), this=%p\n", this));

  nsresult result=(mSink) ? mSink->CloseFrameset(*aNode) : NS_OK; 

  MOZ_TIMER_DEBUGLOG(("Start: Parse Time: COtherDTD::CloseFrameset(), this=%p\n", this));
  START_TIMER();

  return result;
}



/**
 * This method does two things: 1st, help construct
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 * @update  gess4/6/98
 * @param   aNode -- next node to be added to model
 * @param   aClosedByStartTag -- ONLY TRUE if the container is being closed by opening of another container.
 * @return  TRUE if ok, FALSE if error
 */
nsresult
COtherDTD::OpenContainer(const nsIParserNode *aNode,eHTMLTags aTag,PRBool aClosedByStartTag,nsEntryStack* aStyleStack){
  NS_PRECONDITION(mBodyContext->GetCount() >= 0, kInvalidTagStackPos);
  
  nsresult   result=NS_OK; 
  PRBool     isDefaultNode=PR_FALSE;

  switch(aTag) {

    case eHTMLTag_html:
      result=OpenHTML(aNode); break;

    case eHTMLTag_head:
     // result=OpenHead(aNode); //open the head...
      result=OpenHead(aNode); 
      break;

    case eHTMLTag_body:
      result=OpenBody(aNode); 
      break;

    case eHTMLTag_style:
    case eHTMLTag_title:
      break;

    case eHTMLTag_textarea:
      result=AddLeaf(aNode);
      break;

    case eHTMLTag_map:
      result=OpenMap(aNode);
      break;

    case eHTMLTag_form:
      result=OpenForm(aNode); break;

    case eHTMLTag_frameset:
      if(mHasOpenHead)
        mHasOpenHead=1;
      CloseHead(aNode); //do this just in case someone left it open...
      result=OpenFrameset(aNode); break;

    case eHTMLTag_script:
      if(mHasOpenHead)
        mHasOpenHead=1;
      CloseHead(aNode); //do this just in case someone left it open...
      result=HandleScriptToken(aNode);
      break;

    default:
      isDefaultNode=PR_TRUE;
      break;
  }

  if(isDefaultNode) {

      STOP_TIMER();
      MOZ_TIMER_DEBUGLOG(("Stop: Parse Time: COtherDTD::OpenContainer(), this=%p\n", this));

      result=(mSink) ? mSink->OpenContainer(*aNode) : NS_OK; 

      MOZ_TIMER_DEBUGLOG(("Start: Parse Time: COtherDTD::OpenContainer(), this=%p\n", this));
      START_TIMER();

      mBodyContext->Push(aNode,aStyleStack);
  }

  return result;
}

/**
 * This method does two things: 1st, help construct
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 * @update  gess4/6/98
 * @param   aNode -- next node to be removed from our model
 * @param   aTag  -- id of tag to be closed
 * @param   aClosedByStartTag -- ONLY TRUE if the container is being closed by opening of another container.
 * @return  TRUE if ok, FALSE if error
 */
nsresult
COtherDTD::CloseContainer(const nsIParserNode *aNode,eHTMLTags aTarget,PRBool aClosedByStartTag){
  nsresult   result=NS_OK;
  eHTMLTags nodeType=(eHTMLTags)aNode->GetNodeType();

#ifdef ENABLE_CRC
  #define K_CLOSEOP 200
  CRCStruct theStruct(nodeType,K_CLOSEOP);
  mComputedCRC32=AccumulateCRC(mComputedCRC32,(char*)&theStruct,sizeof(theStruct));
#endif

  switch(nodeType) {

    case eHTMLTag_html:
      result=CloseHTML(aNode); break;

    case eHTMLTag_style:
    case eHTMLTag_textarea:
      break;

    case eHTMLTag_head:
      result=CloseHead(aNode); 
      break;

    case eHTMLTag_body:
      result=CloseBody(aNode); 
      break;

    case eHTMLTag_map:
      result=CloseMap(aNode);
      break;

    case eHTMLTag_form:
      result=CloseForm(aNode); 
      break;

    case eHTMLTag_frameset:
      result=CloseFrameset(aNode); 
      break;

    case eHTMLTag_title:
    default:

      STOP_TIMER();
      MOZ_TIMER_DEBUGLOG(("Stop: Parse Time: COtherDTD::CloseContainer(), this=%p\n", this));

      result=(mSink) ? mSink->CloseContainer(*aNode) : NS_OK; 

      MOZ_TIMER_DEBUGLOG(("Start: Parse Time: COtherDTD::CloseContainer(), this=%p\n", this));
      START_TIMER();

      break;
  }

  return result;
}

/**
 * This method does two things: 1st, help construct
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 * @update  gess4/6/98
 * @param   anIndex
 * @param   aTag
 * @param   aClosedByStartTag -- if TRUE, then we're closing something because a start tag caused it
 * @return  TRUE if ok, FALSE if error
 */
nsresult COtherDTD::CloseContainersTo(PRInt32 anIndex,eHTMLTags aTarget, PRBool aClosedByStartTag){
  NS_PRECONDITION(mBodyContext->GetCount() > 0, kInvalidTagStackPos);
  nsresult result=NS_OK;
  

  if((anIndex<mBodyContext->GetCount()) && (anIndex>=0)) {

    while(mBodyContext->GetCount()>anIndex) {

      eHTMLTags     theTag=mBodyContext->Last();
      nsEntryStack  *theChildStyleStack=0;      
      nsCParserNode *theNode=(nsCParserNode*)mBodyContext->Pop(theChildStyleStack);

      if(theNode) {
        result=CloseContainer(theNode,aTarget,aClosedByStartTag);
        
      }//if anode
      RecycleNode(theNode);
    }

  } //if
  return result;
}

/**
 * This method does two things: 1st, help construct
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 * @update  gess4/6/98
 * @param   aTag --
 * @param   aClosedByStartTag -- ONLY TRUE if the container is being closed by opening of another container.
 * @return  TRUE if ok, FALSE if error
 */
nsresult COtherDTD::CloseContainersTo(eHTMLTags aTarget,PRBool aClosedByStartTag){
  NS_PRECONDITION(mBodyContext->GetCount() > 0, kInvalidTagStackPos);

  nsresult result=NS_OK;
  return result;
}

/**
 * This method does two things: 1st, help construct
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 * @update  gess4/6/98
 * @param   aNode -- next node to be added to model
 * @return  error code; 0 means OK
 */
nsresult COtherDTD::AddLeaf(const nsIParserNode *aNode){
  nsresult result=NS_OK;
  
  if(mSink){
    eHTMLTags theTag=(eHTMLTags)aNode->GetNodeType();

    STOP_TIMER();
    MOZ_TIMER_DEBUGLOG(("Stop: Parse Time: COtherDTD::AddLeaf(), this=%p\n", this));
    
    result=mSink->AddLeaf(*aNode);
    
    if(NS_SUCCEEDED(result)) {
      PRBool done=PR_FALSE;
      eHTMLTags thePrevTag=theTag;
      nsCParserNode theNode;

      while(!done && NS_SUCCEEDED(result)) {
        CToken*   theToken=mTokenizer->PeekToken();
        if(theToken) {
          theTag=(eHTMLTags)theToken->GetTypeID();
          switch(theTag) {
            case eHTMLTag_newline:
              mLineNumber++;
            case eHTMLTag_whitespace:
              {
                theToken=mTokenizer->PopToken();
                theNode.Init(theToken,mLineNumber,0);

                STOP_TIMER();
                MOZ_TIMER_DEBUGLOG(("Stop: Parse Time: COtherDTD::AddLeaf(), this=%p\n", this));
              
                result=mSink->AddLeaf(theNode);

                if((NS_SUCCEEDED(result))||(NS_ERROR_HTMLPARSER_BLOCK==result)) {
                  if(mTokenRecycler) {
                     mTokenRecycler->RecycleToken(theToken);
                  }
                  else delete theToken;
                }
                
                MOZ_TIMER_DEBUGLOG(("Start: Parse Time: COtherDTD::AddLeaf(), this=%p\n", this));
                START_TIMER();
                thePrevTag=theTag;
              }
              break; 

            case eHTMLTag_text:
              if((mHasOpenBody) && (!mHasOpenHead) &&
                !(gElementTable->mElements[thePrevTag]->IsWhiteSpaceTag())) {
                theToken=mTokenizer->PopToken();
                theNode.Init(theToken,mLineNumber);

                MOZ_TIMER_DEBUGLOG(("Stop: Parse Time: COtherDTD::AddLeaf(), this=%p\n", this));
                STOP_TIMER();

                mLineNumber += theToken->mNewlineCount;
                result=mSink->AddLeaf(theNode);

                if((NS_SUCCEEDED(result))||(NS_ERROR_HTMLPARSER_BLOCK==result)) {
                  if(mTokenRecycler) {
                     mTokenRecycler->RecycleToken(theToken);
                  }
                  else delete theToken;
                }
              
                MOZ_TIMER_DEBUGLOG(("Start: Parse Time: COtherDTD::AddLeaf(), this=%p\n", this));
                START_TIMER();
              }
              else done=PR_TRUE;
              break;

            default:
              done=PR_TRUE;
          } //switch
        }//if
        else done=PR_TRUE;
      } //while
    } //if

    MOZ_TIMER_DEBUGLOG(("Start: Parse Time: COtherDTD::AddLeaf(), this=%p\n", this));
    START_TIMER();

  }
  return result;
}

/**
 * Call this method ONLY when you want to write a leaf
 * into the head container.
 * 
 * @update  gess 03/14/99
 * @param   aNode -- next node to be added to model
 * @return  error code; 0 means OK
 */
nsresult COtherDTD::AddHeadLeaf(nsIParserNode *aNode){
  nsresult result=NS_OK;

  static eHTMLTags gNoXTags[]={eHTMLTag_noembed,eHTMLTag_noframes,eHTMLTag_nolayer,eHTMLTag_noscript};
  
  //this code has been added to prevent <meta> tags from being processed inside
  //the document if the <meta> tag itself was found in a <noframe>, <nolayer>, or <noscript> tag.
  eHTMLTags theTag=(eHTMLTags)aNode->GetNodeType();

  if(eHTMLTag_meta==theTag)
    if(HasOpenContainer(gNoXTags,sizeof(gNoXTags)/sizeof(eHTMLTag_unknown))) {
      return result;
    }

  if(mSink) {
    result=OpenHead(aNode);
    if(NS_OK==result) {
      if(eHTMLTag_title==theTag) {
      }
      else result=AddLeaf(aNode);
        // XXX If the return value tells us to block, go
        // ahead and close the tag out anyway, since its
        // contents will be consumed.
      if (NS_SUCCEEDED(result)) {
        nsresult rv=CloseHead(aNode);
        // XXX Only send along a failure. If the close 
        // succeeded we still may need to indicate that the
        // parser has blocked (i.e. return the result of
        // the AddLeaf.
        if (rv != NS_OK) {
          result = rv;
        }
      }
    }  
  }

  return result;
}

 
/**
 * Retrieve the preferred tokenizer for use by this DTD.
 * @update  gess12/28/98
 * @param   none
 * @return  ptr to tokenizer
 */
nsresult COtherDTD::GetTokenizer(nsITokenizer*& aTokenizer) {
  nsresult result=NS_OK;
  if(!mTokenizer) {
    result=NS_NewHTMLTokenizer(&mTokenizer,mParseMode,mDocType,mParserCommand);
  }
  aTokenizer=mTokenizer;
  return result;
}
 

/**
 * 
 * @update  gess8/4/98
 * @param 
 * @return
 */
nsITokenRecycler* COtherDTD::GetTokenRecycler(void){
  if(!mTokenRecycler) {
    nsresult result=GetTokenizer(mTokenizer);
    if (NS_SUCCEEDED(result)) {
      mTokenRecycler=(CTokenRecycler*)mTokenizer->GetTokenRecycler();
    }
  } 
  return mTokenRecycler;
}
 
/**
 * 
 * @update  gess5/18/98 
 * @param 
 * @return 
 */
nsresult COtherDTD::WillResumeParse(void) {

  STOP_TIMER();
  MOZ_TIMER_DEBUGLOG(("Stop: Parse Time: COtherDTD::WillResumeParse(), this=%p\n", this));

  nsresult result=(mSink) ? mSink->WillResume() : NS_OK; 

  MOZ_TIMER_DEBUGLOG(("Start: Parse Time: COtherDTD::WillResumeParse(), this=%p\n", this));
  START_TIMER();

  return result;
}

/**
 * This method gets called when the parsing process is interrupted
 * due to lack of data (waiting for netlib).
 * @update  gess5/18/98
 * @return  error code
 */ 
nsresult COtherDTD::WillInterruptParse(void){ 

  STOP_TIMER();
  MOZ_TIMER_DEBUGLOG(("Stop: Parse Time: COtherDTD::WillInterruptParse(), this=%p\n", this));

  nsresult result=(mSink) ? mSink->WillInterrupt() : NS_OK; 

  MOZ_TIMER_DEBUGLOG(("Start: Parse Time: COtherDTD::WillInterruptParse(), this=%p\n", this));
  START_TIMER();

  return result;
}

#endif /* From #if 0 at the begining */

