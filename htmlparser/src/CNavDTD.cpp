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
//#define ALLOW_TR_AS_CHILD_OF_TABLE  //by setting this to true, TR is allowable directly in TABLE.

#define ENABLE_RESIDUALSTYLE  

     
#include "nsDebug.h" 
#include "nsIDTDDebug.h"  
#include "CNavDTD.h" 
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
#include "nsIFormProcessor.h"
#include "nsVoidArray.h"

#include "prmem.h"


static NS_DEFINE_IID(kIHTMLContentSinkIID, NS_IHTML_CONTENT_SINK_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);                 
static NS_DEFINE_IID(kIDTDIID,      NS_IDTD_IID);
static NS_DEFINE_IID(kClassIID,     NS_INAVHTML_DTD_IID); 

static NS_DEFINE_CID(kFormProcessorCID, NS_FORMPROCESSOR_CID); 
 
static const  char* kNullToken = "Error: Null token given";
static const  char* kInvalidTagStackPos = "Error: invalid tag stack position";
static char*        kVerificationDir = "c:/temp";


#ifdef  ENABLE_CRC
static char gShowCRC;
#endif

#include "nsElementTable.h"


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

/************************************************************************
  And now for the main class -- CNavDTD...
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
nsresult CNavDTD::QueryInterface(const nsIID& aIID, void** aInstancePtr)  
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
    *aInstancePtr = (CNavDTD*)(this);                                        
  }                 
  else {
    *aInstancePtr=0;
    return NS_NOINTERFACE;
  }
  NS_ADDREF_THIS();
  return NS_OK;                                                        
}

NS_IMPL_ADDREF(CNavDTD)
NS_IMPL_RELEASE(CNavDTD)

/**
 *  Default constructor
 *   
 *  @update  gess 4/9/98
 *  @param   
 *  @return  
 */
CNavDTD::CNavDTD() : nsIDTD(), 
    mMisplacedContent(0), 
    mSkippedContent(0), 
    mSharedNodes(0) {
  NS_INIT_REFCNT();
  mSink = 0; 
  mParser=0;       
  mDTDDebug=0;   
  mLineNumber=1;  
  mHasOpenBody=PR_FALSE;
  mHasOpenHead=0;
  mHasOpenForm=PR_FALSE;
  mHasOpenMap=PR_FALSE;
  mHasOpenNoXXX=0;
  mFormContext=0;
  mMapContext=0;
  mTempContext=0;
  mTokenizer=0;
  mComputedCRC32=0;
  mExpectedCRC32=0;
  mDTDState=NS_OK;
  mStyleHandlingEnabled=PR_TRUE;
  mDocType=eHTML3Text;
  mRequestedHead=PR_FALSE;
  mIsFormContainer=PR_FALSE;

  mHeadContext=new nsDTDContext();
  mBodyContext=new nsDTDContext();

  if(!gHTMLElements) {
    InitializeElementTable();
  }

  mNodeRecycler=0;

#ifdef  RICKG_DEBUG
  //DebugDumpContainmentRules2(*this,"c:/temp/DTDRules.new","New CNavDTD Containment Rules");
  nsHTMLElement::DebugDumpContainment("c:/temp/contain.new","ElementTable Rules");
  nsHTMLElement::DebugDumpMembership("c:/temp/membership.out");
  nsHTMLElement::DebugDumpContainType("c:/temp/ctnrules.out");
#endif
}

void CNavDTD::ReleaseTable(void) {
  if(gHTMLElements) {
    delete [] gHTMLElements;  //fixed bug 49564
    gHTMLElements=0;
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
void CNavDTD::RecycleNodes(nsEntryStack *aNodeStack) {
  if(aNodeStack) {
    PRInt32 theCount=aNodeStack->mCount;
    PRInt32 theIndex=0;

    for(theIndex=0;theIndex<theCount;theIndex++) {
      nsCParserNode* theNode=(nsCParserNode*)aNodeStack->NodeAt(theIndex);
      if(theNode) {

        theNode->mUseCount=0;

        IF_FREE(theNode->mToken);

        CToken* theToken=0;
        while((theToken=(CToken*)theNode->PopAttributeToken())){
          IF_FREE(theToken); 
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
const nsIID& CNavDTD::GetMostDerivedIID(void)const {
  return kClassIID;
}
 
/**
 *  Default destructor
 *  
 *  @update  gess 4/9/98
 *  @param   
 *  @return  
 */
CNavDTD::~CNavDTD(){
  if(mHeadContext) {
    delete mHeadContext;
    mHeadContext=0;
  }
  
  if(mBodyContext) {
    delete mBodyContext;
    mBodyContext=0;
  }

  NS_IF_RELEASE(mTokenizer);

  if(mTempContext) {
    delete mTempContext;
    mTempContext=0;
  }

 // delete mNodeRecycler;

  NS_IF_RELEASE(mSink);
  NS_IF_RELEASE(mDTDDebug);
}
 

/**
 * Call this method if you want the DTD to construct a fresh 
 * instance of itself. 
 * @update  gess 25May2000
 * @param 
 * @return
 */
nsresult CNavDTD::CreateNewInstance(nsIDTD** aInstancePtrResult){

  nsresult result=NS_NewNavHTMLDTD(aInstancePtrResult);

  if(aInstancePtrResult) {
    CNavDTD *theOtherDTD=(CNavDTD*)*aInstancePtrResult;
    if(theOtherDTD) {
      theOtherDTD->mDTDMode=mDTDMode;
      theOtherDTD->mParserCommand=mParserCommand;
      theOtherDTD->mDocType=mDocType;
    }
  }

  return result;
}

/**
 * Called by the parser to initiate dtd verification of the
 * internal context stack.
 * @update  gess 7/23/98
 * @param 
 * @return
 */
PRBool CNavDTD::Verify(nsString& aURLRef,nsIParser* aParser){
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
 * @update  gess 02/24/00
 * @param   
 * @return  TRUE if this DTD can satisfy the request; FALSE otherwise.
 */ 
eAutoDetectResult CNavDTD::CanParse(CParserContext& aParserContext,nsString& aBuffer, PRInt32 aVersion) {
  eAutoDetectResult result=eUnknownDetect;

  if(eViewSource==aParserContext.mParserCommand) {
    if(PR_TRUE==aParserContext.mMimeType.EqualsWithConversion(kPlainTextContentType)) {
      result=ePrimaryDetect;
    }
    else if(aParserContext.mMimeType.EqualsWithConversion(kRTFTextContentType)){ 
      result=ePrimaryDetect;
    }
  }     
  else {
    if(PR_TRUE==aParserContext.mMimeType.EqualsWithConversion(kHTMLTextContentType)) {
      result=ePrimaryDetect;
    }
    else if(PR_TRUE==aParserContext.mMimeType.EqualsWithConversion(kPlainTextContentType)) {
      result=ePrimaryDetect;
    } 
    else {
      //otherwise, look into the buffer to see if you recognize anything...
      PRBool theBufHasXML=PR_FALSE;
      if(BufferContainsHTML(aBuffer,theBufHasXML)){
        result = eValidDetect ;
        if(0==aParserContext.mMimeType.Length()) {
          aParserContext.SetMimeType(NS_ConvertToString(kHTMLTextContentType));
          if(!theBufHasXML) {
            switch(aParserContext.mDTDMode) {
              case eDTDMode_strict:
              case eDTDMode_transitional:
                result=eValidDetect;
                break;
              default:
                result=ePrimaryDetect;
                break;
            }
          }
          else result=eValidDetect;
        }
      }
    } 
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
nsresult CNavDTD::WillBuildModel(  const CParserContext& aParserContext,nsIContentSink* aSink) {
  nsresult result=NS_OK;

  mFilename=aParserContext.mScanner->GetFilename();
  mHasOpenBody=PR_FALSE;
  mHadBody=PR_FALSE;
  mHadFrameset=PR_FALSE;
  mLineNumber=1;
  mHasOpenScript=PR_FALSE;
  mHasOpenNoXXX=0;
  mDTDMode=aParserContext.mDTDMode;
  mParserCommand=aParserContext.mParserCommand;
  mStyleHandlingEnabled=(eDTDMode_quirks==mDTDMode);
  mRequestedHead=PR_FALSE;
  mMimeType=aParserContext.mMimeType;
    
  if((!aParserContext.mPrevContext) && (aSink)) {

    STOP_TIMER();
    MOZ_TIMER_DEBUGLOG(("Stop: Parse Time: CNavDTD::WillBuildModel(), this=%p\n", this));

    mBodyContext->ResetCounters();
    mDocType=aParserContext.mDocType;
    
    mStyleHandlingEnabled=PR_TRUE;

    if(aSink && (!mSink)) {
      result=aSink->QueryInterface(kIHTMLContentSinkIID, (void **)&mSink);
    }

    if(result==NS_OK) {
      result = aSink->WillBuildModel();

      MOZ_TIMER_DEBUGLOG(("Start: Parse Time: CNavDTD::WillBuildModel(), this=%p\n", this));
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
nsresult CNavDTD::BuildModel(nsIParser* aParser,nsITokenizer* aTokenizer,nsITokenObserver* anObserver,nsIContentSink* aSink) {
  NS_PRECONDITION(mBodyContext!=nsnull,"Create a context before calling build model");

  nsresult result=NS_OK;

  if(aTokenizer) {
    nsITokenizer*  oldTokenizer=mTokenizer;
    mTokenizer=aTokenizer;
    mParser=(nsParser*)aParser;

    if(mTokenizer) {
      
      mTokenAllocator=mTokenizer->GetTokenAllocator();

      result=mBodyContext->GetNodeRecycler(mNodeRecycler); // Get a copy...

      if(NS_FAILED(result)) return result;

      if(mSink) {

        if(!mBodyContext->GetCount()) {
          CStartToken* theToken=nsnull;
          if(ePlainText==mDocType) {
            //we do this little trick for text files, in both normal and viewsource mode...
            theToken=(CStartToken*)mTokenAllocator->CreateTokenOfType(eToken_start,eHTMLTag_pre);
            if(theToken) {
              mTokenizer->PushTokenFront(theToken);
            }
          }
            //if the content model is empty, then begin by opening <html>...
          theToken=(CStartToken*)mTokenAllocator->CreateTokenOfType(eToken_start,eHTMLTag_html,NS_ConvertToString("html"));
          if(theToken) {
            mTokenizer->PushTokenFront(theToken); //this token should get pushed on the context stack.
          }
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
nsresult CNavDTD::DidBuildModel(nsresult anErrorCode,PRBool aNotifySink,nsIParser* aParser,nsIContentSink* aSink){
  nsresult result=NS_OK;

  if(aSink) { 

    if((NS_OK==anErrorCode) && (!mHadBody) && (!mHadFrameset)) { 

      mSkipTarget=eHTMLTag_unknown; //clear this in case we were searching earlier.

      CStartToken *theToken=(CStartToken*)mTokenAllocator->CreateTokenOfType(eToken_start,eHTMLTag_body,NS_ConvertToString("body"));
      mTokenizer->PushTokenFront(theToken); //this token should get pushed on the context stack, don't recycle it 
      result=BuildModel(aParser,mTokenizer,0,aSink);
    } 

    if(aParser && (NS_OK==result)){ 
      if(aNotifySink){ 
        if((NS_OK==anErrorCode) && (mBodyContext->GetCount()>0)) {
          if(mSkipTarget) {
            CHTMLToken* theEndToken=nsnull;
            theEndToken=(CHTMLToken*)mTokenAllocator->CreateTokenOfType(eToken_end,mSkipTarget);
            if(theEndToken) {
              result=HandleToken(theEndToken,mParser);
            }
          }
          if(result==NS_OK) {
            eHTMLTags theTarget; 

              //now let's disable style handling to save time when closing remaining stack members...
            mStyleHandlingEnabled=PR_FALSE;

            while(mBodyContext->GetCount() > 0) { 
              theTarget = mBodyContext->Last(); 
              CloseContainersTo(theTarget,PR_FALSE); 
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
            mNodeRecycler->RecycleNode(theNode);
            if(theChildStyles) {
              delete theChildStyles;
            }
          } 

        }

        STOP_TIMER();
        MOZ_TIMER_DEBUGLOG(("Stop: Parse Time: CNavDTD::DidBuildModel(), this=%p\n", this));

#ifdef  ENABLE_CRC

          //let's only grab this state once! 
        if(!gShowCRC) { 
          gShowCRC=1; //this only indicates we'll not initialize again. 
          char* theEnvString = PR_GetEnv("RICKG_CRC"); 
          if(theEnvString){ 
            if(('1'==theEnvString[0]) || ('Y'==theEnvString[0]) || ('y'==theEnvString[0])){ 
              gShowCRC=2;  //this indicates that the CRC flag was found in the environment. 
            } 
          } 
        } 

        if(2==gShowCRC) { 
          if(mComputedCRC32!=mExpectedCRC32) { 
            if(mExpectedCRC32!=0) { 
              printf("CRC Computed: %u  Expected CRC: %u\n,",mComputedCRC32,mExpectedCRC32); 
              result = aSink->DidBuildModel(2); 
            } 
            else { 
              printf("Computed CRC: %u.\n",mComputedCRC32); 
              result = aSink->DidBuildModel(3); 
            } 
          } 
          else result = aSink->DidBuildModel(0); 
        } 
        else result=aSink->DidBuildModel(0); 
#endif

        MOZ_TIMER_DEBUGLOG(("Start: Parse Time: CNavDTD::DidBuildModel(), this=%p\n", this));
        START_TIMER();

          //Now make sure the misplaced content list is empty,
          //by forcefully recycling any tokens we might find there.

        CToken* theToken=0;
        while((theToken=(CToken*)mMisplacedContent.Pop())) {
          IF_FREE(theToken);
        }

        if(mDTDDebug) { 
          mDTDDebug->DumpVectorRecord(); 
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
nsresult CNavDTD::HandleToken(CToken* aToken,nsIParser* aParser){
  nsresult  result=NS_OK;

  if(aToken) {
    CHTMLToken*     theToken= (CHTMLToken*)(aToken);
    eHTMLTokenTypes theType=eHTMLTokenTypes(theToken->GetTokenType());
    eHTMLTags       theTag=(eHTMLTags)theToken->GetTypeID();
    PRBool          execSkipContent=PR_FALSE;

    /* ---------------------------------------------------------------------------------
       To understand this little piece of code, you need to look below too.
       In essence, this code caches "skipped content" until we find a given skiptarget.
       Once we find the skiptarget, we take all skipped content up to that point and
       coallate it. Then we push those tokens back onto the tokenizer deque.
       ---------------------------------------------------------------------------------
     */

      // printf("token: %p\n",aToken);

     if(mSkipTarget){  //handle a preexisting target...
      if((theTag==mSkipTarget) && (eToken_end==theType)){
        mSkipTarget=eHTMLTag_unknown; //stop skipping.  
        //mTokenizer->PushTokenFront(aToken); //push the end token...
        execSkipContent=PR_TRUE;
        IF_FREE(aToken);
        theToken=(CHTMLToken*)mSkippedContent.PopFront();
        theType=eToken_start;
      }
      else {
        mSkippedContent.Push(theToken);
        return result;
      }
    }

    /* ---------------------------------------------------------------------------------
       This section of code is used to "move" misplaced content from one location in 
       our document model to another. (Consider what would happen if we found a <P> tag
       and text in the head.) To move content, we throw it onto the misplacedcontent 
       deque until we can deal with it.
       ---------------------------------------------------------------------------------
     */
    if(!execSkipContent && mDTDState!=NS_HTMLPARSER_ALTERNATECONTENT) {

      switch(theTag) {
        case eHTMLTag_html:
        case eHTMLTag_noscript:
        case eHTMLTag_script:
        case eHTMLTag_markupDecl:
          break;  // simply pass these through to token handler without further ado...
        case eHTMLTag_comment:
        case eHTMLTag_newline:
        case eHTMLTag_whitespace:
          if(mMisplacedContent.GetSize()<=0) // fix for bugs 17017,18308,23765, and 24275
            break;                           // Push only when it's absolutely necessary.
        default:
          if(!gHTMLElements[eHTMLTag_html].SectionContains(theTag,PR_FALSE)) {
            if((!mHadBody) && (!mHadFrameset)){

              //For bug examples from this code, see bugs: 18928, 20989.

              //At this point we know the body/frameset aren't open. 
              //If the child belongs in the head, then handle it (which may open the head);
              //otherwise, push it onto the misplaced stack.

              PRBool theExclusive=PR_FALSE;
              PRBool theChildBelongsInHead=gHTMLElements[eHTMLTag_head].IsChildOfHead(theTag,theExclusive);
              if(!theChildBelongsInHead) {

                //If you're here then we found a child of the body that was out of place.
                //We're going to move it to the body by storing it temporarily on the misplaced stack.
                //However, in quirks mode, a few tags request, ambiguosly, for a BODY. - Bugs 18928, 24204.-
                mMisplacedContent.Push(aToken);
                if(mDTDMode==eDTDMode_quirks && (gHTMLElements[theTag].HasSpecialProperty(kRequiresBody))) {
                  CToken* theBodyToken=(CToken*)mTokenAllocator->CreateTokenOfType(eToken_start,eHTMLTag_body,NS_ConvertToString("body"));
                  result=HandleToken(theBodyToken,aParser);
                }
                return result;
              }

            } //if
          } //if
      }//switch

    } //if
    
    if(theToken){
      //Before dealing with the token normally, we need to deal with skip targets
      if((!execSkipContent) && 
         (theType!=eToken_end) &&
         (eHTMLTag_unknown==mSkipTarget) && 
         (gHTMLElements[theTag].mSkipTarget)){ //create a new target
        // Ref: Bug# 19977
        // For optimization, determine if the skipped content is well
        // placed. This would avoid unnecessary node creation and 
        // extra string append. BTW, watch out in handling the head
        // children ( especially the TITLE tag).
        PRBool theExclusive=PR_FALSE;
        if(!gHTMLElements[eHTMLTag_head].IsChildOfHead(theTag,theExclusive)) {
          eHTMLTags theParentTag      = mBodyContext->Last();
          PRBool    theParentContains = -1;
          if(CanOmit(theParentTag,theTag,theParentContains)) {
            result=HandleOmittedTag(theToken,theTag,theParentTag,nsnull);
            return result;
          }
        }
        mSkipTarget=gHTMLElements[theTag].mSkipTarget;
        mSkippedContent.Push(theToken);
      }
      else {

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
           IF_FREE(theToken);
        }
        else if(result==NS_ERROR_HTMLPARSER_STOPPARSING) {
          mDTDState=result;
        }
        else {
          return NS_OK;
        }

#if 0
        if (mDTDDebug) {
          mDTDDebug->Verify(this, mParser, mBodyContext->GetCount(), mBodyContext->mStack, mFilename);
        }
#endif
      }
    }

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
nsresult CNavDTD::DidHandleStartTag(nsCParserNode& aNode,eHTMLTags aChildTag){
  nsresult result=NS_OK;

#if 0
    // XXX --- Ignore this: it's just rickg debug testing...
  nsAutoString theStr;
  aNode.GetSource(theStr);
#endif

  switch(aChildTag){

    case eHTMLTag_pre:
    case eHTMLTag_listing:
      {
        CToken* theNextToken=mTokenizer->PeekToken();
        if(theNextToken)  {
          eHTMLTokenTypes theType=eHTMLTokenTypes(theNextToken->GetTokenType());
          if(eToken_newline==theType){
            mLineNumber++;
            theNextToken=mTokenizer->PopToken();  //skip 1st newline inside PRE and LISTING
            IF_FREE(theNextToken); // fix for Bug 29379
          }//if
        }//if
      }
      break;

    case eHTMLTag_plaintext:
    case eHTMLTag_xmp:
      //grab the skipped content and dump it out as text...
      {        
        STOP_TIMER()
        MOZ_TIMER_DEBUGLOG(("Stop: Parse Time: CNavDTD::DidHandleStartTag(), this=%p\n", this));
        const nsString& theString=aNode.GetSkippedContent();
        if(0<theString.Length()) {
          CTextToken *theToken=(CTextToken*)mTokenAllocator->CreateTokenOfType(eToken_text,eHTMLTag_text,theString);
          nsCParserNode theNode(theToken,0);
          result=mSink->AddLeaf(theNode); //when the node get's destructed, so does the new token
        }
        MOZ_TIMER_DEBUGLOG(("Start: Parse Time: CNavDTD::DidHandleStartTag(), this=%p\n", this));
        START_TIMER()
      }
      break;

    case eHTMLTag_counter:
      {
        PRInt32   theCount=mBodyContext->GetCount();
        eHTMLTags theGrandParentTag=mBodyContext->TagAt(theCount-1);
        
        nsAutoString  theNumber;
        
        mBodyContext->IncrementCounter(theGrandParentTag,aNode,theNumber);

        CTextToken theToken(theNumber);
        PRInt32 theLineNumber=0;
        nsCParserNode theNode(&theToken,theLineNumber);
        result=mSink->AddLeaf(theNode);
      }
      break;

    case eHTMLTag_meta:
      {
          //we should only enable user-defined entities in debug builds...

        PRInt32 theCount=aNode.GetAttributeCount();
        const nsString* theNamePtr=0;
        const nsString* theValuePtr=0;

        if(theCount) {
          PRInt32 theIndex=0;
          for(theIndex=0;theIndex<theCount;theIndex++){
            const nsString& theKey=aNode.GetKeyAt(theIndex);
            if(theKey.EqualsWithConversion("ENTITY",PR_TRUE)) {
              const nsString& theName=aNode.GetValueAt(theIndex);
              theNamePtr=&theName;
            }
            else if(theKey.EqualsWithConversion("VALUE",PR_TRUE)) {
              //store the named enity with the context...
              const nsString& theValue=aNode.GetValueAt(theIndex);
              theValuePtr=&theValue;
            }
          }
        }
        if(theNamePtr && theValuePtr) {
          mBodyContext->RegisterEntity(*theNamePtr,*theValuePtr);
        }
      }
      break; 

    default:
      break;
  }//switch 

    //handle <empty/> tags by generating a close tag...
    //added this to fix bug 48351, which contains XHTML and uses empty tags.
  if(nsHTMLElement::IsContainer(aChildTag) && aNode.mToken) {  //nullptr test fixes bug 56085
    CStartToken *theToken=NS_STATIC_CAST(CStartToken*,aNode.mToken);
    if(theToken->IsEmpty()){

      CToken *theEndToken=mTokenAllocator->CreateTokenOfType(eToken_end,aChildTag); 
      if(theEndToken) {
        result=HandleEndToken(theEndToken);
        IF_FREE(theEndToken);
      }
    }
  }

  return result;
} 
 
/**
 *  Determine whether the given tag is open anywhere
 *  in our context stack.
 *  
 *  @update  gess 4/2/98
 *  @param   eHTMLTags tag to be searched for in stack
 *  @return  topmost index of tag on stack
 */
PRInt32 CNavDTD::LastOf(eHTMLTags aTagSet[],PRInt32 aCount) const {
  int theIndex=0;
  for(theIndex=mBodyContext->GetCount()-1;theIndex>=0;theIndex--){
    if(FindTagInSet((*mBodyContext)[theIndex],aTagSet,aCount)) { 
      return theIndex;
    } 
  } 
  return kNotFound;
}

/**
 *  Call this to find the index of a given child, or (if not found)
 *  the index of its nearest synonym.
 *   
 *  @update  gess 3/25/98
 *  @param   aTagStack -- list of open tags
 *  @param   aTag -- tag to test for containership
 *  @return  index of kNotFound
 */
static
PRInt32 GetIndexOfChildOrSynonym(nsDTDContext& aContext,eHTMLTags aChildTag) {
  PRInt32 theChildIndex=aContext.LastOf(aChildTag);
  if(kNotFound==theChildIndex) {
    TagList* theSynTags=gHTMLElements[aChildTag].GetSynonymousTags(); //get the list of tags that THIS tag can close
    if(theSynTags) {
      theChildIndex=LastOf(aContext,*theSynTags);
    } 
    else{
      PRInt32 theGroup=nsHTMLElement::GetSynonymousGroups(aChildTag);
      if(theGroup) {
        theChildIndex=aContext.GetCount();
        while(-1<--theChildIndex) {
          eHTMLTags theTag=aContext[theChildIndex];
          if(gHTMLElements[theTag].IsMemberOf(theGroup)) {
            break;   
          }
        }
      }
    }
  }
  return theChildIndex;
}

/** 
 *  This method is called to determine whether or not the child
 *  tag is happy being OPENED in the context of the current
 *  tag stack. This is only called if the current parent thinks
 *  it wants to contain the given childtag.
 *    
 *  @param   aChildTag -- tag enum of child to be opened
 *  @param   aTagStack -- ref to current tag stack in DTD.
 *  @return  PR_TRUE if child agrees to be opened here.
 */  
static
PRBool CanBeContained(eHTMLTags aChildTag,nsDTDContext& aContext) {

  /* #    Interesting test cases:       Result:
   * 1.   <UL><LI>..<B>..<LI>           inner <LI> closes outer <LI>
   * 2.   <CENTER><DL><DT><A><CENTER>   allow nested <CENTER>
   * 3.   <TABLE><TR><TD><TABLE>...     allow nested <TABLE>
   * 4.   <FRAMESET> ... <FRAMESET>
   */

  //Note: This method is going away. First we need to get the elementtable to do closures right, and
  //      therefore we must get residual style handling to work.

  //the changes to this method were added to fix bug 54651...

  PRBool  result=PR_TRUE;
  PRInt32 theCount=aContext.GetCount();

  if(0<theCount){
    TagList* theRootTags=gHTMLElements[aChildTag].GetRootTags();
    TagList* theSpecialParents=gHTMLElements[aChildTag].GetSpecialParents();
    if(theRootTags) {
      PRInt32 theRootIndex=LastOf(aContext,*theRootTags);
      PRInt32 theSPIndex=(theSpecialParents) ? LastOf(aContext,*theSpecialParents) : kNotFound;  
      PRInt32 theChildIndex=GetIndexOfChildOrSynonym(aContext,aChildTag);
      PRInt32 theTargetIndex=(theRootIndex>theSPIndex) ? theRootIndex : theSPIndex;

      if((theTargetIndex==theCount-1) ||
        ((theTargetIndex==theChildIndex) && gHTMLElements[aChildTag].CanContainSelf())) {
        result=PR_TRUE;
      }
      else {
        
        result=PR_FALSE;

        PRInt32 theIndex=theCount-1;
        while(theChildIndex<theIndex) {
          eHTMLTags theParentTag=aContext.TagAt(theIndex--);
          if (gHTMLElements[theParentTag].IsMemberOf(kBlockEntity)  || 
              gHTMLElements[theParentTag].IsMemberOf(kHeading)      || 
              gHTMLElements[theParentTag].IsMemberOf(kPreformatted) || 
              gHTMLElements[theParentTag].IsMemberOf(kList)) {
            if(!HasOptionalEndTag(theParentTag)) {
              result=PR_TRUE;
              break;
            }
          }
        }
      }
    }
  }

  return result;

}

enum eProcessRule {eNormalOp,eLetInlineContainBlock};

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
nsresult CNavDTD::HandleDefaultStartToken(CToken* aToken,eHTMLTags aChildTag,nsIParserNode *aNode) {
  NS_PRECONDITION(0!=aToken,kNullToken);

  nsresult  result=NS_OK;

  PRBool  theChildAgrees=PR_TRUE;
  PRInt32 theIndex=mBodyContext->GetCount();
  PRBool  theChildIsContainer=nsHTMLElement::IsContainer(aChildTag);
  PRBool  theParentContains=-1;
  
  do {
 
    eHTMLTags theParentTag=mBodyContext->TagAt(--theIndex);
    theParentContains=CanContain(theParentTag,aChildTag);  //precompute containment, and pass it to CanOmit()...

    if(CanOmit(theParentTag,aChildTag,theParentContains)) {
      result=HandleOmittedTag(aToken,aChildTag,theParentTag,aNode);
      return result;
    }

    eProcessRule theRule=eNormalOp; 

    if((!theParentContains) &&
       (nsHTMLElement::IsResidualStyleTag(theParentTag)) &&
       (IsBlockElement(aChildTag,theParentTag))) {

      if((eHTMLTag_table!=aChildTag) && (eHTMLTag_li!=aChildTag)) {
        nsCParserNode* theParentNode= NS_STATIC_CAST(nsCParserNode*, mBodyContext->PeekNode());
        if(theParentNode->mToken->IsWellFormed()) {
          theRule=eLetInlineContainBlock;
        }
      }
    }

    switch(theRule){

      case eNormalOp:        

        theChildAgrees=PR_TRUE;
        if(theParentContains) {

          eHTMLTags theAncestor=gHTMLElements[aChildTag].mExcludingAncestor;
          if(eHTMLTag_unknown!=theAncestor){
            theChildAgrees=!HasOpenContainer(theAncestor);
          }

          if(theChildAgrees){
            theAncestor=gHTMLElements[aChildTag].mRequiredAncestor;
            if(eHTMLTag_unknown!=theAncestor){
              theChildAgrees=HasOpenContainer(theAncestor);
            }
          }

          if(theChildAgrees && theChildIsContainer) {
            if(theParentTag!=aChildTag) {             
              // Double check the power structure a
              // Note: The bit is currently set on <A> and <LI>.
              if(gHTMLElements[theParentTag].ShouldVerifyHierarchy(aChildTag)){
                PRInt32 theChildIndex=GetIndexOfChildOrSynonym(*mBodyContext,aChildTag);
            
                if((kNotFound<theChildIndex) && (theChildIndex<theIndex)) {
                 
                /*-------------------------------------------------------------------------------------
                   1  Here's a tricky case from bug 22596:  <h5><li><h5>
                      How do we know that the 2nd <h5> should close the <LI> rather than nest inside the <LI>?
                      (Afterall, the <h5> is a legal child of the <LI>).
            
                      The way you know is that there is no root between the two, so the <h5> binds more
                      tightly to the 1st <h5> than to the <LI>.
                   2.  Also, bug 6148 shows this case: <SPAN><DIV><SPAN>
                      From this case we learned not to execute this logic if the parent is a block.
                  
                   3. Fix for 26583
                      Ex. <A href=foo.html><B>foo<A href-bar.html>bar</A></B></A>  <-- A legal HTML
                      In the above example clicking on "foo" or "bar" should link to
                      foo.html or bar.html respectively. That is, the inner <A> should be informed
                      about the presence of an open <A> above <B>..so that the inner <A> can close out
                      the outer <A>. The following code does it for us.
                   
                   4. Fix for 27865 [ similer to 22596 ]. Ex: <DL><DD><LI>one<DD><LI>two
                 -------------------------------------------------------------------------------------*/
                   
                  theChildAgrees=CanBeContained(aChildTag,*mBodyContext);
                } //if
              }//if
            } //if
          } //if
        } //if parentcontains

        if(!(theParentContains && theChildAgrees)) {
          if (!CanPropagate(theParentTag,aChildTag,theParentContains)) { 
            if(theChildIsContainer || (!theParentContains)){ 
              if((!theChildAgrees) && (!gHTMLElements[aChildTag].CanAutoCloseTag(*mBodyContext,aChildTag)) ||
                 (mBodyContext->mContextTopIndex > 0 && theIndex <= mBodyContext->mContextTopIndex)) {
                // Closing the tags above might cause non-compatible results.
                // Ex. <TABLE><TR><TD><TBODY>Text</TD></TR></TABLE>. 
                // In the example above <TBODY> is badly misplaced, but 
                // we should not attempt to close the tags above it, 
                // The safest thing to do is to discard this tag.     
                return result;
              }
              CloseContainersTo(theIndex,aChildTag,PR_TRUE);
            }//if
            else break;
          }//if
          else {
            CreateContextStackFor(aChildTag);
            theIndex=mBodyContext->GetCount();
          }
        }//if
        break;

      case eLetInlineContainBlock:
        theParentContains=theChildAgrees=PR_TRUE; //cause us to fall out of loop and open the block.
        break;

      default:
        break;

    }//switch
  } while(!(theParentContains && theChildAgrees));

  if(theChildIsContainer){
    result=OpenContainer(aNode,aChildTag,PR_TRUE);
  }
  else {  //we're writing a leaf...
    result=AddLeaf(aNode);
  }

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
nsresult CNavDTD::WillHandleStartTag(CToken* aToken,eHTMLTags aTag,nsCParserNode& aNode){
  nsresult result=NS_OK; 
  PRInt32 theAttrCount  = aNode.GetAttributeCount(); 

    //first let's see if there's some skipped content to deal with... 
  if(gHTMLElements[aTag].mSkipTarget) { 
    result=CollectSkippedContent(aNode,theAttrCount); 
  } 

  //this little gem creates a special attribute for the editor team to use.
  //The attribute only get's applied to unknown tags, and is used by ender
  //(during editing) to display a special icon for unknown tags.

  if(eHTMLTag_userdefined==aTag) {
    CAttributeToken* theToken= (CAttributeToken*)mTokenAllocator->CreateTokenOfType(eToken_attribute,aTag);
    if(theToken) {
      theToken->mTextKey.AssignWithConversion("_moz-userdefined");
      aNode.AddAttribute(theToken);    
    }
  }

    /**************************************************************************************
     *
     * Now a little code to deal with bug #49687 (crash when layout stack gets too deep)
     * I've also opened this up to any container (not just inlines): re bug 55095
     *
     **************************************************************************************/

  if(MAX_REFLOW_DEPTH<mBodyContext->GetCount()) {
    return kHierarchyTooDeep;  
  }

  STOP_TIMER()
  MOZ_TIMER_DEBUGLOG(("Stop: Parse Time: CNavDTD::WillHandleStartTag(), this=%p\n", this));

  if(mParser && mDTDState!=NS_HTMLPARSER_ALTERNATECONTENT) {

    CObserverService* theService=mParser->GetObserverService();
    if(theService) {
      const nsISupportsParserBundle*  bundle=mParser->GetParserBundle();
      result=theService->Notify(aTag,aNode,(void*)bundle, NS_ConvertToString(kHTMLTextContentType), mParser);
    }
  }

  MOZ_TIMER_DEBUGLOG(("Start: Parse Time: CNavDTD::WillHandleStartTag(), this=%p\n", this));
  START_TIMER()

  if(NS_SUCCEEDED(result)) {

#ifdef ENABLE_CRC

    STOP_TIMER()

      if(eHTMLTag_meta==aTag) {  
      PRInt32 theCount=aNode.GetAttributeCount(); 
      if(1<theCount){ 
  
        const nsString& theKey=aNode.GetKeyAt(0); 
        if(theKey.Equals("NAME",IGNORE_CASE)) { 
          const nsString& theValue1=aNode.GetValueAt(0); 
          if(theValue1.Equals("\"CRC\"",IGNORE_CASE)) { 
            const nsString& theKey2=aNode.GetKeyAt(1); 
            if(theKey2.Equals("CONTENT",IGNORE_CASE)) { 
              const nsString& theValue2=aNode.GetValueAt(1); 
              PRInt32 err=0; 
              mExpectedCRC32=theValue2.ToInteger(&err); 
            } //if 
          } //if 
        } //else 

      } //if 
    }//if 

    START_TIMER()

#endif

    if(NS_OK==result) {
      result=gHTMLElements[aTag].HasSpecialProperty(kDiscardTag) ? 1 : NS_OK;
    }

      //this code is here to make sure the head is closed before we deal 
      //with any tags that don't belong in the head.
    if(NS_OK==result) {
      if(mHasOpenHead){
        static eHTMLTags skip2[]={eHTMLTag_newline,eHTMLTag_whitespace};
        if(!FindTagInSet(aTag,skip2,sizeof(skip2)/sizeof(eHTMLTag_unknown))){
          PRBool theExclusive=PR_FALSE;
          if(!gHTMLElements[eHTMLTag_head].IsChildOfHead(aTag,theExclusive)){      

              //because this code calls CloseHead() directly, stack-based token/nodes are ok.
            CEndToken     theToken(eHTMLTag_head);
            nsCParserNode theNode(&theToken,mLineNumber);
            result=CloseHead(&theNode);
          }
        }
      }
    }
  }
  return result;
}

static void PushMisplacedAttributes(nsIParserNode& aNode,nsDeque& aDeque,PRInt32& aCount) {
  if(aCount > 0) {
    CToken* theAttrToken=nsnull;
    nsCParserNode* theAttrNode = (nsCParserNode*)&aNode;
    if(theAttrNode) {
      while(aCount){ 
        theAttrToken=theAttrNode->PopAttributeToken();
        if(theAttrToken) {
          aDeque.Push(theAttrToken);
        }
        aCount--;
      }//while
    }//if
  }//if
}

/** 
 *  This method gets called when a start token has been encountered that the parent
 *  wants to omit. 
 *   
 *  @update  gess 3/25/98
 *  @param   aToken -- next (start) token to be handled
 *  @param   aChildTag -- id of the child in question
 *  @param   aParent -- id of the parent in question
 *  @param   aNode -- CParserNode representing this start token
 *  @return  PR_TRUE if all went well; PR_FALSE if error occured
 */
nsresult CNavDTD::HandleOmittedTag(CToken* aToken,eHTMLTags aChildTag,eHTMLTags aParent,nsIParserNode* aNode) {
  NS_PRECONDITION(mBodyContext != nsnull,"need a context to work with");

  nsresult  result=NS_OK;

  //The trick here is to see if the parent can contain the child, but prefers not to.
  //Only if the parent CANNOT contain the child should we look to see if it's potentially a child
  //of another section. If it is, the cache it for later.
  //  1. Get the root node for the child. See if the ultimate node is the BODY, FRAMESET, HEAD or HTML
  PRInt32   theTagCount = mBodyContext->GetCount();

  if(aToken) {
    // Note: The node for a misplaced skipped content is not yet created ( for optimization ) and hence
    //       it's impossible to collect attributes. So, treat the token as though it doesn't have attibutes.
    PRInt32   attrCount   = (gHTMLElements[aChildTag].mSkipTarget)? 0:aToken->GetAttributeCount();
    if((gHTMLElements[aParent].HasSpecialProperty(kBadContentWatch)) &&
       (!nsHTMLElement::IsWhitespaceTag(aChildTag))) {
      eHTMLTags theTag=eHTMLTag_unknown;
      PRInt32   theIndex=kNotFound;
    
      while(theTagCount > 0) {
        theTag = mBodyContext->TagAt(--theTagCount);
        if(!gHTMLElements[theTag].HasSpecialProperty(kBadContentWatch)) {
          if(!gHTMLElements[theTag].CanContain(aChildTag)) break;
          theIndex = theTagCount;
          break;
        }
      }

      if(theIndex>kNotFound) {
        // Avoid TD and TH getting into misplaced content stack
        // because they are legal table elements. -- Ref: Bug# 20797
        static eHTMLTags gLegalElements[]={eHTMLTag_td,eHTMLTag_th};
                  
        mMisplacedContent.Push(aToken);  

        IF_HOLD(aToken);  // Hold on to this token for later use.

        // If the token is attributed then save those attributes too.    
        if(attrCount > 0) PushMisplacedAttributes(*aNode,mMisplacedContent,attrCount);

        while(1){
         
          CToken* theToken=mTokenizer->PeekToken();
          
          if(theToken) {
            theTag=(eHTMLTags)theToken->GetTypeID();
            if(!nsHTMLElement::IsWhitespaceTag(theTag) && theTag!=eHTMLTag_unknown) {
              if((gHTMLElements[theTag].mSkipTarget && theToken->GetTokenType() != eToken_end) ||
                 (FindTagInSet(theTag,gLegalElements,sizeof(gLegalElements)/sizeof(theTag))) ||
                 (gHTMLElements[theTag].HasSpecialProperty(kBadContentWatch))                ||
                 (gHTMLElements[aParent].CanContain(theTag))) {
                  break;
              }            
            }
              
            theToken=mTokenizer->PopToken();
            mMisplacedContent.Push(theToken);

          }
          else break;
        }//while
        if(result==NS_OK) {
          result=HandleSavedTokens(mBodyContext->mContextTopIndex=theIndex);
          mBodyContext->mContextTopIndex=0;
        }
      }//if
    }//if

    if((aChildTag!=aParent) && (gHTMLElements[aParent].HasSpecialProperty(kSaveMisplaced))) {
      
      IF_HOLD(aToken);  // Hold on to this token for later use. Ref Bug. 53695
      
      mMisplacedContent.Push(aToken);
      // If the token is attributed then save those attributes too.
       if(attrCount > 0) PushMisplacedAttributes(*aNode,mMisplacedContent,attrCount);
    }
  }
  return result;
}

/** 
 *  This method gets called when a kegen token is found.
 *   
 *  @update  harishd 05/02/00
 *  @param   aNode -- CParserNode representing keygen
 *  @return  NS_OK if all went well; ERROR if error occured
 */
nsresult CNavDTD::HandleKeyGen(nsIParserNode* aNode) { 
  nsresult result=NS_OK; 

  if(aNode) { 
  
    NS_WITH_SERVICE(nsIFormProcessor, theFormProcessor, kFormProcessorCID, &result) 
  
    if(NS_SUCCEEDED(result)) { 
      PRInt32      theAttrCount=aNode->GetAttributeCount(); 
      nsVoidArray  theContent; 
      nsAutoString theAttribute; 
      nsAutoString theFormType; 
      CToken*      theToken=nsnull; 

      theFormType.AssignWithConversion("select"); 
  
      result=theFormProcessor->ProvideContent(theFormType,theContent,theAttribute); 

      if(NS_SUCCEEDED(result)) {
        nsString* theTextValue=nsnull; 
        PRInt32   theIndex=nsnull; 
  
        if(mTokenizer && mTokenAllocator) {
          // Populate the tokenizer with the fabricated elements in the reverse order 
          // such that <SELECT> is on the top fo the tokenizer followed by <OPTION>s 
          // and </SELECT> 
          theToken=mTokenAllocator->CreateTokenOfType(eToken_end,eHTMLTag_select); 
          mTokenizer->PushTokenFront(theToken); 
  
          for(theIndex=theContent.Count()-1;theIndex>-1;theIndex--) { 
            theTextValue=(nsString*)theContent[theIndex]; 
            theToken=mTokenAllocator->CreateTokenOfType(eToken_text,eHTMLTag_text,*theTextValue); 
            mTokenizer->PushTokenFront(theToken); 
            theToken=mTokenAllocator->CreateTokenOfType(eToken_start,eHTMLTag_option); 
            mTokenizer->PushTokenFront(theToken); 
          } 
  
          // The attribute ( provided by the form processor ) should be a part of the SELECT.
          // Placing the attribute token on the tokenizer to get picked up by the SELECT.
          theToken=mTokenAllocator->CreateTokenOfType(eToken_attribute,eHTMLTag_unknown,theAttribute);

          nsString& theKey=((CAttributeToken*)theToken)->GetKey(); 
          theKey.AssignWithConversion("_moz-type"); 
          mTokenizer->PushTokenFront(theToken); 
  
          // Pop out NAME and CHALLENGE attributes ( from the keygen NODE ) 
          // and place it in the tokenizer such that the attribtues get 
          // sucked into SELECT node. 
          for(theIndex=theAttrCount;theIndex>0;theIndex--) { 
            mTokenizer->PushTokenFront(((nsCParserNode*)aNode)->PopAttributeToken()); 
          } 
  
          theToken=mTokenAllocator->CreateTokenOfType(eToken_start,eHTMLTag_select); 
          // Increament the count because of the additional attribute from the form processor.
          theToken->SetAttributeCount(theAttrCount+1); 
          mTokenizer->PushTokenFront(theToken); 
        }//if(mTokenizer && mTokenAllocator)
      }//if(NS_SUCCEEDED(result))
    }// if(NS_SUCCEEDED(result)) 
  } //if(aNode) 
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
nsresult CNavDTD::HandleStartToken(CToken* aToken) {
  NS_PRECONDITION(0!=aToken,kNullToken);

  #ifdef  RICKG_DEBUG
    WriteTokenToLog(aToken);
  #endif

  //Begin by gathering up attributes...

  nsCParserNode* theNode=mNodeRecycler->CreateNode();
  theNode->Init(aToken,mLineNumber,mTokenAllocator);
  
  eHTMLTags     theChildTag=(eHTMLTags)aToken->GetTypeID();
  PRInt16       attrCount=aToken->GetAttributeCount();
  eHTMLTags     theParent=mBodyContext->Last();
  nsresult      result=(0==attrCount) ? NS_OK : CollectAttributes(*theNode,theChildTag,attrCount);

  if(NS_OK==result) {
    result=WillHandleStartTag(aToken,theChildTag,*theNode);
    if(NS_OK==result) {
      PRBool isTokenHandled =PR_FALSE;
      PRBool theHeadIsParent=PR_FALSE;

      if(nsHTMLElement::IsSectionTag(theChildTag)){
        switch(theChildTag){
          case eHTMLTag_html:
            if(mBodyContext->GetCount()>0) {
              result=OpenContainer(theNode,theChildTag,PR_FALSE);
              isTokenHandled=PR_TRUE;
            }
            break;
          case eHTMLTag_body:
            if(mHasOpenBody) {
              result=OpenContainer(theNode,theChildTag,PR_FALSE);
              isTokenHandled=PR_TRUE;
            }
            break;
          case eHTMLTag_head:
            if(mHadBody || mHadFrameset) {
              result=HandleOmittedTag(aToken,theChildTag,theParent,theNode);
              isTokenHandled=PR_TRUE;
              mRequestedHead=PR_TRUE;
            }
            break;
          default:
            break;
        }
      }

      mLineNumber += aToken->mNewlineCount;
      PRBool theExclusive=PR_FALSE;
      theHeadIsParent=nsHTMLElement::IsChildOfHead(theChildTag,theExclusive);
      
      switch(theChildTag) { 
        case eHTMLTag_newline:
          mLineNumber++;
          break;

        case eHTMLTag_area:
          if(!mHasOpenMap) isTokenHandled=PR_TRUE;

          STOP_TIMER();
          MOZ_TIMER_DEBUGLOG(("Stop: Parse Time: CNavDTD::HandleStartToken(), this=%p\n", this));
          
          if (mHasOpenMap && mSink) {
            result=mSink->AddLeaf(*theNode);
            isTokenHandled=PR_TRUE;
          }
          
          MOZ_TIMER_DEBUGLOG(("Start: Parse Time: CNavDTD::HandleStartToken(), this=%p\n", this));
          START_TIMER();
          break;

        case eHTMLTag_image:
          aToken->SetTypeID(theChildTag=eHTMLTag_img);
          break;

        case eHTMLTag_keygen:
          result=HandleKeyGen(theNode);
          isTokenHandled=PR_TRUE;
          break;

        case eHTMLTag_noframes:
        case eHTMLTag_noembed:
          mHasOpenNoXXX++;
          break;

        case eHTMLTag_script:
          theHeadIsParent=((!mHasOpenBody) || mRequestedHead);
          mHasOpenScript=PR_TRUE;

        default:
            break;
      }//switch

#if 0
        //we'll move to this approach after beta...
      if(!isTokenHandled) {

        if(theHeadIsParent && (theExclusive || mHasOpenHead)) {
          result=AddHeadLeaf(theNode);
        }
        else result=HandleDefaultStartToken(aToken,theChildTag,theNode); 
      }
#else
        //the old way...
      if(!isTokenHandled) {
        if(theHeadIsParent || 
           (mHasOpenHead && ((eHTMLTag_newline==theChildTag) || (eHTMLTag_whitespace==theChildTag)))) {
            result=AddHeadLeaf(theNode);
        }
        else result=HandleDefaultStartToken(aToken,theChildTag,theNode); 
      }
#endif
      //now do any post processing necessary on the tag...
      if(NS_OK==result)
        DidHandleStartTag(*theNode,theChildTag);
    }//if
  } //if

  if(kHierarchyTooDeep==result) {
    //reset this error to ok; all that happens here is that given inline tag
    //gets dropped because the stack is too deep. Don't terminate parsing.
    result=NS_OK;
  }

  mNodeRecycler->RecycleNode(theNode);
  return result;
}

/**
 *  Call this to see if you have a closeable peer on the stack that
 *  is ABOVE one of its root tags.
 *   
 *  @update  gess 4/11/99
 *  @param   aRootTagList -- list of root tags for aTag
 *  @param   aTag -- tag to test for containership
 *  @return  PR_TRUE if given tag can contain other tags
 */
static
PRBool HasCloseablePeerAboveRoot(TagList& aRootTagList,nsDTDContext& aContext,eHTMLTags aTag,PRBool anEndTag) {
  PRInt32   theRootIndex=LastOf(aContext,aRootTagList);  
  TagList*  theCloseTags=(anEndTag) ? gHTMLElements[aTag].GetAutoCloseEndTags() : gHTMLElements[aTag].GetAutoCloseStartTags();
  PRInt32 theChildIndex=-1;

  if(theCloseTags) {
    theChildIndex=LastOf(aContext,*theCloseTags);
  }
  else {
    if((anEndTag) || (!gHTMLElements[aTag].CanContainSelf()))
      theChildIndex=aContext.LastOf(aTag);
  }
    // I changed this to theRootIndex<=theChildIndex so to handle this case:
    //  <SELECT><OPTGROUP>...</OPTGROUP>
    //
  return PRBool(theRootIndex<=theChildIndex);  
}


/**
 *  This method is called to determine whether or not an END tag
 *  can be autoclosed. This means that based on the current
 *  context, the stack should be closed to the nearest matching
 *  tag.
 *     
 *  @param   aTag -- tag enum of child to be tested
 *  @return  PR_TRUE if autoclosure should occur
 */ 
static
eHTMLTags FindAutoCloseTargetForEndTag(eHTMLTags aCurrentTag,nsDTDContext& aContext) {
  int theTopIndex=aContext.GetCount();
  eHTMLTags thePrevTag=aContext.Last();
 
  if(nsHTMLElement::IsContainer(aCurrentTag)){
    PRInt32 theChildIndex=GetIndexOfChildOrSynonym(aContext,aCurrentTag);
    
    if(kNotFound<theChildIndex) {
      if((thePrevTag==aContext[theChildIndex]) || 
         (eHTMLTag_noscript==aCurrentTag)){  //bug 54571
        return aContext[theChildIndex];
      } 
    
      if(nsHTMLElement::IsBlockCloser(aCurrentTag)) {

          /*here's what to do: 
              Our here is sitting at aChildIndex. There are other tags above it
              on the stack. We have to try to close them out, but we may encounter
              one that can block us. The way to tell is by comparing each tag on
              the stack against our closeTag and rootTag list. 

              For each tag above our hero on the stack, ask 3 questions:
                1. Is it in the closeTag list? If so, the we can skip over it
                2. Is it in the rootTag list? If so, then we're gated by it
                3. Otherwise its non-specified and we simply presume we can close it.
          */
 
        TagList* theCloseTags=gHTMLElements[aCurrentTag].GetAutoCloseEndTags();
        TagList* theRootTags=gHTMLElements[aCurrentTag].GetEndRootTags();
      
        if(theCloseTags){  
            //at a min., this code is needed for H1..H6
        
          while(theChildIndex<--theTopIndex) {
            eHTMLTags theNextTag=aContext[theTopIndex];
            if(PR_FALSE==FindTagInSet(theNextTag,theCloseTags->mTags,theCloseTags->mCount)) {
              if(PR_TRUE==FindTagInSet(theNextTag,theRootTags->mTags,theRootTags->mCount)) {
                return eHTMLTag_unknown; //we encountered a tag in root list so fail (because we're gated).
              }
              //otherwise presume it's something we can simply ignore and continue search...
            }
            //otherwise its in the close list so skip to next tag...
          } 
          eHTMLTags theTarget=aContext.TagAt(theChildIndex);
          if(aCurrentTag!=theTarget) {
            aCurrentTag=theTarget; //use the synonym.
          }
          return aCurrentTag; //if you make it here, we're ungated and found a target!
        }//if
        else if(theRootTags) {
          //since we didn't find any close tags, see if there is an instance of aCurrentTag
          //above the stack from the roottag.
          if(HasCloseablePeerAboveRoot(*theRootTags,aContext,aCurrentTag,PR_TRUE))
            return aCurrentTag;
          else return eHTMLTag_unknown;
        }
      } //if blockcloser
      else{
        //Ok, a much more sensible approach for non-block closers; use the tag group to determine closure:
        //For example: %phrasal closes %phrasal, %fontstyle and %special
        return gHTMLElements[aCurrentTag].GetCloseTargetForEndTag(aContext,theChildIndex);
      }
    }//if
  } //if
  return eHTMLTag_unknown;
}

/**
 * 
 * @param   
 * @param   
 * @update  gess 10/11/99
 * @return  nada
 */
static void StripWSFollowingTag(eHTMLTags aChildTag,nsITokenizer* aTokenizer,nsTokenAllocator* aTokenAllocator, PRInt32& aNewlineCount){ 
  CToken* theToken= (aTokenizer)? aTokenizer->PeekToken():nsnull; 

  if(aTokenAllocator) { 
    while(theToken) { 
      eHTMLTokenTypes theType=eHTMLTokenTypes(theToken->GetTokenType()); 
      switch(theType) { 
        case eToken_newline: aNewlineCount++; 
        case eToken_whitespace: 
          theToken=aTokenizer->PopToken();
          IF_FREE(theToken);
          theToken=aTokenizer->PeekToken(); 
          break; 
        default: 
          theToken=0; 
          break; 
      } 
    } 
  } 
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
nsresult CNavDTD::HandleEndToken(CToken* aToken) {
  NS_PRECONDITION(0!=aToken,kNullToken);

  nsresult    result=NS_OK;
  eHTMLTags   theChildTag=(eHTMLTags)aToken->GetTypeID();

  #ifdef  RICKG_DEBUG
    WriteTokenToLog(aToken);
  #endif

  switch(theChildTag) {

    case eHTMLTag_script:
      mHasOpenScript=PR_FALSE;
    case eHTMLTag_style:
    case eHTMLTag_link:
    case eHTMLTag_meta:
    case eHTMLTag_textarea:
    case eHTMLTag_title:
      break;

    case eHTMLTag_head:
      StripWSFollowingTag(theChildTag,mTokenizer,mTokenAllocator,mLineNumber);
      mRequestedHead=PR_FALSE;
      //ok to fall through...

    case eHTMLTag_form:
      {
          //this is safe because we call close container directly. This node/token is not cached.
        nsCParserNode theNode((CHTMLToken*)aToken,mLineNumber);
        result=CloseContainer(&theNode,theChildTag,PR_FALSE);
      }
      break;

    case eHTMLTag_br:
      {
          //This is special NAV-QUIRKS code that allows users
          //to use </BR>, even though that isn't a legitimate tag.
        if(eDTDMode_quirks==mDTDMode) {
          // Use recycler and pass the token thro' HandleToken() to fix bugs like 32782.
          CHTMLToken* theToken = (CHTMLToken*)mTokenAllocator->CreateTokenOfType(eToken_start,theChildTag);
          result=HandleToken(theToken,mParser);
        }
      }
      break;

    case eHTMLTag_body:
    case eHTMLTag_html:
      StripWSFollowingTag(theChildTag,mTokenizer,mTokenAllocator,mLineNumber);
      break;

    case eHTMLTag_noframes:
    case eHTMLTag_noembed:
      mHasOpenNoXXX--;
      //and allow to fall through...

    default:
     {
        //now check to see if this token should be omitted, or 
        //if it's gated from closing by the presence of another tag.
        if(gHTMLElements[theChildTag].CanOmitEndTag()) {
          PopStyle(theChildTag);
        }
        else {
          eHTMLTags theParentTag=mBodyContext->Last();

          if(kNotFound==GetIndexOfChildOrSynonym(*mBodyContext,theChildTag)) {

            // Ref: bug 30487
            // Make sure that we don't cross boundaries, of certain elements,
            // to close stylistic information.
            // Ex. <font face="helvetica"><table><tr><td></font></td></tr></table> some text...
            // In the above ex. the orphaned FONT tag, inside TD, should cross TD boundaryto 
            // close the FONT tag above TABLE. 
            static eHTMLTags gBarriers[]={eHTMLTag_td,eHTMLTag_th};

            if(!FindTagInSet(theParentTag,gBarriers,sizeof(gBarriers)/sizeof(theParentTag))) {
              PopStyle(theChildTag);
            }

            // If the bit kHandleStrayTag is set then we automatically open up a matching
            // start tag ( compatibility ).  Currently this bit is set on P tag.
            // This also fixes Bug: 22623
            if(gHTMLElements[theChildTag].HasSpecialProperty(kHandleStrayTag) && mDTDMode!=eDTDMode_strict) {
              // Oh boy!! we found a "stray" tag. Nav4.x and IE introduce line break in
              // such cases. So, let's simulate that effect for compatibility.
              // Ex. <html><body>Hello</P>There</body></html>
              PRBool theParentContains=-1; //set to -1 to force canomit to recompute.
              if(!CanOmit(theParentTag,theChildTag,theParentContains)) {
                IF_HOLD(aToken);
                mTokenizer->PushTokenFront(aToken); //put this end token back...
                CHTMLToken* theToken = (CHTMLToken*)mTokenAllocator->CreateTokenOfType(eToken_start,theChildTag);
                mTokenizer->PushTokenFront(theToken); //put this new token onto stack...
              }
            }
            return result;
          }
          if(result==NS_OK) {
            eHTMLTags theTarget=FindAutoCloseTargetForEndTag(theChildTag,*mBodyContext);
            if(eHTMLTag_unknown!=theTarget) {
              result=CloseContainersTo(theTarget,PR_FALSE);
            }
          }
        }
      }
      break;
  }

  return result;
}

/**
 * This method will be triggered when the end of a table is
 * encountered.  Its primary purpose is to process all the
 * bad-contents pertaining a particular table. The position
 * of the table is the token bank ID.
 *
 * @update harishd 03/24/99
 * @param  aTag - This ought to be a table tag
 *
 */
nsresult CNavDTD::HandleSavedTokens(PRInt32 anIndex) {
    NS_PRECONDITION(mBodyContext != nsnull && mBodyContext->GetCount() > 0,"invalid context");

    nsresult  result      = NS_OK;

    if(anIndex>kNotFound) {
      PRInt32  theBadTokenCount   = mMisplacedContent.GetSize();

      if(theBadTokenCount > 0) {
        
        if(mTempContext==nsnull) mTempContext=new nsDTDContext();

        CToken*   theToken;
        eHTMLTags theTag;
        PRInt32   attrCount;
        PRInt32   theTopIndex = anIndex + 1;
        PRInt32   theTagCount = mBodyContext->GetCount();
        //eHTMLTags theParentTag= mBodyContext->TagAt(anIndex);
               
        //XXX  In the content sink, FORM behaves as a container for parents 
        //other than eHTMLTag_table,eHTMLTag_tbody,eHTMLTag_tr,eHTMLTag_col,
        //eHTMLTag_tfoot,eHTMLTag_thead,eHTMLTag_colgroup.In those cases the stack 
        //position, in the parser, should be synchronized with the sink. -- Ref: Bug 20087.

        if(mHasOpenForm && mIsFormContainer) anIndex++;

        STOP_TIMER()
        MOZ_TIMER_DEBUGLOG(("Stop: Parse Time: CNavDTD::HandleSavedTokensAbove(), this=%p\n", this));     
        // Pause the main context and switch to the new context.
        mSink->BeginContext(anIndex);
        MOZ_TIMER_DEBUGLOG(("Start: Parse Time: CNavDTD::HandleSavedTokensAbove(), this=%p\n", this));
        START_TIMER()
      
        // The body context should contain contents only upto the marked position.  
        PRInt32 i=0;
        nsEntryStack* theChildStyleStack=0;

        for(i=0; i<(theTagCount - theTopIndex); i++) {
          mTempContext->Push((nsCParserNode*)mBodyContext->Pop(theChildStyleStack));
        }
     
        // Now flush out all the bad contents.
        while(theBadTokenCount-- > 0){
          theToken=(CToken*)mMisplacedContent.PopFront();
          if(theToken) {
            theTag       = (eHTMLTags)theToken->GetTypeID();
            attrCount    = (gHTMLElements[theTag].mSkipTarget)? 0:theToken->GetAttributeCount();
            // Put back attributes, which once got popped out, into the tokenizer
            for(PRInt32 j=0;j<attrCount; j++){
              CToken* theAttrToken = (CToken*)mMisplacedContent.PopFront();
              if(theAttrToken) {
                mTokenizer->PushTokenFront(theAttrToken);
              }
              theBadTokenCount--;
            }
            
            if(eToken_end==theToken->GetTokenType()) {
              // Ref: Bug 25202
              // Make sure that the BeginContext() is ended only by the call to
              // EndContext(). Ex: <center><table><a></center>.
              // In the Ex. above </center> should not close <center> above table.
              // Doing so will cause the current context to get closed prematurely. 
              PRInt32 theIndex=mBodyContext->LastOf(theTag);
              
              if(theIndex!=kNotFound && theIndex<=mBodyContext->mContextTopIndex) {
                IF_FREE(theToken);
                continue;
              }
            }
            result=HandleToken(theToken,mParser);
          }
        }//while
        if(theTopIndex != mBodyContext->GetCount()) {
           CloseContainersTo(theTopIndex,mBodyContext->TagAt(theTopIndex),PR_TRUE);
        }
     
        // Bad-contents were successfully processed. Now, itz time to get
        // back to the original body context state.
        for(PRInt32 k=0; k<(theTagCount - theTopIndex); k++) {
          mBodyContext->Push((nsCParserNode*)mTempContext->Pop(theChildStyleStack));
        }

        STOP_TIMER()
        MOZ_TIMER_DEBUGLOG(("Stop: Parse Time: CNavDTD::HandleSavedTokensAbove(), this=%p\n", this));     
        // Terminate the new context and switch back to the main context
        mSink->EndContext(anIndex);
        MOZ_TIMER_DEBUGLOG(("Start: Parse Time: CNavDTD::HandleSavedTokensAbove(), this=%p\n", this));
        START_TIMER()
      }
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
nsresult CNavDTD::HandleEntityToken(CToken* aToken) {
  NS_PRECONDITION(0!=aToken,kNullToken);

  nsresult  result=NS_OK;

  nsString& theStr=aToken->GetStringValueXXX();
  PRUnichar theChar=theStr.CharAt(0);
  if((kHashsign!=theChar) && (-1==nsHTMLEntities::EntityToUnicode(theStr))){

    //before we just toss this away as a bogus entity, let's check...
    CNamedEntity *theEntity=mBodyContext->GetEntity(theStr);
    CToken *theToken=0;
    if(theEntity) {
      theToken=(CTextToken*)mTokenAllocator->CreateTokenOfType(eToken_text,eHTMLTag_text,theEntity->mValue);
    }
    else {
      //if you're here we have a bogus entity.
      //convert it into a text token.
      theToken=(CTextToken*)mTokenAllocator->CreateTokenOfType(eToken_text,eHTMLTag_text,NS_ConvertToString("&"));
      if(theToken) {
        nsString &theTokenStr=theToken->GetStringValueXXX();
        theTokenStr.Append(theStr); //should append the entity name; fix bug 51161.
      }
    }
    return HandleToken(theToken,mParser); //theToken should get recycled automagically...
  }

  eHTMLTags theParentTag=mBodyContext->Last();

  nsCParserNode* theNode=mNodeRecycler->CreateNode();
  if(theNode) {
    theNode->Init(aToken,mLineNumber,0);
    PRBool theParentContains=-1; //set to -1 to force CanOmit to recompute...
    if(CanOmit(theParentTag,eHTMLTag_entity,theParentContains)) {
      eHTMLTags theCurrTag=(eHTMLTags)aToken->GetTypeID();
      result=HandleOmittedTag(aToken,theCurrTag,theParentTag,theNode);
      return result;
    }
  
    #ifdef  RICKG_DEBUG
      WriteTokenToLog(aToken);
    #endif

    result=AddLeaf(theNode);
    mNodeRecycler->RecycleNode(theNode);
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
nsresult CNavDTD::HandleCommentToken(CToken* aToken) {
  NS_PRECONDITION(0!=aToken,kNullToken);
  
  nsresult  result=NS_OK;

  nsString& theComment=aToken->GetStringValueXXX();
  mLineNumber += (theComment).CountChar(kNewLine);

  nsCParserNode* theNode=mNodeRecycler->CreateNode();
  if(theNode) {
    theNode->Init(aToken,mLineNumber,0);

  #ifdef  RICKG_DEBUG
    WriteTokenToLog(aToken);
  #endif

    STOP_TIMER();
    MOZ_TIMER_DEBUGLOG(("Stop: Parse Time: CNavDTD::HandleCommentToken(), this=%p\n", this));

    result=(mSink) ? mSink->AddComment(*theNode) : NS_OK;  

    mNodeRecycler->RecycleNode(theNode);

    MOZ_TIMER_DEBUGLOG(("Start: Parse Time: CNavDTD::HandleCommentToken(), this=%p\n", this));
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
nsresult CNavDTD::HandleAttributeToken(CToken* aToken) {
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
nsresult CNavDTD::HandleScriptToken(const nsIParserNode *aNode) {
  // PRInt32 attrCount=aNode.GetAttributeCount(PR_TRUE);

  STOP_TIMER();
  MOZ_TIMER_DEBUGLOG(("Stop: Parse Time: CNavDTD::HandleScriptToken(), this=%p\n", this));

  nsresult result=AddLeaf(aNode);

  MOZ_TIMER_DEBUGLOG(("Start: Parse Time: CNavDTD::HandleScriptToken(), this=%p\n", this));
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
nsresult CNavDTD::HandleProcessingInstructionToken(CToken* aToken){
  NS_PRECONDITION(0!=aToken,kNullToken);

  nsresult  result=NS_OK;

  nsCParserNode* theNode=mNodeRecycler->CreateNode();
  if(theNode) {
    theNode->Init(aToken,mLineNumber,0);

  #ifdef  RICKG_DEBUG
    WriteTokenToLog(aToken);
  #endif

    STOP_TIMER();
    MOZ_TIMER_DEBUGLOG(("Stop: Parse Time: CNavDTD::HandleProcessingInstructionToken(), this=%p\n", this));

    result=(mSink) ? mSink->AddProcessingInstruction(*theNode) : NS_OK; 

    mNodeRecycler->RecycleNode(theNode);

    MOZ_TIMER_DEBUGLOG(("Start: Parse Time: CNavDTD::HandleProcessingInstructionToken(), this=%p\n", this));
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
nsresult CNavDTD::HandleDocTypeDeclToken(CToken* aToken){
  NS_PRECONDITION(0!=aToken,kNullToken);

  nsresult result=NS_OK;

  #ifdef  RICKG_DEBUG
    WriteTokenToLog(aToken);
  #endif

  nsString& docTypeStr=aToken->GetStringValueXXX();
  mLineNumber += (docTypeStr).CountChar(kNewLine);
  
  PRInt32 len=docTypeStr.Length();
  PRInt32 pos=docTypeStr.RFindChar(kGreaterThan);
  if(pos>-1) {
    docTypeStr.Cut(pos,len-pos);// First remove '>' from the end.
  }
  docTypeStr.Cut(0,2); // Now remove "<!" from the begining

  nsCParserNode* theNode=mNodeRecycler->CreateNode();
  if(theNode) {
    theNode->Init(aToken,mLineNumber,0);

  STOP_TIMER();
  MOZ_TIMER_DEBUGLOG(("Stop: Parse Time: CNavDTD::HandleDocTypeDeclToken(), this=%p\n", this));
  
    /*************************************************************
      While the parser is happy to deal with various modes, the
      rest of layout prefers only 2: strict vs. quirks. So we'll
      constrain the modes when reporting to layout.
     *************************************************************/
    nsDTDMode theMode=mDTDMode;
    switch(mDTDMode) {
      case eDTDMode_transitional:
      case eDTDMode_strict:
        theMode=eDTDMode_strict;
        break;
      default:
        theMode=eDTDMode_quirks;
    }

    result = (mSink)? mSink->AddDocTypeDecl(*theNode,theMode):NS_OK;
    
    mNodeRecycler->RecycleNode(theNode);
  
  MOZ_TIMER_DEBUGLOG(("Start: Parse Time: CNavDTD::HandleDocTypeDeclToken(), this=%p\n", this));
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
nsresult CNavDTD::CollectAttributes(nsCParserNode& aNode,eHTMLTags aTag,PRInt32 aCount){
  int attr=0;

  nsresult result=NS_OK;
  int theAvailTokenCount=mTokenizer->GetCount() + mSkippedContent.GetSize();
  if(aCount<=theAvailTokenCount) {
    CToken* theToken=0;
    eHTMLTags theSkipTarget=gHTMLElements[aTag].mSkipTarget;
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
 *
 * @update  gess 4Sep2000
 * @param   node to consume skipped-content
 * @param   holds the number of skipped content elements encountered
 * @return  Error condition.
 */
nsresult CNavDTD::CollectSkippedContent(nsCParserNode& aNode,PRInt32 &aCount) {

  eHTMLTags       theNodeTag=(eHTMLTags)aNode.GetNodeType();

  int aIndex=0;
  int aMax=mSkippedContent.GetSize();
  
  // XXX rickg This linefeed conversion stuff should be moved out of
  // the parser and into the form element code
  PRBool aMustConvertLinebreaks = PR_FALSE;

  mScratch.Truncate();
  aNode.SetSkippedContent(mScratch);  //this guarantees us some skipped content storage.

  for(aIndex=0;aIndex<aMax;aIndex++){
    CHTMLToken* theNextToken=(CHTMLToken*)mSkippedContent.PopFront();

    eHTMLTokenTypes theTokenType=(eHTMLTokenTypes)theNextToken->GetTokenType();

    // Dont worry about attributes here because it's already stored in 
    // the start token as mTrailing content and will get appended in 
    // start token's GetSource();
    if(eToken_attribute!=theTokenType) {
      if ((eToken_entity==theTokenType) &&
         ((eHTMLTag_textarea==theNodeTag) || (eHTMLTag_title==theNodeTag))) {
          mScratch.Truncate();
          ((CEntityToken*)theNextToken)->TranslateToUnicodeStr(mScratch);
          // since this is an entity, we know that it's only one character.
          // check to see if it's a CR, in which case we'll need to do line
          // termination conversion at the end.
          if(mScratch.Length()>0){
            aMustConvertLinebreaks |= (mScratch[0] == kCR);
            aNode.mSkippedContent->Append(mScratch);
          }
        }
      else theNextToken->AppendSource(*aNode.mSkippedContent);
    }
    IF_FREE(theNextToken);
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
PRBool CNavDTD::CanContain(PRInt32 aParent,PRInt32 aChild) const {
  PRBool result=gHTMLElements[aParent].CanContain((eHTMLTags)aChild);

#ifdef ALLOW_TR_AS_CHILD_OF_TABLE
  if(!result) {
      //XXX This vile hack is here to support bug 30378, which allows
      //table to contain tr directly in an html32 document.
    if((eHTMLTag_tr==aChild) && (eHTMLTag_table==aParent)) {
      result=PR_TRUE;
    }
  }
#endif

  if(!result) {
    // Bug 42429 - Preserve whitespace inside TABLE,TR,TBODY,TFOOT,etc.,
    if(gHTMLElements[aParent].HasSpecialProperty(kBadContentWatch)) {
      if(nsHTMLElement::IsWhitespaceTag((eHTMLTags)aChild)) { 
        result=PR_TRUE; 
      }
    }
  }

  return result;
} 

/**
 * Give rest of world access to our tag enums, so that CanContain(), etc,
 * become useful.
 */
NS_IMETHODIMP CNavDTD::StringTagToIntTag(nsString &aTag, PRInt32* aIntTag) const {
  *aIntTag = nsHTMLTags::LookupTag(aTag);
  return NS_OK;
}

NS_IMETHODIMP CNavDTD::IntTagToStringTag(PRInt32 aIntTag, nsString& aTag) const {
  aTag.AssignWithConversion(nsHTMLTags::GetStringValue((nsHTMLTag)aIntTag));
  return NS_OK;
}

NS_IMETHODIMP CNavDTD::ConvertEntityToUnicode(const nsString& aEntity, PRInt32* aUnicode) const {
  *aUnicode = nsHTMLEntities::EntityToUnicode(aEntity);
  return NS_OK;
}


/**
 *  This method is called to determine whether or not
 *  the given childtag is a block element.
 *
 *  @update  gess 6June2000
 *  @param   aChildID -- tag id of child 
 *  @param   aParentID -- tag id of parent (or eHTMLTag_unknown)
 *  @return  PR_TRUE if this tag is a block tag
 */
PRBool CNavDTD::IsBlockElement(PRInt32 aTagID,PRInt32 aParentID) const {
  PRBool result=PR_FALSE;
  eHTMLTags theTag=(eHTMLTags)aTagID;

  if((theTag>eHTMLTag_unknown) && (theTag<eHTMLTag_userdefined)) {
    result=((gHTMLElements[theTag].IsMemberOf(kBlock))       || 
            (gHTMLElements[theTag].IsMemberOf(kBlockEntity)) || 
            (gHTMLElements[theTag].IsMemberOf(kHeading))     || 
            (gHTMLElements[theTag].IsMemberOf(kPreformatted))|| 
            (gHTMLElements[theTag].IsMemberOf(kList))); 
  }

  return result;
}

/**
 *  This method is called to determine whether or not
 *  the given childtag is an inline element.
 *
 *  @update  gess 6June2000
 *  @param   aChildID -- tag id of child 
 *  @param   aParentID -- tag id of parent (or eHTMLTag_unknown)
 *  @return  PR_TRUE if this tag is an inline tag
 */
PRBool CNavDTD::IsInlineElement(PRInt32 aTagID,PRInt32 aParentID) const {
  PRBool result=PR_FALSE;
  eHTMLTags theTag=(eHTMLTags)aTagID;

  if((theTag>eHTMLTag_unknown) && (theTag<eHTMLTag_userdefined)) {
    result=((gHTMLElements[theTag].IsMemberOf(kInlineEntity))|| 
            (gHTMLElements[theTag].IsMemberOf(kFontStyle))   || 
            (gHTMLElements[theTag].IsMemberOf(kPhrase))      || 
            (gHTMLElements[theTag].IsMemberOf(kSpecial))     || 
            (gHTMLElements[theTag].IsMemberOf(kFormControl)));
  }

  return result;
}

/**
 *  This method is called to determine whether or not
 *  the necessary intermediate tags should be propagated
 *  between the given parent and given child.
 *
 *  @update  gess 4/8/98
 *  @param   aParent -- tag enum of parent container
 *  @param   aChild -- tag enum of child container
 *  @return  PR_TRUE if propagation should occur
 */
PRBool CNavDTD::CanPropagate(eHTMLTags aParentTag,eHTMLTags aChildTag,PRBool aParentContains)  {
  PRBool    result=PR_FALSE;
  PRBool    theParentContains=(-1==aParentContains) ? CanContain(aParentTag,aChildTag) : aParentContains;

  if(aParentTag==aChildTag) {
    return result;
  }

  if(nsHTMLElement::IsContainer(aChildTag)){
    mScratch.Truncate();
    if(!gHTMLElements[aChildTag].HasSpecialProperty(kNoPropagate)){
      if(nsHTMLElement::IsBlockParent(aParentTag) || (gHTMLElements[aParentTag].GetSpecialChildren())) {

        result=ForwardPropagate(mScratch,aParentTag,aChildTag);

        if(PR_FALSE==result){

          if(eHTMLTag_unknown!=aParentTag) {
            if(aParentTag!=aChildTag) //dont even bother if we're already inside a similar element...
              result=BackwardPropagate(mScratch,aParentTag,aChildTag);
          } //if
          else result=BackwardPropagate(mScratch,eHTMLTag_html,aChildTag);

        } //elseif

      }//if
    }//if
    if(mScratch.Length()-1>gHTMLElements[aParentTag].mPropagateRange)
      result=PR_FALSE;
  }//if
  else result=theParentContains;


  return result;
}


/**
 *  This method gets called to determine whether a given 
 *  tag can be omitted from opening. Most cannot.
 *  
 *  @update  gess 3/25/98
 *  @param   aParent
 *  @param   aChild
 *  @param   aParentContains
 *  @return  PR_TRUE if given tag can contain other tags
 */
PRBool CNavDTD::CanOmit(eHTMLTags aParent,eHTMLTags aChild,PRBool& aParentContains)  {

  eHTMLTags theAncestor=gHTMLElements[aChild].mExcludingAncestor;
  if(eHTMLTag_unknown!=theAncestor){
    if(HasOpenContainer(theAncestor))
      return PR_TRUE;
  }

  theAncestor=gHTMLElements[aChild].mRequiredAncestor;
  if(eHTMLTag_unknown!=theAncestor){
    if(!HasOpenContainer(theAncestor)) {
      if(!CanPropagate(aParent,aChild,aParentContains)) {
        return PR_TRUE;
      }
    }
    return PR_FALSE;
  }


  if(gHTMLElements[aParent].HasSpecialProperty(kOmitWS)) {
    if(nsHTMLElement::IsWhitespaceTag(aChild)) {
      return PR_TRUE;
    }
  }

  if(gHTMLElements[aParent].CanExclude(aChild)){
    return PR_TRUE;
  }

    //Now the obvious test: if the parent can contain the child, don't omit.
  if(-1==aParentContains)
    aParentContains=CanContain(aParent,aChild);

  if(aParentContains || (aChild==aParent)){
    return PR_FALSE;
  }

  if(gHTMLElements[aParent].IsBlockEntity()) {
    if(nsHTMLElement::IsInlineEntity(aChild)) {  //feel free to drop inlines that a block doesn't contain.
      return PR_TRUE;
    }
  }

  if(gHTMLElements[aParent].HasSpecialProperty(kBadContentWatch)) {

    if(-1==aParentContains) {    
      //we need to compute parent containment here, since it wasn't given...
      if(!gHTMLElements[aParent].CanContain(aChild)){
        return PR_TRUE;
      }
    }
    else if (!aParentContains) {
      if(!gHTMLElements[aChild].HasSpecialProperty(kBadContentWatch)) {
        return PR_TRUE; 
      }
      return PR_FALSE; // Ref. Bug 25658
    }
  }

  if(gHTMLElements[aParent].HasSpecialProperty(kSaveMisplaced)) {
    return PR_TRUE;
  }

  return PR_FALSE;
}
     

/**
 *  This method gets called to determine whether a given 
 *  tag is itself a container
 *  
 *  @update  gess 4/8/98
 *  @param   aTag -- tag to test as a container
 *  @return  PR_TRUE if given tag can contain other tags
 */
PRBool CNavDTD::IsContainer(PRInt32 aTag) const {
  return nsHTMLElement::IsContainer((eHTMLTags)aTag);
}


/**
 * This method tries to design a context vector (without actually
 * changing our parser state) from the parent down to the
 * child. 
 * 
 * @update  gess4/6/98
 * @param   aVector is the string where we store our output vector
 *          in bottom-up order.
 * @param   aParent -- tag type of parent
 * @param   aChild -- tag type of child
 * @return  TRUE if propagation closes; false otherwise
 */
PRBool CNavDTD::ForwardPropagate(nsString& aSequence,eHTMLTags aParentTag,eHTMLTags aChildTag)  {
  PRBool result=PR_FALSE;

  switch(aParentTag) {
    case eHTMLTag_table:
      {
        if((eHTMLTag_tr==aChildTag) || (eHTMLTag_td==aChildTag)) {
          return BackwardPropagate(aSequence,aParentTag,aChildTag);
        }
      }
      //otherwise, intentionally fall through...

    case eHTMLTag_tr:
      {  
        PRBool theCanContainResult=CanContain(eHTMLTag_td,aChildTag);
        if(PR_TRUE==theCanContainResult) {
          aSequence.Append((PRUnichar)eHTMLTag_td);
          result=BackwardPropagate(aSequence,aParentTag,eHTMLTag_td);
        }
      }
      break;

    case eHTMLTag_th:
      break;

    default:
      break;
  }//switch
  return result;
}


/**
 * This method tries to design a context map (without actually
 * changing our parser state) from the child up to the parent.
 *
 * @update  gess4/6/98
 * @param   aVector is the string where we store our output vector
 *          in bottom-up order.
 * @param   aParent -- tag type of parent
 * @param   aChild -- tag type of child
 * @return  TRUE if propagation closes; false otherwise
 */
PRBool CNavDTD::BackwardPropagate(nsString& aSequence,eHTMLTags aParentTag,eHTMLTags aChildTag) const {

  eHTMLTags theParentTag=aParentTag; //just init to get past first condition...

  do {
    TagList* theRootTags=gHTMLElements[aChildTag].GetRootTags();
    if(theRootTags) {
      theParentTag=theRootTags->mTags[0];
      if(CanContain(theParentTag,aChildTag)) {
        //we've found a complete sequence, so push the parent...
        aChildTag=theParentTag;
        aSequence.Append((PRUnichar)theParentTag);
      }
    }
    else break;
  } 
  while((theParentTag!=eHTMLTag_unknown) && (theParentTag!=aParentTag));

  return PRBool(aParentTag==theParentTag);
}


/**
 *  This method allows the caller to determine if a given container
 *  is currently open
 *  
 *  @update  gess 11/9/98
 *  @param   
 *  @return  
 */
PRBool CNavDTD::HasOpenContainer(eHTMLTags aContainer) const {
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
PRBool CNavDTD::HasOpenContainer(const eHTMLTags aTagSet[],PRInt32 aCount) const {

  int theIndex; 
  int theTopIndex=mBodyContext->GetCount()-1;

  for(theIndex=theTopIndex;theIndex>0;theIndex--){
    if(FindTagInSet((*mBodyContext)[theIndex],aTagSet,aCount))
      return PR_TRUE;
  }
  return PR_FALSE;
}

/**
 *  This method retrieves the HTMLTag type of the topmost
 *  container on the stack.
 *  
 *  @update  gess 4/2/98
 *  @return  tag id of topmost node in contextstack
 */
eHTMLTags CNavDTD::GetTopNode() const {
  return mBodyContext->Last();
}

/*********************************************
  Here comes code that handles the interface
  to our content sink.
 *********************************************/
 

/**
 * It is with great trepidation that I offer this method (privately of course).
 * The gets called whenever a container gets opened. This methods job is to 
 * take a look at the (transient) style stack, and open any style containers that
 * are there. Of course, we shouldn't bother to open styles that are incompatible
 * with our parent container.
 *
 * @update  gess6/4/98
 * @param   tag of the container just opened
 * @return  0 (for now)
 */
nsresult CNavDTD::OpenTransientStyles(eHTMLTags aChildTag){
  nsresult result=NS_OK;

  // No need to open transient styles in head context - Fix for 41427
  if(mStyleHandlingEnabled  && (eHTMLTag_newline!=aChildTag) && !mHasOpenHead) {

#ifdef  ENABLE_RESIDUALSTYLE

    if(CanContain(eHTMLTag_font,aChildTag)) {

      PRUint32 theCount=mBodyContext->GetCount();
      PRUint32 theLevel=theCount;

        //this first loop is used to determine how far up the containment
        //hierarchy we go looking for residual styles.
      while ( 1<theLevel) {
        eHTMLTags theParentTag = mBodyContext->TagAt(--theLevel);
        if(gHTMLElements[theParentTag].HasSpecialProperty(kNoStyleLeaksIn)) {
          break;
        }
      }

      mStyleHandlingEnabled=PR_FALSE;
      for(;theLevel<theCount;theLevel++){
        nsEntryStack* theStack=mBodyContext->GetStylesAt(theLevel);
        if(theStack){

          PRInt32 sindex=0;

          nsTagEntry *theEntry=theStack->mEntries;
          for(sindex=0;sindex<theStack->mCount;sindex++){            
            nsCParserNode* theNode=(nsCParserNode*)theEntry->mNode;
            if(1==theNode->mUseCount) {
              eHTMLTags theNodeTag=(eHTMLTags)theNode->GetNodeType();
              if(gHTMLElements[theNodeTag].CanContain(aChildTag)) {
                theEntry->mParent=theStack;  //we do this too, because this entry differs from the new one we're pushing...              
                result=OpenContainer(theNode,theNodeTag,PR_FALSE,theStack);
              }
              else {
                //if the node tag can't contain the child tag, then remove the child tag from the style stack
                nsCParserNode* theRemovedNode=(nsCParserNode*)theStack->Remove(sindex,theNodeTag);
                if(theRemovedNode) {
                  mNodeRecycler->RecycleNode(theRemovedNode);
                }
                theEntry--; //back up by one
              }
            } //if
            theEntry++;
          } //for
        } //if
      } //for
      mStyleHandlingEnabled=PR_TRUE;

    } //if

#endif
  }//if
  return result;
}

/**
 * It is with great trepidation that I offer this method (privately of course).
 * The gets called just prior when a container gets opened. This methods job is to 
 * take a look at the (transient) style stack, and <i>close</i> any style containers 
 * that are there. Of course, we shouldn't bother to open styles that are incompatible
 * with our parent container.
 * SEE THE TOP OF THIS FILE for more information about how the transient style stack works.
 *
 * @update  gess6/4/98
 * @param   tag of the container just opened
 * @return  0 (for now)
 */
nsresult CNavDTD::CloseTransientStyles(eHTMLTags aChildTag){
  nsresult result=NS_OK;

  if(mStyleHandlingEnabled) {

#ifdef  ENABLE_RESIDUALSTYLE
#if 0
    int theTagPos=0;
      //now iterate style set, and close the containers...

    nsEntryStack* theStack=mBodyContext->GetStylesAt();
    for(theTagPos=mBodyContext->mOpenStyles;theTagPos>0;theTagPos--){
      eHTMLTags theTag=GetTopNode();
      CStartToken   token(theTag);
      nsCParserNode theNode(&token,mLineNumber);
      token.SetTypeID(theTag); 
      result=CloseContainer(theNode,theTag,PR_FALSE);
    }
#endif
#endif
  } //if
  return result;
}

/**
 * This method gets called when an explicit style close-tag is encountered.
 * It results in the style tag id being popped from our internal style stack.
 *
 * @update  gess6/4/98
 * @param 
 * @return  0 if all went well (which it always does)
 */
nsresult CNavDTD::PopStyle(eHTMLTags aTag){
  nsresult result=0;

  if(mStyleHandlingEnabled) {
#ifdef  ENABLE_RESIDUALSTYLE
    if(nsHTMLElement::IsResidualStyleTag(aTag)) {
      nsCParserNode* theNode=(nsCParserNode*)mBodyContext->PopStyle(aTag);
      if(theNode) {
        mNodeRecycler->RecycleNode(theNode);
      }
    }
#endif
  } //if
  return result;
}


/**
 * This method does two things: 1st, help construct
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 * 
 * @update  gess4/22/98
 * @param   aNode -- next node to be added to model
 */
nsresult CNavDTD::OpenHTML(const nsIParserNode *aNode){
  NS_PRECONDITION(mBodyContext->GetCount() >= 0, kInvalidTagStackPos);

  STOP_TIMER();
  MOZ_TIMER_DEBUGLOG(("Stop: Parse Time: CNavDTD::OpenHTML(), this=%p\n", this));

  nsresult result=(mSink) ? mSink->OpenHTML(*aNode) : NS_OK; 

  MOZ_TIMER_DEBUGLOG(("Start: Parse Time: CNavDTD::OpenHTML(), this=%p\n", this));
  START_TIMER();

  // Don't push more than one HTML tag into the stack...
  if(mBodyContext->GetCount()==0) 
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
nsresult CNavDTD::CloseHTML(const nsIParserNode *aNode){

  STOP_TIMER();
  MOZ_TIMER_DEBUGLOG(("Stop: Parse Time: CNavDTD::CloseHTML(), this=%p\n", this));

  nsresult result=(mSink) ? mSink->CloseHTML(*aNode) : NS_OK; 

  MOZ_TIMER_DEBUGLOG(("Start: Parse Time: CNavDTD::CloseHTML(), this=%p\n", this));
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
nsresult CNavDTD::OpenHead(const nsIParserNode *aNode){

  nsresult result=NS_OK;

  STOP_TIMER();
  MOZ_TIMER_DEBUGLOG(("Stop: Parse Time: CNavDTD::OpenHead(), this=%p\n", this));

  if(!mHasOpenHead++) {
    result=(mSink) ? mSink->OpenHead(*aNode) : NS_OK;
  }

  MOZ_TIMER_DEBUGLOG(("Start: Parse Time: CNavDTD::OpenHead(), this=%p\n", this));
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
nsresult CNavDTD::CloseHead(const nsIParserNode *aNode){
  nsresult result=NS_OK;

  if(mHasOpenHead) {
    if(0==--mHasOpenHead){

      STOP_TIMER();
      MOZ_TIMER_DEBUGLOG(("Stop: Parse Time: CNavDTD::CloseHead(), this=%p\n", this));

      result=(mSink) ? mSink->CloseHead(*aNode) : NS_OK; 

      MOZ_TIMER_DEBUGLOG(("Start: Parse Time: CNavDTD::CloseHead(), this=%p\n", this));
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
nsresult CNavDTD::OpenBody(const nsIParserNode *aNode){
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
    MOZ_TIMER_DEBUGLOG(("Stop: Parse Time: CNavDTD::OpenBody(), this=%p\n", this));

    result=(mSink) ? mSink->OpenBody(*aNode) : NS_OK; 

    MOZ_TIMER_DEBUGLOG(("Start: Parse Time: CNavDTD::OpenBody(), this=%p\n", this));
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
nsresult CNavDTD::CloseBody(const nsIParserNode *aNode){
//  NS_PRECONDITION(mBodyContext->GetCount() >= 0, kInvalidTagStackPos);

  STOP_TIMER();
  MOZ_TIMER_DEBUGLOG(("Stop: Parse Time: CNavDTD::CloseBody(), this=%p\n", this));

  nsresult result=(mSink) ? mSink->CloseBody(*aNode) : NS_OK; 

  MOZ_TIMER_DEBUGLOG(("Start: Parse Time: CNavDTD::CloseBody(), this=%p\n", this));
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
nsresult CNavDTD::OpenForm(const nsIParserNode *aNode){
  static eHTMLTags gTableElements[]={eHTMLTag_table,eHTMLTag_tbody,eHTMLTag_tr,
                                     eHTMLTag_td,eHTMLTag_th,eHTMLTag_col,
                                     eHTMLTag_tfoot,eHTMLTag_thead,eHTMLTag_colgroup};
  if(mHasOpenForm)
    CloseForm(aNode);
    
  mIsFormContainer=!(FindTagInSet(mBodyContext->Last(),gTableElements,sizeof(gTableElements)/sizeof(eHTMLTag_unknown)));

  STOP_TIMER();
  MOZ_TIMER_DEBUGLOG(("Stop: Parse Time: CNavDTD::OpenForm(), this=%p\n", this));

  nsresult result=(mSink) ? mSink->OpenForm(*aNode) : NS_OK; 

  MOZ_TIMER_DEBUGLOG(("Start: Parse Time: CNavDTD::OpenForm(), this=%p\n", this));
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
nsresult CNavDTD::CloseForm(const nsIParserNode *aNode){
//  NS_PRECONDITION(mBodyContext->GetCount() > 0, kInvalidTagStackPos);
  nsresult result=NS_OK;
  if(mHasOpenForm) {
    mHasOpenForm=PR_FALSE;

    STOP_TIMER();
    MOZ_TIMER_DEBUGLOG(("Stop: Parse Time: CNavDTD::CloseForm(), this=%p\n", this));

    result=(mSink) ? mSink->CloseForm(*aNode) : NS_OK; 

    MOZ_TIMER_DEBUGLOG(("Start: Parse Time: CNavDTD::CloseForm(), this=%p\n", this));
    START_TIMER();
        
    if(mIsFormContainer)
      mIsFormContainer=PR_FALSE;

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
nsresult CNavDTD::OpenMap(const nsIParserNode *aNode){
  if(mHasOpenMap)
    CloseMap(aNode);

  STOP_TIMER();
  MOZ_TIMER_DEBUGLOG(("Stop: Parse Time: CNavDTD::OpenMap(), this=%p\n", this));

  nsresult result=(mSink) ? mSink->OpenMap(*aNode) : NS_OK; 

  MOZ_TIMER_DEBUGLOG(("Start: Parse Time: CNavDTD::OpenMap(), this=%p\n", this));
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
nsresult CNavDTD::CloseMap(const nsIParserNode *aNode){
//  NS_PRECONDITION(mBodyContext->GetCount() > 0, kInvalidTagStackPos);
  nsresult result=NS_OK;
  if(mHasOpenMap) {
    mHasOpenMap=PR_FALSE;

    STOP_TIMER();
    MOZ_TIMER_DEBUGLOG(("Stop: Parse Time: CNavDTD::CloseMap(), this=%p\n", this));

    result=(mSink) ? mSink->CloseMap(*aNode) : NS_OK; 

    MOZ_TIMER_DEBUGLOG(("Start: Parse Time: CNavDTD::CloseMap(), this=%p\n", this));
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
nsresult CNavDTD::OpenFrameset(const nsIParserNode *aNode){
  NS_PRECONDITION(mBodyContext->GetCount() >= 0, kInvalidTagStackPos);

  mHadFrameset=PR_TRUE;

  STOP_TIMER();
  MOZ_TIMER_DEBUGLOG(("Stop: Parse Time: CNavDTD::OpenFrameset(), this=%p\n", this));

  nsresult result=(mSink) ? mSink->OpenFrameset(*aNode) : NS_OK; 

  MOZ_TIMER_DEBUGLOG(("Start: Parse Time: CNavDTD::OpenFrameset(), this=%p\n", this));
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
nsresult CNavDTD::CloseFrameset(const nsIParserNode *aNode){
//  NS_PRECONDITION(mBodyContext->GetCount() > 0, kInvalidTagStackPos);

  STOP_TIMER();
  MOZ_TIMER_DEBUGLOG(("Stop: Parse Time: CNavDTD::CloseFrameset(), this=%p\n", this));

  nsresult result=(mSink) ? mSink->CloseFrameset(*aNode) : NS_OK; 

  MOZ_TIMER_DEBUGLOG(("Start: Parse Time: CNavDTD::CloseFrameset(), this=%p\n", this));
  START_TIMER();

  return result;
}

/**
 *  This method would determine how the noscript content
 *  should be handled.
 *
 *  harishd 08/24/00
 *  @param  aNode - The noscript node
 *  return  NS_OK if succeeded else ERROR
 */
nsresult CNavDTD::OpenNoscript(const nsIParserNode *aNode,nsEntryStack* aStyleStack) {
  nsresult result=NS_OK;

  if(mSink) {
    STOP_TIMER();
    MOZ_TIMER_DEBUGLOG(("Stop: Parse Time: CNavDTD::OpenNoscript(), this=%p\n", this));
    
    result=mSink->OpenNoscript(*aNode);

    MOZ_TIMER_DEBUGLOG(("Start: Parse Time: CNavDTD::OpenNoscript(), this=%p\n", this));
    START_TIMER();

    if(NS_SUCCEEDED(result)) {
      if(result==NS_HTMLPARSER_ALTERNATECONTENT) {
        // We're here because the sink has identified that
        // JS is enabled and therefore noscript content should
        // not be treated as a regular content,i.e., make sure
        // that head elements are handled correctly and may be
        // residual style.
        mDTDState=result;
        // Though NS_HTMLPARSER_ALTERNATECONTENT is a succeeded message we don't want to propagate it
        // because there are lots of places where we don't check for succeeded result instead
        // we check for NS_OK. Also, this message is pertinent to the DTD only 
        result=NS_OK; 
      }
      mHasOpenNoXXX++;
      mBodyContext->Push(aNode,aStyleStack);
    }
  }
  
  return result;
}

/**
 *  Call this method to stop handling noscript content
 *
 *  harishd 08/24/00
 *  @param  aNode - The noscript node
 *  return  NS_OK if succeeded else ERROR
 */
nsresult CNavDTD::CloseNoscript(const nsIParserNode *aNode) {
  nsresult result=NS_OK;

  if(mSink) {
    STOP_TIMER();
    MOZ_TIMER_DEBUGLOG(("Stop: Parse Time: CNavDTD::CloseNoscript(), this=%p\n", this));

    result=mSink->CloseNoscript(*aNode);

    MOZ_TIMER_DEBUGLOG(("Start: Parse Time: CNavDTD::CloseNoscript(), this=%p\n", this));
    START_TIMER();

    if(NS_SUCCEEDED(result)) {
      NS_ASSERTION((mHasOpenNoXXX > -1), "mHasOpenNoXXX underflow");
      if(mHasOpenNoXXX > 0) { 
        mHasOpenNoXXX--;
      }
      mDTDState=NS_OK;  // switch from alternate content state to regular state
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
 * @param   aClosedByStartTag -- ONLY TRUE if the container is being closed by opening of another container.
 * @return  TRUE if ok, FALSE if error
 */
nsresult
CNavDTD::OpenContainer(const nsIParserNode *aNode,eHTMLTags aTag,PRBool aClosedByStartTag,nsEntryStack* aStyleStack){
  NS_PRECONDITION(mBodyContext->GetCount() >= 0, kInvalidTagStackPos);
  
  nsresult   result=NS_OK; 
  PRBool     isDefaultNode=PR_FALSE;


  if (nsHTMLElement::IsResidualStyleTag(aTag)) {
    /***********************************************************************
     *  Here's an interesting problem:
     *
     *  If there's an <a> on the RS-stack, and you're trying to open 
     *  another <a>, the one on the RS-stack should be discarded.
     *
     *  I'm updating OpenTransientStyles to throw old <a>'s away.
     *
     ***********************************************************************/

    OpenTransientStyles(aTag); 
  }

#ifdef ENABLE_CRC
  #define K_OPENOP 100
  CRCStruct theStruct(aTag,K_OPENOP);
  mComputedCRC32=AccumulateCRC(mComputedCRC32,(char*)&theStruct,sizeof(theStruct));
#endif

  switch(aTag) {

    case eHTMLTag_html:
      result=OpenHTML(aNode); break;

    case eHTMLTag_head:
     // result=OpenHead(aNode); //open the head...
      result=OpenHead(aNode); 
      break;

    case eHTMLTag_body:
      {
        eHTMLTags theParent=mBodyContext->Last();
        if(!gHTMLElements[aTag].IsSpecialParent(theParent)) {
          mHasOpenBody=PR_TRUE;
          if(mHasOpenHead)
            mHasOpenHead=1;
          CloseHead(aNode); //do this just in case someone left it open...
          result=OpenBody(aNode); 
        }
        else isDefaultNode=PR_TRUE;
      }
      break;

    case eHTMLTag_counter: //drop it on the floor.
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
    
    case eHTMLTag_noscript:
      result=OpenNoscript(aNode,aStyleStack);
      break;

    default:
      isDefaultNode=PR_TRUE;
      break;
  }

  STOP_TIMER();
  MOZ_TIMER_DEBUGLOG(("Stop: Parse Time: CNavDTD::OpenContainer(), this=%p\n", this));

  if(isDefaultNode) {
      result=(mSink) ? mSink->OpenContainer(*aNode) : NS_OK; 
      mBodyContext->Push(aNode,aStyleStack);
  }

  MOZ_TIMER_DEBUGLOG(("Start: Parse Time: CNavDTD::OpenContainer(), this=%p\n", this));
  START_TIMER();

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
CNavDTD::CloseContainer(const nsIParserNode *aNode,eHTMLTags aTarget,PRBool aClosedByStartTag){
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
    
    case eHTMLTag_noscript:
      result=CloseNoscript(aNode);
      break;

    case eHTMLTag_title:
    default:

      STOP_TIMER();
      MOZ_TIMER_DEBUGLOG(("Stop: Parse Time: CNavDTD::CloseContainer(), this=%p\n", this));

      result=(mSink) ? mSink->CloseContainer(*aNode) : NS_OK; 

      MOZ_TIMER_DEBUGLOG(("Start: Parse Time: CNavDTD::CloseContainer(), this=%p\n", this));
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
nsresult CNavDTD::CloseContainersTo(PRInt32 anIndex,eHTMLTags aTarget, PRBool aClosedByStartTag){
  NS_PRECONDITION(mBodyContext->GetCount() > 0, kInvalidTagStackPos);
  nsresult result=NS_OK;
  

  if((anIndex<mBodyContext->GetCount()) && (anIndex>=0)) {

    PRInt32 count=0;
    while((count=mBodyContext->GetCount())>anIndex) {

      nsEntryStack  *theChildStyleStack=0;      
      eHTMLTags     theTag=mBodyContext->Last();
      nsCParserNode *theNode=(nsCParserNode*)mBodyContext->Pop(theChildStyleStack);
//      eHTMLTags     theParent=mBodyContext->Last();

      if(theNode) {
        result=CloseContainer(theNode,aTarget,aClosedByStartTag);
        
#ifdef  ENABLE_RESIDUALSTYLE

        PRBool theTagIsStyle=nsHTMLElement::IsResidualStyleTag(theTag);
        // If the current tag cannot leak out then we shouldn't leak out of the target - Fix 40713
        PRBool theStyleDoesntLeakOut = gHTMLElements[theTag].HasSpecialProperty(kNoStyleLeaksOut);
        if(!theStyleDoesntLeakOut) {
          theStyleDoesntLeakOut = gHTMLElements[aTarget].HasSpecialProperty(kNoStyleLeaksOut);
        }
        //  (aClosedByStartTag) ? gHTMLElements[aTarget].HasSpecialProperty(kNoStyleLeaksOut) 
        //                      :  gHTMLElements[theParent].HasSpecialProperty(kNoStyleLeaksOut);

        
          /*************************************************************
            I've added a check (mhasOpenNoXXX) below to prevent residual
            style handling from getting invoked in these cases. 
            This fixes bug 25214.
           *************************************************************/

        if(theTagIsStyle && (0==mHasOpenNoXXX)) {

          PRBool theTargetTagIsStyle=nsHTMLElement::IsResidualStyleTag(aTarget);

          if(aClosedByStartTag) { 

            /***********************************************************
              Handle closure due to new start tag.          

              The cases we're handing here:
                1. <body><b><DIV>       //<b> gets pushed onto <body>.mStyles.
                2. <body><a>text<a>     //in this case, the target matches, so don't push style
             ***************************************************************************/

            if(0==theNode->mUseCount){
              if(theTag!=aTarget) {
                  //don't push if thechild==theTarget
                if(theChildStyleStack) {
                  theChildStyleStack->PushFront(theNode);
                }
                else mBodyContext->PushStyle(theNode);
              }
            } 
            else if((theTag==aTarget) && (!gHTMLElements[aTarget].CanContainSelf())) {
              //here's a case we missed:  <a><div>text<a>text</a></div>
              //The <div> pushes the 1st <a> onto the rs-stack, then the 2nd <a>
              //pops the 1st <a> from the rs-stack altogether.
              mBodyContext->PopStyle(theTag);
            }


            if(theChildStyleStack) {
              mBodyContext->PushStyles(theChildStyleStack);
            }
          }
          else { //Handle closure due to another close tag.      

            /***********************************************************             
              if you're here, then we're dealing with the closure of tags
              caused by a close tag (as opposed to an open tag).
              At a minimum, we should consider pushing residual styles up 
              up the stack or popping and recycling displaced nodes.

              Known cases: 
                1. <body><b><div>text</DIV> 
                      Here the <b> will leak into <div> (see case given above), and 
                      when <div> closes the <b> is dropped since it's already residual.

                2. <body><div><b>text</div>
                      Here the <b> will leak out of the <div> and get pushed onto
                      the RS stack for the <body>, since it originated in the <div>.

                3. <body><span><b>text</span>
                      In this case, the the <b> get's pushed onto the style stack.
                      Later we deal with RS styles stored on the <span>

                4. <body><span><b>text</i>
                      Here we the <b>is closed by a (synonymous) style tag. 
                      In this case, the <b> is simply closed.
             ***************************************************************************/

            if(theChildStyleStack) {
              if(!theStyleDoesntLeakOut) {
                if(theTag!=aTarget) {
                  if (0==theNode->mUseCount) {
                    theChildStyleStack->PushFront(theNode);
                  }
                }
                else if(1==theNode->mUseCount) {
                  // This fixes bug 30885 and 29626
                  // Make sure that the node, which is about to
                  // get released does not stay on the style stack...
                  mBodyContext->PopStyle(theTag);
                }
                mBodyContext->PushStyles(theChildStyleStack);
              }
              else{
                //add code here to recycle styles...
                RecycleNodes(theChildStyleStack);
                delete theChildStyleStack; // XXX try to recycle this...
                theChildStyleStack=0;
              }
            }
            else if (0==theNode->mUseCount) {

              //The old version of this only pushed if the targettag wasn't style.
              //But that misses this case: <font><b>text</font>, where the b should leak
              if(aTarget!=theTag) {
                mBodyContext->PushStyle(theNode);
              }
            } 
            else {
              //Ah, at last, the final case. If you're here, then we just popped a 
              //style tag that got onto that tag stack from a stylestack somewhere.
              //Pop it from the stylestack if the target is also a style tag.
              if(theTargetTagIsStyle) {
                theNode=(nsCParserNode*)mBodyContext->PopStyle(theTag);
              }
            }
          }
        } //if
        else { 
          //the tag is not a style tag...
          if(theChildStyleStack) {
            if(theStyleDoesntLeakOut) {
              RecycleNodes(theChildStyleStack);
              delete theChildStyleStack; // XXX try to recycle this...
              theChildStyleStack=0;
            }
            else mBodyContext->PushStyles(theChildStyleStack);
          }
        }
#endif
      }//if anode
      mNodeRecycler->RecycleNode(theNode);
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
nsresult CNavDTD::CloseContainersTo(eHTMLTags aTarget,PRBool aClosedByStartTag){
  NS_PRECONDITION(mBodyContext->GetCount() > 0, kInvalidTagStackPos);

  PRInt32 pos=mBodyContext->LastOf(aTarget);

  if(kNotFound!=pos) {
    //the tag is indeed open, so close it.
    return CloseContainersTo(pos,aTarget,aClosedByStartTag);
  }

  eHTMLTags theTopTag=mBodyContext->Last();

  PRBool theTagIsSynonymous=((nsHTMLElement::IsResidualStyleTag(aTarget)) && (nsHTMLElement::IsResidualStyleTag(theTopTag)));
  if(!theTagIsSynonymous){
    theTagIsSynonymous=((nsHTMLElement::IsHeadingTag(aTarget)) && (nsHTMLElement::IsHeadingTag(theTopTag)));  
  }

  if(theTagIsSynonymous) {
    //if you're here, it's because we're trying to close one tag,
    //but a different (synonymous) one is actually open. Because this is NAV4x
    //compatibililty mode, we must close the one that's really open.
    aTarget=theTopTag;    
    pos=mBodyContext->LastOf(aTarget);
    if(kNotFound!=pos) {
      //the tag is indeed open, so close it.
      return CloseContainersTo(pos,aTarget,aClosedByStartTag);
    }
  }
  
  nsresult result=NS_OK;
  TagList* theRootTags=gHTMLElements[aTarget].GetRootTags();
  eHTMLTags theParentTag=(theRootTags) ? theRootTags->mTags[0] : eHTMLTag_unknown;
  pos=mBodyContext->LastOf(theParentTag);
  if(kNotFound!=pos) {
    //the parent container is open, so close it instead
    result=CloseContainersTo(pos+1,aTarget,aClosedByStartTag);
  }
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
nsresult CNavDTD::AddLeaf(const nsIParserNode *aNode){
  nsresult result=NS_OK;
  
  if(mSink){
    eHTMLTags theTag=(eHTMLTags)aNode->GetNodeType();
    OpenTransientStyles(theTag); 

    STOP_TIMER();
    MOZ_TIMER_DEBUGLOG(("Stop: Parse Time: CNavDTD::AddLeaf(), this=%p\n", this));
    
    result=mSink->AddLeaf(*aNode);
    
    if(NS_SUCCEEDED(result)) {
      PRBool done=PR_FALSE;
      eHTMLTags thePrevTag=theTag;
      //nsCParserNode theNode;

      while(!done && NS_SUCCEEDED(result)) {
        CToken*   theToken=mTokenizer->PeekToken();
        if(theToken) {
          nsCParserNode* theNode=mNodeRecycler->CreateNode();
          if(theNode) {
            theTag=(eHTMLTags)theToken->GetTypeID();
            switch(theTag) {
              case eHTMLTag_newline:
                mLineNumber++;
              case eHTMLTag_whitespace:
                {
                  theToken=mTokenizer->PopToken();
                  theNode->Init(theToken,mLineNumber,0);

                  STOP_TIMER();
                  MOZ_TIMER_DEBUGLOG(("Stop: Parse Time: CNavDTD::AddLeaf(), this=%p\n", this));
              
                  result=mSink->AddLeaf(*theNode);

                  if((NS_SUCCEEDED(result))||(NS_ERROR_HTMLPARSER_BLOCK==result)) {
                    IF_FREE(theToken);
                  }
                
                  MOZ_TIMER_DEBUGLOG(("Start: Parse Time: CNavDTD::AddLeaf(), this=%p\n", this));
                  START_TIMER();
                  thePrevTag=theTag;
                }
                break;
              case eHTMLTag_text:
                if((mHasOpenBody) && (!mHasOpenHead) &&
                  !(nsHTMLElement::IsWhitespaceTag(thePrevTag))) {
                  theToken=mTokenizer->PopToken();
                  theNode->Init(theToken,mLineNumber);

                  MOZ_TIMER_DEBUGLOG(("Stop: Parse Time: CNavDTD::AddLeaf(), this=%p\n", this));
                  STOP_TIMER();

                  mLineNumber += theToken->mNewlineCount;
                  result=mSink->AddLeaf(*theNode);

                  if((NS_SUCCEEDED(result))||(NS_ERROR_HTMLPARSER_BLOCK==result)) {
                    IF_FREE(theToken);
                  }
              
                  MOZ_TIMER_DEBUGLOG(("Start: Parse Time: CNavDTD::AddLeaf(), this=%p\n", this));
                  START_TIMER();
                }
                else done=PR_TRUE;
                break;

              default:
                done=PR_TRUE;
            } //switch
            mNodeRecycler->RecycleNode(theNode);
          } //if
        }//if
        else done=PR_TRUE;
      } //while
    } //if

    MOZ_TIMER_DEBUGLOG(("Start: Parse Time: CNavDTD::AddLeaf(), this=%p\n", this));
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
nsresult CNavDTD::AddHeadLeaf(nsIParserNode *aNode){
  nsresult result=NS_OK;

  static eHTMLTags gNoXTags[]={eHTMLTag_noembed,eHTMLTag_noframes};

  eHTMLTags theTag=(eHTMLTags)aNode->GetNodeType();
  
  // XXX - SCRIPT inside NOTAGS should not get executed unless the pref.
  // says so.  Since we don't have this support yet..lets ignore the
  // SCRIPT inside NOTAGS.  Ref Bug 25880.
  if(eHTMLTag_meta==theTag || eHTMLTag_script==theTag) {
    if(HasOpenContainer(gNoXTags,sizeof(gNoXTags)/sizeof(eHTMLTag_unknown))) {
      return result;
    }
  }

  if(mSink) {
    // Alternate content => Content that shouldn't get processed
    // as a regular content. That is, probably the content is
    // within NOSCRIPT and since JS is enanbled we should not process
    // this content. However, when JS is disabled alternate content
    // would become regular content.
    if(mDTDState!=NS_HTMLPARSER_ALTERNATECONTENT) {
      result=OpenHead(aNode);
      if(NS_OK==result) {
        if(eHTMLTag_title==theTag) {

          const nsString& theString=aNode->GetSkippedContent();
          PRInt32 theLen=theString.Length();
          CBufDescriptor theBD(theString.GetUnicode(), PR_TRUE, theLen+1, theLen);
          nsAutoString theString2(theBD);

          theString2.CompressWhitespace();

          STOP_TIMER()
          MOZ_TIMER_DEBUGLOG(("Stop: Parse Time: CNavDTD::AddHeadLeaf(), this=%p\n", this));
          mSink->SetTitle(theString2);
          MOZ_TIMER_DEBUGLOG(("Start: Parse Time: CNavDTD::AddHeadLeaf(), this=%p\n", this));
          START_TIMER()

        }
        else result=AddLeaf(aNode);
        // XXX If the return value tells us to block, go
        // ahead and close the tag out anyway, since its
        // contents will be consumed.

        // Fix for Bug 31392
        // Do not leave a head context open no matter what the result is.
        if(mHasOpenHead) {
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
    else result=AddLeaf(aNode);
  }
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
nsresult CNavDTD::CreateContextStackFor(eHTMLTags aChildTag){
  
  mScratch.Truncate();
  
  nsresult  result=(nsresult)kContextMismatch;
  eHTMLTags theTop=mBodyContext->Last();
  PRBool    bResult=ForwardPropagate(mScratch,theTop,aChildTag);
  
  if(PR_FALSE==bResult){

    if(eHTMLTag_unknown!=theTop) {
      if(theTop!=aChildTag) //dont even bother if we're already inside a similar element...
        bResult=BackwardPropagate(mScratch,theTop,aChildTag);      
    } //if
    else bResult=BackwardPropagate(mScratch,eHTMLTag_html,aChildTag);
  } //elseif

  PRInt32   theLen=mScratch.Length();
  eHTMLTags theTag=(eHTMLTags)mScratch[--theLen];

  if((0==mBodyContext->GetCount()) || (mBodyContext->Last()==theTag))
    result=NS_OK;

  //now, build up the stack according to the tags 
  //you have that aren't in the stack...
  if(PR_TRUE==bResult){
    while(theLen) {
      theTag=(eHTMLTags)mScratch[--theLen];

#ifdef ALLOW_TR_AS_CHILD_OF_TABLE
      if((eHTML3Text==mDocType) && (eHTMLTag_tbody==theTag)) {
        //the prev. condition prevents us from emitting tbody in html3.2 docs; fix bug 30378
        continue;
      }
#endif
      CStartToken *theToken=(CStartToken*)mTokenAllocator->CreateTokenOfType(eToken_start,theTag);
      HandleToken(theToken,mParser);  //these should all wind up on contextstack, so don't recycle.
    }
    result=NS_OK;
  }
  return result;
}

/**
 * Retrieve the preferred tokenizer for use by this DTD.
 * @update  gess12/28/98
 * @param   none
 * @return  ptr to tokenizer
 */
nsresult CNavDTD::GetTokenizer(nsITokenizer*& aTokenizer) {
  nsresult result=NS_OK;
  if(!mTokenizer) {
    result=NS_NewHTMLTokenizer(&mTokenizer,mDTDMode,mDocType,mParserCommand);
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
nsTokenAllocator* CNavDTD::GetTokenAllocator(void){
  if(!mTokenAllocator) {
    nsresult result=GetTokenizer(mTokenizer);
    if (NS_SUCCEEDED(result)) {
      mTokenAllocator=mTokenizer->GetTokenAllocator();
    }
  }
  return mTokenAllocator;
}

/**
 * 
 * @update  gess5/18/98
 * @param 
 * @return
 */
nsresult CNavDTD::WillResumeParse(void){

  STOP_TIMER();
  MOZ_TIMER_DEBUGLOG(("Stop: Parse Time: CNavDTD::WillResumeParse(), this=%p\n", this));

  nsresult result=(mSink) ? mSink->WillResume() : NS_OK; 

  MOZ_TIMER_DEBUGLOG(("Start: Parse Time: CNavDTD::WillResumeParse(), this=%p\n", this));
  START_TIMER();

  return result;
}

/**
 * This method gets called when the parsing process is interrupted
 * due to lack of data (waiting for netlib).
 * @update  gess5/18/98
 * @return  error code
 */
nsresult CNavDTD::WillInterruptParse(void){

  STOP_TIMER();
  MOZ_TIMER_DEBUGLOG(("Stop: Parse Time: CNavDTD::WillInterruptParse(), this=%p\n", this));

  nsresult result=(mSink) ? mSink->WillInterrupt() : NS_OK; 

  MOZ_TIMER_DEBUGLOG(("Start: Parse Time: CNavDTD::WillInterruptParse(), this=%p\n", this));
  START_TIMER();

  return result;
}


/**
 * 
 * @update  gpk03/14/99
 * @param 
 * @return
 */
nsresult CNavDTD::DoFragment(PRBool aFlag) 
{
  return NS_OK;
}

