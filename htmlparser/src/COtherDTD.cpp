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
#define ENABLE_RESIDUALSTYLE  
#ifdef  RICKG_DEBUG
#include  <fstream.h>  
#endif
     
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



/***************************************************************
  This the ITagHandler deque deallocator, needed by the 
  CTagHandlerRegister
 ***************************************************************/
class CTagHandlerDeallocator: public nsDequeFunctor{
public:
  virtual void* operator()(void* aObject) {
    nsITagHandler*  tagHandler = (nsITagHandler*)aObject;
    delete tagHandler;
    return 0;
  }
};

/***************************************************************
  This funtor will be called for each item in the TagHandler que to 
  check for a Tag name, and setting the current TagHandler when it is reached
 ***************************************************************/
class CTagFinder: public nsDequeFunctor{

public:
  CTagFinder(){}
  void Initialize(const nsString &aTagName) {mTagName.Assign(aTagName);}

  virtual ~CTagFinder() {
  }

  virtual void* operator()(void* aObject) {
    nsString* theString = ((nsITagHandler*)aObject)->GetString();
    if( theString->Equals(mTagName)){
      return aObject;
    }
    return(0); 
  }

  nsAutoString  mTagName;
};

/***************************************************************
  This a an object that will keep track of TagHandlers in 
  the DTD.  Uses a factory pattern
 ***************************************************************/
class CTagHandlerRegister {
public:
  
  CTagHandlerRegister();

  ~CTagHandlerRegister();

  void RegisterTagHandler(nsITagHandler *aTagHandler){
    mTagHandlerDeque.Push(aTagHandler);
  }
  
  nsITagHandler*  FindTagHandler(const nsString &aTagName){
    nsITagHandler* foundHandler = nsnull;

    mTagFinder.Initialize(aTagName);
    mTagHandlerDeque.Begin();
    foundHandler = (nsITagHandler*) mTagHandlerDeque.FirstThat(mTagFinder);
    return foundHandler;
  }
  

  nsDeque                 mTagHandlerDeque;
  CTagFinder              mTagFinder;
};

MOZ_DECL_CTOR_COUNTER(CTagHandlerRegister);

CTagHandlerRegister::CTagHandlerRegister() : mTagHandlerDeque(new CTagHandlerDeallocator())
{
  MOZ_COUNT_CTOR(CTagHandlerRegister);
}

CTagHandlerRegister::~CTagHandlerRegister() 
{
  MOZ_COUNT_DTOR(CTagHandlerRegister);
}

/************************************************************************
  The CTagHandlerRegister for a COtherDTD.
  This is where special taghanders for our tags can be managed and called from
  Note:  This can also be attached to some object so it can be refcounted 
  and destroyed if you want this to go away when not imbedded.
 ************************************************************************/
//CTagHandlerRegister gTagHandlerRegister;


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
  mMapContext=0;
  mTempContext=0;
  mTokenizer=0;
  mComputedCRC32=0;
  mExpectedCRC32=0;
  mDTDState=NS_OK;
  mStyleHandlingEnabled=PR_TRUE;
  mDocType=ePlainText;

  if(!gHTMLElements) {
    InitializeElementTable();
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

  if(mTempContext)
    delete mTempContext;

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

  
  if(eViewSource!=aParserContext.mParserCommand) {
    if(PR_TRUE==(aParserContext.mMimeType.EqualsWithConversion(kHTMLTextContentType) || aParserContext.mMimeType.EqualsWithConversion(kPlainTextContentType))) {
      result=ePrimaryDetect;
    }
    else {
      //otherwise, look into the buffer to see if you recognize anything...
      PRBool theBufHasXML=PR_FALSE;
      if(BufferContainsHTML(aBuffer,theBufHasXML)){
        result = eValidDetect ;
        if(0==aParserContext.mMimeType.Length()) {
          aParserContext.SetMimeType( NS_ConvertToString(kHTMLTextContentType) );
          result = (theBufHasXML) ? eValidDetect : ePrimaryDetect;
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

  mStyleHandlingEnabled=(eParseMode_quirks==mParseMode);
    
  if((!aParserContext.mPrevContext) && (aSink)) {

    STOP_TIMER();
    MOZ_TIMER_DEBUGLOG(("Stop: Parse Time: COtherDTD::WillBuildModel(), this=%p\n", this));

    mTokenRecycler=0;
    mStyleHandlingEnabled=PR_TRUE;

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

      mTokenizer->PrependTokens(mMisplacedContent); //push misplaced content 

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
          if(mSkipTarget) {
            CHTMLToken* theEndToken=nsnull;
            theEndToken=(CHTMLToken*)mTokenRecycler->CreateTokenOfType(eToken_end,mSkipTarget);
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
            RecycleNode(theNode);
            if(theChildStyles) {
              delete theChildStyles;
            }
          } 

        }

        STOP_TIMER();
        MOZ_TIMER_DEBUGLOG(("Stop: Parse Time: COtherDTD::DidBuildModel(), this=%p\n", this));

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

        MOZ_TIMER_DEBUGLOG(("Start: Parse Time: COtherDTD::DidBuildModel(), this=%p\n", this));
        START_TIMER();

          //Now make sure the misplaced content list is empty,
          //by forcefully recycling any tokens we might find there.

        CToken* theToken=0;
        while((theToken=(CToken*)mMisplacedContent.Pop())) {
          mTokenRecycler->RecycleToken(theToken);
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
nsresult COtherDTD::HandleToken(CToken* aToken,nsIParser* aParser){
  nsresult  result=NS_OK;

  if(aToken) {
    CHTMLToken*     theToken= (CHTMLToken*)(aToken);
    eHTMLTokenTypes theType=eHTMLTokenTypes(theToken->GetTokenType());
    eHTMLTags       theTag=(eHTMLTags)theToken->GetTypeID();
    PRBool          execSkipContent=PR_FALSE;

    theToken->mUseCount=0;  //assume every token coming into this system needs recycling.

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
        mTokenRecycler->RecycleToken(aToken);
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
    if(!execSkipContent) {

      switch(theTag) {
        case eHTMLTag_html:
        case eHTMLTag_comment:
        case eHTMLTag_script:
        case eHTMLTag_markupDecl:
        case eHTMLTag_userdefined:
          break;  //simply pass these through to token handler without further ado...

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
                mMisplacedContent.Push(aToken);
                aToken->mUseCount++;
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

          case eToken_style:
            result=HandleStyleToken(theToken); break;

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

        /*************************************************************/
         // CAUTION: Here we are forgetting to push the ATTRIBUTE Tokens.
         //          So, before you uncomment this part please make sure
         //          that the attribute tokens are also accounted for.
        
         //else if(NS_ERROR_HTMLPARSER_MISPLACED!=result)
         //  mTokenizer->PushTokenFront(theToken);
         //else result=NS_OK;
        /***************************************************************/
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
 
/**
 *  Determine whether the given tag is open anywhere
 *  in our context stack.
 *  
 *  @update  gess 4/2/98
 *  @param   eHTMLTags tag to be searched for in stack
 *  @return  topmost index of tag on stack
 */
PRInt32 COtherDTD::LastOf(eHTMLTags aTagSet[],PRInt32 aCount) const {
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

  PRBool result=PR_TRUE;
  if(aContext.GetCount()){
    TagList* theRootTags=gHTMLElements[aChildTag].GetRootTags();
    TagList* theSpecialParents=gHTMLElements[aChildTag].GetSpecialParents();
    if(theRootTags) {
      PRInt32 theRootIndex=LastOf(aContext,*theRootTags);
      PRInt32 theSPIndex=(theSpecialParents) ? LastOf(aContext,*theSpecialParents) : kNotFound;  
      PRInt32 theChildIndex=GetIndexOfChildOrSynonym(aContext,aChildTag);
      PRInt32 theBaseIndex=(theRootIndex>theSPIndex) ? theRootIndex : theSPIndex;

      if((theBaseIndex==theChildIndex) && (gHTMLElements[aChildTag].CanContainSelf()))
        result=PR_TRUE;
      else result=PRBool(theBaseIndex>theChildIndex);
    }
  }

  return result;
}

enum eProcessRule {eIgnore,eTest};

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
nsresult COtherDTD::HandleDefaultStartToken(CToken* aToken,eHTMLTags aChildTag,nsIParserNode *aNode) {
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

    eProcessRule theRule=eTest; 
    switch(theRule){
      case eTest:        
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
            if ((theParentTag!=aChildTag) && (!nsHTMLElement::IsResidualStyleTag(aChildTag))) { 
              
              PRInt32 theChildIndex=GetIndexOfChildOrSynonym(*mBodyContext,aChildTag);
              
              if((kNotFound<theChildIndex) && (theChildIndex<theIndex)) {

              /*-------------------------------------------------------------------------------------
                Here's a tricky case from bug 22596:  <h5><li><h5>

                How do we know that the 2nd <h5> should close the <LI> rather than nest inside the <LI>?
                (Afterall, the <h5> is a legal child of the <LI>).
              
                The way you know is that there is no root between the two, so the <h5> binds more
                tightly to the 1st <h5> than to the <LI>.
               -------------------------------------------------------------------------------------*/

                theChildAgrees=CanBeContained(aChildTag,*mBodyContext);
              } //if
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
      case eIgnore:
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
nsresult COtherDTD::WillHandleStartTag(CToken* aToken,eHTMLTags aTag,nsCParserNode& aNode){
  nsresult result=NS_OK; 
  PRInt32 theAttrCount  = aNode.GetAttributeCount(); 

    //first let's see if there's some skipped content to deal with... 
  if(gHTMLElements[aTag].mSkipTarget) { 
    result=CollectSkippedContent(aNode,theAttrCount); 
  } 
  
  STOP_TIMER()
  MOZ_TIMER_DEBUGLOG(("Stop: Parse Time: COtherDTD::WillHandleStartTag(), this=%p\n", this));

  if(mParser) {

    CObserverService* theService=mParser->GetObserverService();
    if(theService) {
      CParserContext*   pc=mParser->PeekContext();
      void*             theDocID=(pc)? pc->mKey:0;
    
      result=theService->Notify(aTag,aNode,(PRUint32)theDocID, NS_ConvertToString(kHTMLTextContentType), mParser);                            
    }
  }

  MOZ_TIMER_DEBUGLOG(("Start: Parse Time: COtherDTD::WillHandleStartTag(), this=%p\n", this));
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

    PRBool theExclusive=PR_FALSE;
    PRBool isHeadChild=gHTMLElements[eHTMLTag_head].IsChildOfHead(aTag,theExclusive);

      //this code is here to make sure the head is closed before we deal 
      //with any tags that don't belong in the head.
    if(NS_OK==result) {
      if(mHasOpenHead){
        static eHTMLTags skip2[]={eHTMLTag_newline,eHTMLTag_whitespace};
        if(!FindTagInSet(aTag,skip2,sizeof(skip2)/sizeof(eHTMLTag_unknown))){
          if(!isHeadChild){      

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

void PushMisplacedAttributes2(nsIParserNode& aNode,nsDeque& aDeque,PRInt32& aCount) {
  if(aCount > 0) {
    CToken* theAttrToken=nsnull;
    nsCParserNode* theAttrNode = (nsCParserNode*)&aNode;
    if(theAttrNode) {
      while(aCount){ 
        theAttrToken=theAttrNode->PopAttributeToken();
        if(theAttrToken) {
          aDeque.Push(theAttrToken);
          theAttrToken->mUseCount=0;
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
nsresult COtherDTD::HandleOmittedTag(CToken* aToken,eHTMLTags aChildTag,eHTMLTags aParent,nsIParserNode* aNode) {
  NS_PRECONDITION(mBodyContext != nsnull,"need a context to work with");

  nsresult  result=NS_OK;

  //The trick here is to see if the parent can contain the child, but prefers not to.
  //Only if the parent CANNOT contain the child should we look to see if it's potentially a child
  //of another section. If it is, the cache it for later.
  //  1. Get the root node for the child. See if the ultimate node is the BODY, FRAMESET, HEAD or HTML
  PRInt32   theTagCount = mBodyContext->GetCount();
  CToken*   theToken    = aToken;

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

      PRBool done=PR_FALSE;
      if(theIndex>kNotFound) {
        // Avoid TD and TH getting into misplaced content stack
        // because they are legal table elements. -- Ref: Bug# 20797
        static eHTMLTags gLegalElements[]={eHTMLTag_td,eHTMLTag_th};
        while(!done){
          mMisplacedContent.Push(theToken);  
          theToken->mUseCount++;

          // If the token is attributed then save those attributes too.
          if(attrCount > 0) PushMisplacedAttributes2(*aNode,mMisplacedContent,attrCount);
         
          theToken=mTokenizer->PeekToken();
          
          if(theToken) {
            theToken->mUseCount=0;
            theTag=(eHTMLTags)theToken->GetTypeID();
            if(!nsHTMLElement::IsWhitespaceTag(theTag) && theTag!=eHTMLTag_unknown) {
              if((gHTMLElements[theTag].mSkipTarget && theToken->GetTokenType() != eToken_end) ||
                 (FindTagInSet(theTag,gLegalElements,sizeof(gLegalElements)/sizeof(theTag))) ||
                 (gHTMLElements[theTag].HasSpecialProperty(kBadContentWatch))                ||
                 (gHTMLElements[aParent].CanContain(theTag))) {
                  done=PR_TRUE;
              }            
            }
            if(!done) theToken=mTokenizer->PopToken();
          }
          else done=PR_TRUE;
        }//while
        if(result==NS_OK) {
          result=HandleSavedTokens(mBodyContext->mContextTopIndex=theIndex);
          mBodyContext->mContextTopIndex=0;
        }
      }//if
    }//if

    if((aChildTag!=aParent) && (gHTMLElements[aParent].HasSpecialProperty(kSaveMisplaced))) {
      mMisplacedContent.Push(aToken);
      aToken->mUseCount++;
      // If the token is attributed then save those attributes too.
       if(attrCount > 0) PushMisplacedAttributes2(*aNode,mMisplacedContent,attrCount);
    }
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
      PRBool isTokenHandled =PR_FALSE;
      PRBool theHeadIsParent=PR_FALSE;

      if(nsHTMLElement::IsSectionTag(theChildTag)){
        switch(theChildTag){
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
          MOZ_TIMER_DEBUGLOG(("Stop: Parse Time: COtherDTD::HandleStartToken(), this=%p\n", this));
          
          if (mHasOpenMap && mSink) {
            result=mSink->AddLeaf(*theNode);
            isTokenHandled=PR_TRUE;
          }
          
          MOZ_TIMER_DEBUGLOG(("Start: Parse Time: COtherDTD::HandleStartToken(), this=%p\n", this));
          START_TIMER();
          break;

        case eHTMLTag_image:
          aToken->SetTypeID(theChildTag=eHTMLTag_img);
          break;

        //case eHTMLTag_userdefined:
        case eHTMLTag_noscript:     //HACK XXX! Throw noscript on the floor for now.
          isTokenHandled=PR_TRUE;
          break;

        case eHTMLTag_script:
          theHeadIsParent=(!mHasOpenBody);
          mHasOpenScript=PR_TRUE;

        default:
            break;
      }//switch

      if(!isTokenHandled) {
        if((theHeadIsParent && theExclusive) || 
           (mHasOpenHead && ((eHTMLTag_newline==theChildTag) || (eHTMLTag_whitespace==theChildTag)))) {
              result=AddHeadLeaf(theNode);
        }
        else result=HandleDefaultStartToken(aToken,theChildTag,theNode); 
      }
      //now do any post processing necessary on the tag...
      if(NS_OK==result)
        DidHandleStartTag(*theNode,theChildTag);
    }//if
  } //if

  RecycleNode(theNode);
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
      if(thePrevTag==aContext[theChildIndex]){
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
void StripWSFollowingTag2(eHTMLTags aChildTag,nsITokenizer* aTokenizer,nsITokenRecycler* aRecycler, PRInt32& aNewlineCount){ 
  CToken* theToken= (aTokenizer)? aTokenizer->PeekToken():nsnull; 

  if(aRecycler) { 
    while(theToken) { 
      eHTMLTokenTypes theType=eHTMLTokenTypes(theToken->GetTokenType()); 
      switch(theType) { 
        case eToken_newline: aNewlineCount++; 
        case eToken_whitespace: 
          aRecycler->RecycleToken(aTokenizer->PopToken()); 
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
nsresult COtherDTD::HandleEndToken(CToken* aToken) {
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
      StripWSFollowingTag2(theChildTag,mTokenizer,mTokenRecycler,mLineNumber);
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
        if(eParseMode_quirks==mParseMode) {
          CStartToken theToken(theChildTag);  //leave this on the stack; no recycling necessary.
          theToken.mUseCount=1;
          result=HandleStartToken(&theToken);
        }
      }
      break;

    case eHTMLTag_body:
    case eHTMLTag_html:
      StripWSFollowingTag2(theChildTag,mTokenizer,mTokenRecycler,mLineNumber);
      break;

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

            PopStyle(theChildTag);

            // If the bit kHandleStrayTag is set then we automatically open up a matching
            // start tag ( compatibility ).  Currently this bit is set on P tag.
            // This also fixes Bug: 22623
            if(gHTMLElements[theChildTag].HasSpecialProperty(kHandleStrayTag) &&
               mParseMode!=eParseMode_noquirks) {
              // Oh boy!! we found a "stray" tag. Nav4.x and IE introduce line break in
              // such cases. So, let's simulate that effect for compatibility.
              // Ex. <html><body>Hello</P>There</body></html>
              PRBool theParentContains=-1; //set to -1 to force canomit to recompute.
              if(!CanOmit(theParentTag,theChildTag,theParentContains)) {
                mTokenizer->PushTokenFront(aToken); //put this end token back...
                CHTMLToken* theToken = (CHTMLToken*)mTokenRecycler->CreateTokenOfType(eToken_start,theChildTag);
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
nsresult COtherDTD::HandleSavedTokens(PRInt32 anIndex) {
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
        eHTMLTags theParentTag= mBodyContext->TagAt(anIndex);

        STOP_TIMER()
        MOZ_TIMER_DEBUGLOG(("Stop: Parse Time: COtherDTD::HandleSavedTokensAbove(), this=%p\n", this));     
        // Pause the main context and switch to the new context.
        mSink->BeginContext(anIndex);
        MOZ_TIMER_DEBUGLOG(("Start: Parse Time: COtherDTD::HandleSavedTokensAbove(), this=%p\n", this));
        START_TIMER()
      
        // The body context should contain contents only upto the marked position.  
        PRInt32 i=0;
        nsEntryStack* theChildStyleStack=0;

        for(i=0; i<(theTagCount - theTopIndex); i++) {
          mTempContext->Push((nsCParserNode*)mBodyContext->Pop(theChildStyleStack));
        }
     
        // Now flush out all the bad contents.
        while(theBadTokenCount > 0){
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
            }
            // Make sure that the BeginContext() is ended only by the call to
            // EndContext(). 
            if(theTag!=theParentTag || eToken_end!=theToken->GetTokenType())
              result=HandleToken(theToken,mParser);
            else mTokenRecycler->RecycleToken(theToken);
          }
          theBadTokenCount--;
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
        MOZ_TIMER_DEBUGLOG(("Stop: Parse Time: COtherDTD::HandleSavedTokensAbove(), this=%p\n", this));     
        // Terminate the new context and switch back to the main context
        mSink->EndContext(anIndex);
        MOZ_TIMER_DEBUGLOG(("Start: Parse Time: COtherDTD::HandleSavedTokensAbove(), this=%p\n", this));
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
nsresult COtherDTD::HandleEntityToken(CToken* aToken) {
  NS_PRECONDITION(0!=aToken,kNullToken);

  nsresult  result=NS_OK;
  eHTMLTags theParentTag=mBodyContext->Last();

  nsCParserNode* theNode=CreateNode();
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
 *  This method gets called when a style token has been 
 *  encountered in the parse process. 
 *  
 *  @update  gess 3/25/98
 *  @param   aToken -- next (start) token to be handled
 *  @return  PR_TRUE if all went well; PR_FALSE if error occured
 */
nsresult COtherDTD::HandleStyleToken(CToken* aToken){
  NS_PRECONDITION(0!=aToken,kNullToken);

//  CStyleToken*  st = (CStyleToken*)(aToken);
  return NS_OK;
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
  return gHTMLElements[aParent].CanContain((eHTMLTags)aChild);
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
 *  This method is called to determine whether or not
 *  the necessary intermediate tags should be propagated
 *  between the given parent and given child.
 *
 *  @update  gess 4/8/98
 *  @param   aParent -- tag enum of parent container
 *  @param   aChild -- tag enum of child container
 *  @return  PR_TRUE if propagation should occur
 */
PRBool COtherDTD::CanPropagate(eHTMLTags aParentTag,eHTMLTags aChildTag,PRBool aParentContains)  {
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
PRBool COtherDTD::CanOmit(eHTMLTags aParent,eHTMLTags aChild,PRBool& aParentContains)  {

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
      return PR_TRUE;
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
PRBool COtherDTD::IsContainer(PRInt32 aTag) const {
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
PRBool COtherDTD::ForwardPropagate(nsString& aSequence,eHTMLTags aParentTag,eHTMLTags aChildTag)  {
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
PRBool COtherDTD::BackwardPropagate(nsString& aSequence,eHTMLTags aParentTag,eHTMLTags aChildTag) const {

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

/**
 *  This method retrieves the HTMLTag type of the topmost
 *  container on the stack.
 *  
 *  @update  gess 4/2/98
 *  @return  tag id of topmost node in contextstack
 */
eHTMLTags COtherDTD::GetTopNode() const {
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
nsresult COtherDTD::OpenTransientStyles(eHTMLTags aChildTag){
  nsresult result=NS_OK;

  //later, change this so that transients only open in containers that get leaked in to.

  if(mStyleHandlingEnabled  && (eHTMLTag_newline!=aChildTag)) {

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

          PRUint32 scount=theStack->mCount;
          PRUint32 sindex=0;

          nsTagEntry *theEntry=theStack->mEntries;
          for(sindex=0;sindex<scount;sindex++){            
            nsCParserNode* theNode=(nsCParserNode*)theEntry->mNode;
            if(1==theNode->mUseCount) {
              theEntry->mParent=theStack;  //we do this too, because this entry differs from the new one we're pushing...              
              result=OpenContainer(theNode,(eHTMLTags)theNode->GetNodeType(),PR_FALSE,theStack);
            }
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
nsresult COtherDTD::CloseTransientStyles(eHTMLTags aChildTag){
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
nsresult COtherDTD::PopStyle(eHTMLTags aTag){
  nsresult result=0;

  if(mStyleHandlingEnabled) {
#ifdef  ENABLE_RESIDUALSTYLE
    if(nsHTMLElement::IsResidualStyleTag(aTag)) {
      nsCParserNode* theNode=(nsCParserNode*)mBodyContext->PopStyle(aTag);
      if(theNode) {
        RecycleNode(theNode);
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


  if (nsHTMLElement::IsResidualStyleTag(aTag))
    OpenTransientStyles(aTag); 

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

      nsEntryStack  *theChildStyleStack=0;      
      eHTMLTags     theTag=mBodyContext->Last();
      nsCParserNode *theNode=(nsCParserNode*)mBodyContext->Pop(theChildStyleStack);
//      eHTMLTags     theParent=mBodyContext->Last();

      if(theNode) {
        result=CloseContainer(theNode,aTarget,aClosedByStartTag);
        
#ifdef  ENABLE_RESIDUALSTYLE

        PRBool theTagIsStyle=nsHTMLElement::IsResidualStyleTag(theTag);
        PRBool theStyleDoesntLeakOut = gHTMLElements[aTarget].HasSpecialProperty(kNoStyleLeaksOut);
        //  (aClosedByStartTag) ? gHTMLElements[aTarget].HasSpecialProperty(kNoStyleLeaksOut) 
        //                      :  gHTMLElements[theParent].HasSpecialProperty(kNoStyleLeaksOut);

        if(theTagIsStyle) {

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
                mBodyContext->PushStyles(theChildStyleStack);
              }
              else{
                //add code here to recycle styles...
                RecycleNodes(theChildStyleStack);
              }
            }
            else if (0==theNode->mUseCount) {

              if(!theTargetTagIsStyle) {
                mBodyContext->PushStyle(theNode);
              } //otherwise just let the node recycle, because it was explicty closed
                //and does not live on a style stack.
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
            }
            else mBodyContext->PushStyles(theChildStyleStack);
          }
        }
#endif
      }//if anode
      RecycleNode(theNode);
    }


    if(eParseMode_quirks==mParseMode) {

#if 0 

      //This code takes any nodes in style stack for at this level and reopens
      //then where they were originally.

      if(!aClosedByStartTag) {
        nsEntryStack* theStack=mBodyContext->GetStylesAt(anIndex-1);
        if(theStack){

          PRUint32 scount=theStack->mCount;
          PRUint32 sindex=0;

          for(sindex=0;sindex<scount;sindex++){
            nsIParserNode* theNode=theStack->NodeAt(sindex);
            if(theNode) {
              ((nsCParserNode*)theNode)->mUseCount--;
              result=OpenContainer(theNode,(eHTMLTags)theNode->GetNodeType(),PR_FALSE);
            }
          } 
          theStack->mCount=0; 
        } //if
      } //if
#endif
    }//if

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
nsresult COtherDTD::AddLeaf(const nsIParserNode *aNode){
  nsresult result=NS_OK;
  
  if(mSink){
    eHTMLTags theTag=(eHTMLTags)aNode->GetNodeType();
    OpenTransientStyles(theTag); 

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
                !(nsHTMLElement::IsWhitespaceTag(thePrevTag))) {
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

        const nsString& theString=aNode->GetSkippedContent();
        PRInt32 theLen=theString.Length();
        CBufDescriptor theBD(theString.GetUnicode(), PR_TRUE, theLen+1, theLen);
        nsAutoString theString2(theBD);

        theString2.CompressWhitespace();

        STOP_TIMER()
        MOZ_TIMER_DEBUGLOG(("Stop: Parse Time: COtherDTD::AddHeadLeaf(), this=%p\n", this));
        mSink->SetTitle(theString2);
        MOZ_TIMER_DEBUGLOG(("Start: Parse Time: COtherDTD::AddHeadLeaf(), this=%p\n", this));
        START_TIMER()

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
nsresult COtherDTD::CreateContextStackFor(eHTMLTags aChildTag){
  
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
      CStartToken *theToken=(CStartToken*)mTokenRecycler->CreateTokenOfType(eToken_start,theTag);
      HandleStartToken(theToken);  //these should all wind up on contextstack, so don't recycle.
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
nsresult COtherDTD::WillResumeParse(void){

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


/**
 * 
 * @update  gpk03/14/99
 * @param 
 * @return
 */
nsresult COtherDTD::DoFragment(PRBool aFlag) 
{
  return NS_OK;
}

