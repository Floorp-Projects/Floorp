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
    
#include "nsDebug.h" 
#include "nsIDTDDebug.h"  
#include "CNavDTD.h" 
#include "nsHTMLTokens.h"
#include "nsCRT.h"
#include "nsParser.h"
#include "nsIParser.h"
#include "nsIHTMLContentSink.h" 
#include "nsScanner.h"
#include "nsTokenHandler.h"
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

#ifdef XP_PC
#include <direct.h> //this is here for debug reasons...
#endif
#include "prmem.h"

#undef  ENABLE_RESIDUALSTYLE  
#define RICKG_DEBUG 
#ifdef  RICKG_DEBUG
#include  <fstream.h>  
#endif


static NS_DEFINE_IID(kIHTMLContentSinkIID, NS_IHTML_CONTENT_SINK_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);                 
static NS_DEFINE_IID(kIDTDIID,      NS_IDTD_IID);
static NS_DEFINE_IID(kClassIID,     NS_INAVHTML_DTD_IID); 
 
static const  char* kNullToken = "Error: Null token given";
static const  char* kInvalidTagStackPos = "Error: invalid tag stack position";
static char*        kVerificationDir = "c:/temp";
static char         gShowCRC=0;
static CTokenRecycler* gRecycler=0;
 

static eHTMLTags gFormElementTags[]= {  
    eHTMLTag_button,  eHTMLTag_fieldset,  eHTMLTag_input,
    eHTMLTag_isindex, eHTMLTag_label,     eHTMLTag_legend,
    eHTMLTag_option,  eHTMLTag_optgroup,  eHTMLTag_select,
    eHTMLTag_textarea};

  
static eHTMLTags gWhitespaceTags[]={
  eHTMLTag_newline, eHTMLTag_whitespace};



#include "nsElementTable.h"


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
  void Initialize(const nsString &aTagName) {mTagName = aTagName;}

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
  
  CTagHandlerRegister() : mTagHandlerDeque(new CTagHandlerDeallocator()) {
  }

  ~CTagHandlerRegister() {
  }

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


/************************************************************************
  The CTagHandlerRegister for a CNavDTD.
  This is where special taghanders for our tags can be managed and called from
  Note:  This can also be attached to some object so it can be refcounted 
  and destroyed if you want this to go away when not imbedded.
 ************************************************************************/
//CTagHandlerRegister gTagHandlerRegister;


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


/**
 *  This method is defined in nsIParser. It is used to 
 *  cause the COM-like construction of an nsParser.
 *  
 *  @update  gess 4/8/98
 *  @param   nsIParser** ptr to newly instantiated parser
 *  @return  NS_xxx error result
 */
NS_HTMLPARS nsresult NS_NewNavHTMLDTD(nsIDTD** aInstancePtrResult)
{
  CNavDTD* it = new CNavDTD();

  if (it == 0) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return it->QueryInterface(kClassIID, (void **) aInstancePtrResult);
}


NS_IMPL_ADDREF(CNavDTD)
NS_IMPL_RELEASE(CNavDTD)



/**
 *  
 *  
 *  @update  gess 6/9/98
 *  @param   
 *  @return  
 */
static
PRInt32 NavDispatchTokenHandler(CToken* aToken,nsIDTD* aDTD) {
  PRInt32         result=0;
  CHTMLToken*     theToken= (CHTMLToken*)(aToken);
  eHTMLTokenTypes theType= (eHTMLTokenTypes)theToken->GetTokenType();
  CNavDTD*        theDTD=(CNavDTD*)aDTD;
  
  if(aDTD) {
    switch(theType) {
      case eToken_start:
      case eToken_whitespace: 
      case eToken_newline:
      case eToken_text:
        result=theDTD->HandleStartToken(aToken); break;
      case eToken_end:
        result=theDTD->HandleEndToken(aToken); break;
      case eToken_comment:
        result=theDTD->HandleCommentToken(aToken); break;
      case eToken_entity:
        result=theDTD->HandleEntityToken(aToken); break;
      case eToken_attribute:
        result=theDTD->HandleAttributeToken(aToken); break;
      case eToken_style:
        result=theDTD->HandleStyleToken(aToken); break;
      case eToken_instruction:
        result=theDTD->HandleProcessingInstructionToken(aToken); break;
      default:
        result=0;
    }//switch
  }//if
  return result;
}

/**
 *  Register a handler.
 *  
 *  @update  gess 4/2/98
 *  @param   
 *  @return  
 */
CITokenHandler* CNavDTD::AddTokenHandler(CITokenHandler* aHandler) {
  NS_ASSERTION(0!=aHandler,"Error: Null handler");
  
  if(aHandler)  {
    eHTMLTokenTypes type=(eHTMLTokenTypes)aHandler->GetTokenType();
    if(type<eToken_last) {
      mTokenHandlers[type]=aHandler;
    }
    else {
      //add code here to handle dynamic tokens...
    }
  }
  return 0;
}

/**
 *  init the set of default token handlers...
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  
 */
void CNavDTD::InitializeDefaultTokenHandlers() {
  AddTokenHandler(new CTokenHandler(NavDispatchTokenHandler,eToken_start));

  AddTokenHandler(new CTokenHandler(NavDispatchTokenHandler,eToken_end));
  AddTokenHandler(new CTokenHandler(NavDispatchTokenHandler,eToken_comment));
  AddTokenHandler(new CTokenHandler(NavDispatchTokenHandler,eToken_entity));

  AddTokenHandler(new CTokenHandler(NavDispatchTokenHandler,eToken_whitespace));
  AddTokenHandler(new CTokenHandler(NavDispatchTokenHandler,eToken_newline));
  AddTokenHandler(new CTokenHandler(NavDispatchTokenHandler,eToken_text));
  
  AddTokenHandler(new CTokenHandler(NavDispatchTokenHandler,eToken_attribute));
//  AddTokenHandler(new CTokenHandler(NavDispatchTokenHandler,eToken_script));
  AddTokenHandler(new CTokenHandler(NavDispatchTokenHandler,eToken_style));
  AddTokenHandler(new CTokenHandler(NavDispatchTokenHandler,eToken_skippedcontent));
  AddTokenHandler(new CTokenHandler(NavDispatchTokenHandler,eToken_instruction));
}

/**
 *  Finds a tag handler for the given tag type, given in string.
 *  
 *  @update  gess 4/2/98
 *  @param   aString contains name of tag to be handled
 *  @return  valid tag handler (if found) or null
 */
void CNavDTD::DeleteTokenHandlers(void) {
  for(int i=eToken_unknown;i<eToken_last;i++){
    delete mTokenHandlers[i];
    mTokenHandlers[i]=0;
  }
}


/**
 *  Default constructor
 *   
 *  @update  gess 4/9/98
 *  @param   
 *  @return  
 */
CNavDTD::CNavDTD() : nsIDTD(), mMisplacedContent(0), mSkippedContent(0), mSharedNodes(0) {
  NS_INIT_REFCNT();
  mSink = 0; 
  mParser=0;       
  mDTDDebug=0;   
  mLineNumber=1;  
  nsCRT::zero(mTokenHandlers,sizeof(mTokenHandlers));
  mHasOpenBody=PR_FALSE;
  mHasOpenHead=0;
  mHasOpenForm=PR_FALSE;
  mHasOpenMap=PR_FALSE;
  InitializeDefaultTokenHandlers(); 
  mHeadContext=new nsDTDContext();
  mBodyContext=new nsDTDContext();
  mFormContext=0;
  mMapContext=0;
  mTokenizer=0;
  mComputedCRC32=0;
  mExpectedCRC32=0;
  mSaveBadTokens = PR_FALSE;
  mDTDState=NS_OK;
//  DebugDumpContainmentRules2(*this,"c:/temp/DTDRules.new","New CNavDTD Containment Rules");
#ifdef  RICKG_DEBUG
  nsHTMLElement::DebugDumpContainment("c:/temp/rules.new","ElementTable Rules");
  nsHTMLElement::DebugDumpMembership("c:/temp/table.out");
  nsHTMLElement::DebugDumpContainType("c:/temp/ctnrules.out");
#endif
}


nsCParserNode* CNavDTD::CreateNode(void) {
  nsCParserNode* result=0;
  if(0<mSharedNodes.GetSize()) {
    result=(nsCParserNode*)mSharedNodes.Pop();
  }
  else{
    result=new nsCParserNode();
  }
  return result;
}

void CNavDTD::RecycleNode(nsCParserNode* aNode) {
  if(aNode) {

    CToken* theToken=0;
    while((theToken=(CToken*)aNode->PopAttributeToken())){
      gRecycler->RecycleToken(theToken);
    }

    mSharedNodes.Push(aNode);
  }
}

/**
 * 
 * @update	gess1/8/99
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
  DeleteTokenHandlers();
  delete mHeadContext;
  delete mBodyContext;
  if(mTokenizer)
    delete (nsHTMLTokenizer*)mTokenizer;
  nsCParserNode* theNode=0;
  while((theNode=(nsCParserNode*)mSharedNodes.Pop())){
    delete theNode;
  }
  NS_IF_RELEASE(mSink);
  NS_IF_RELEASE(mDTDDebug);
}
 

/**
 * Call this method if you want the DTD to construct a fresh 
 * instance of itself. 
 * @update  gess7/23/98
 * @param 
 * @return
 */
nsresult CNavDTD::CreateNewInstance(nsIDTD** aInstancePtrResult){
  return NS_NewNavHTMLDTD(aInstancePtrResult);
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
 * @update  gess6/24/98
 * @param   
 * @return  TRUE if this DTD can satisfy the request; FALSE otherwise.
 */
eAutoDetectResult CNavDTD::CanParse(nsString& aContentType, nsString& aCommand, nsString& aBuffer, PRInt32 aVersion) {
  eAutoDetectResult result=eUnknownDetect;

  if(!aCommand.Equals(kViewSourceCommand)) {
    if(PR_TRUE==aContentType.Equals(kHTMLTextContentType)) {
      result=ePrimaryDetect;
    }
    else {
      //otherwise, look into the buffer to see if you recognize anything...
      if(BufferContainsHTML(aBuffer)){
        result=ePrimaryDetect;
        if(0==aContentType.Length())
          aContentType=kHTMLTextContentType;
      }
    }
  }
  return result;
}


PRTime  gStartTime;

/**
 * 
 * @update  gess5/18/98
 * @param 
 * @return
 */
nsresult CNavDTD::WillBuildModel(nsString& aFilename,PRBool aNotifySink,nsString& aSourceType,nsIContentSink* aSink){
  nsresult result=NS_OK;

  mFilename=aFilename;
  mHasOpenBody=PR_FALSE;
  mHadBody=PR_FALSE;
  mHadFrameset=PR_FALSE;
  mLineNumber=1;
  mHasOpenScript=PR_FALSE;
  if((aNotifySink) && (aSink)) {

    if(aSink && (!mSink)) {
      result=aSink->QueryInterface(kIHTMLContentSinkIID, (void **)&mSink);
    }


#ifdef RGESS_DEBUG
    gStartTime = PR_Now();
    printf("Begin parsing...\n");
#endif

    result = aSink->WillBuildModel();
    CStartToken theToken(eHTMLTag_html);
    HandleStartToken(&theToken);

    mSkipTarget=eHTMLTag_unknown;
    mComputedCRC32=0;
    mExpectedCRC32=0;
  }
  return result;
}


/**
  * This is called when it's time to read as many tokens from the tokenizer
  * as you can. Not all tokens may make sense, so you may not be able to
  * read them all (until more come in later).
  *
  * @update	gess5/18/98
  * @param	aParser is the parser object that's driving this process
  * @return	error code (almost always NS_OK)
  */
nsresult CNavDTD::BuildModel(nsIParser* aParser,nsITokenizer* aTokenizer,nsITokenObserver* anObserver,nsIContentSink* aSink) {
  nsresult result=NS_OK;
   
  if(aTokenizer) {
    nsITokenizer*  oldTokenizer=mTokenizer;
    mTokenizer=aTokenizer;
    mParser=(nsParser*)aParser;

    if(mSink) {
      gRecycler=(CTokenRecycler*)mTokenizer->GetTokenRecycler();
      while(NS_SUCCEEDED(result)){
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
      CStartToken theToken(eHTMLTag_body);  //open the body container... 
      result=HandleStartToken(&theToken); 
      mTokenizer->PrependTokens(mMisplacedContent); //push misplaced content 
      result=BuildModel(aParser,mTokenizer,0,aSink); 
    } 

    if(aParser){ 
      if(aNotifySink){ 
        if((NS_OK==anErrorCode) && (mBodyContext->GetCount()>0)) { 
          eHTMLTags theTarget; 
          while(mBodyContext->GetCount() > 0) { 
            theTarget = mBodyContext->Last(); 
            if(gHTMLElements[theTarget].HasSpecialProperty(kBadContentWatch)) 
              result = HandleSavedTokensAbove(theTarget); 
            CloseContainersTo(theTarget,PR_FALSE); 
          } 
          //result = CloseContainersTo(0,eHTMLTag_unknown,PR_FALSE); 
        } 

  #ifdef RGESS_DEBUG 
        PRTime theEnd= PR_Now(); 
        PRTime creates, ustoms; 
        LL_I2L(ustoms, 1000); 
        LL_SUB(creates, theEnd, gStartTime); 
        LL_DIV(creates, creates, ustoms); 
        printf("End parse elapsed: %lldms\n",creates); 
  #endif 

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

        if(mDTDDebug) { 
          mDTDDebug->DumpVectorRecord(); 
        } 
      } 
    } 
  }
  return result;
}

/**
 *  This big dispatch method is used to route token handler calls to the right place.
 *  What's wrong with it? This table, and the dispatch methods themselves need to be 
 *  moved over to the delegate. Ah, so much to do...
 *  
 *  @update  gess 5/21/98
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

    theToken->mRecycle=PR_TRUE;  //assume every token coming into this system needs recycling.

    /* ---------------------------------------------------------------------------------
       To understand this little piece of code, you need to look below too.
       In essence, this code caches "skipped content" until we find a given skiptarget.
       Once we find the skiptarget, we take all skipped content up to that point and
       coallate it. Then we push those tokens back onto the tokenizer deque.
       ---------------------------------------------------------------------------------
     */
     if(mSkipTarget){  //handle a preexisting target...
      if((theTag==mSkipTarget) && (eToken_end==theType)){
        mSkipTarget=eHTMLTag_unknown; //stop skipping.  
        //mTokenizer->PushTokenFront(aToken); //push the end token...
        execSkipContent=PR_TRUE;
        gRecycler->RecycleToken(aToken);
        theToken=(CHTMLToken*)mSkippedContent.PopFront();
        // result=HandleStartToken(theToken);
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
      static eHTMLTags passThru[]= {
        eHTMLTag_html,eHTMLTag_comment,eHTMLTag_newline,
        eHTMLTag_whitespace,eHTMLTag_script,eHTMLTag_noscript,
        eHTMLTag_userdefined};
      if(!FindTagInSet(theTag,passThru,sizeof(passThru)/sizeof(eHTMLTag_unknown))){
        if(!gHTMLElements[eHTMLTag_html].SectionContains(theTag,PR_FALSE)) {
          if((!mHadBody) && (!mHadFrameset)){
            if(mHasOpenHead) {
              //just fall through and handle current token
              if(!gHTMLElements[eHTMLTag_head].IsChildOfHead(theTag)){
                mMisplacedContent.Push(aToken);
                aToken->mRecycle=PR_FALSE;
                return result;
              }
            }
            else {
              if(gHTMLElements[eHTMLTag_body].SectionContains(theTag,PR_TRUE)){
                mTokenizer->PushTokenFront(aToken); //put this token back...
                mTokenizer->PrependTokens(mMisplacedContent); //push misplaced content
                theToken=(CHTMLToken*)gRecycler->CreateTokenOfType(eToken_start,eHTMLTag_body);
                //now open a body...
              }
            }
          } 
        }
      }
    }
    
    if(theToken){
      //Before dealing with the token normally, we need to deal with skip targets
      if((!execSkipContent) && 
         (theType!=eToken_end) &&
         (eHTMLTag_unknown==mSkipTarget) && 
         (gHTMLElements[theTag].mSkipTarget)){ //create a new target
        mSkipTarget=gHTMLElements[theTag].mSkipTarget;
        mSkippedContent.Push(theToken);
      }
      else {

        CITokenHandler* theHandler=GetTokenHandler(theType);
        if(theHandler) {
          mParser=(nsParser*)aParser;
          result=(*theHandler)(theToken,this);
          if(NS_SUCCEEDED(result) || (NS_ERROR_HTMLPARSER_BLOCK==result)) {
            if(theToken->mRecycle)
              gRecycler->RecycleToken(theToken);
          }
          else if(result==NS_ERROR_HTMLPARSER_STOPPARSING)
            return result;
          else return NS_OK;
          /*************************************************************/
           // CAUTION: Here we are forgetting to push the ATTRIBUTE Tokens.
           //          So, before you uncomment this part please make sure
           //          that the attribute tokens are also accounted for.
          
           //else if(NS_ERROR_HTMLPARSER_MISPLACED!=result)
           //  mTokenizer->PushTokenFront(theToken);
           //else result=NS_OK;
          /***************************************************************/
          if (mDTDDebug) {
             //mDTDDebug->Verify(this, mParser, mBodyContext->GetCount(), mBodyContext->mStack, mFilename);
          }
        } //if
      }
    }

  }//if
  return result;
}

/**
 *  This method causes all tokens to be dispatched to the given tag handler.
 *
 *  @update  gess 3/25/98
 *  @param   aHandler -- object to receive subsequent tokens...
 *  @return	 error code (usually 0)
 */
nsresult CNavDTD::CaptureTokenPump(nsITagHandler* aHandler) {
  nsresult result=NS_OK;
  return result;
}

/**
 *  This method releases the token-pump capture obtained in CaptureTokenPump()
 *
 *  @update  gess 3/25/98
 *  @param   aHandler -- object that received tokens...
 *  @return	 error code (usually 0)
 */
nsresult CNavDTD::ReleaseTokenPump(nsITagHandler* aHandler){
  nsresult result=NS_OK;
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

  switch(aChildTag){

    case eHTMLTag_pre:
    case eHTMLTag_listing:
      {
        CToken* theNextToken=mTokenizer->PeekToken();
        if(theNextToken)  {
          eHTMLTokenTypes theType=eHTMLTokenTypes(theNextToken->GetTokenType());
          if(eToken_newline==theType){
            mTokenizer->PopToken();  //skip 1st newline inside PRE and LISTING
          }//if
        }//if
      }
      break;

    case eHTMLTag_plaintext:
    case eHTMLTag_xmp:
      //grab the skipped content and dump it out as text...
      {
        const nsString& theText=aNode.GetSkippedContent();
        if(0<theText.Length()) {
          CViewSourceHTML::WriteText(theText,*mSink,PR_TRUE,PR_FALSE);
        }
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
#if 0
static
PRInt32 GetTopmostIndexOf(eHTMLTags aTag,nsEntryStack& aTagStack) {
  int i=0;
  int count = aTagStack.GetCount();
  for(i=(count-1);i>=0;i--){
    if(aTagStack[i]==aTag)
      return i;
  }
  return kNotFound;
}
#endif

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
PRInt32 GetIndexOfChildOrSynonym(nsEntryStack& aTagStack,eHTMLTags aChildTag) {
  PRInt32 theChildIndex=aTagStack.GetTopmostIndexOf(aChildTag);
  if(kNotFound==theChildIndex) {
    CTagList* theSynTags=gHTMLElements[aChildTag].GetSynonymousTags(); //get the list of tags that THIS tag can close
    if(theSynTags) {
      theChildIndex=theSynTags->GetTopmostIndexOf(aTagStack);
    } 
    else{
      theChildIndex=aTagStack.GetCount();
      PRInt32 theGroup=nsHTMLElement::GetSynonymousGroups(gHTMLElements[aChildTag].mParentBits);
      while(-1<--theChildIndex) {
        eHTMLTags theTag=aTagStack[theChildIndex];
        if(gHTMLElements[theTag].IsMemberOf(theGroup)) {
          break;   
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
PRBool CanBeContained(eHTMLTags aChildTag,nsEntryStack& aTagStack) {

  /* #    Interesting test cases:       Result:
   * 1.   <UL><LI>..<B>..<LI>           inner <LI> closes outer <LI>
   * 2.   <CENTER><DL><DT><A><CENTER>   allow nested <CENTER>
   * 3.   <TABLE><TR><TD><TABLE>...     allow nested <TABLE>
   * 4.   <FRAMESET> ... <FRAMESET>
   */

  //Note: This method is going away. First we need to get the elementtable to do closures right, and
  //      therefore we must get residual style handling to work.

  PRBool result=PR_TRUE;
  if(aTagStack.GetCount()){
    CTagList* theRootTags=gHTMLElements[aChildTag].GetRootTags();
    CTagList* theSpecialParents=gHTMLElements[aChildTag].GetSpecialParents();
    if(theRootTags) {
      PRInt32 theRootIndex=theRootTags->GetTopmostIndexOf(aTagStack);          
      PRInt32 theSPIndex=(theSpecialParents) ? theSpecialParents->GetTopmostIndexOf(aTagStack) : kNotFound;  
      PRInt32 theChildIndex=GetIndexOfChildOrSynonym(aTagStack,aChildTag);
      PRInt32 theBaseIndex=(theRootIndex>theSPIndex) ? theRootIndex : theSPIndex;

      if((theBaseIndex==theChildIndex) && (gHTMLElements[aChildTag].CanContainSelf()))
        result=PR_TRUE;
      else result=PRBool(theBaseIndex>theChildIndex);
    }
  }

  return result;
}

enum eProcessRule {eIgnore,eTest};

eProcessRule GetProcessRule(eHTMLTags aParentTag,eHTMLTags aChildTag){
  int mParentGroup=gHTMLElements[aParentTag].mParentBits;
  int mChildGroup=gHTMLElements[aChildTag].mParentBits;

  eProcessRule result=eTest;

  switch(mParentGroup){
    case kSpecial:
    case kPhrase:
    case kFontStyle:
    case kFormControl:
      switch(mChildGroup){
        case kBlock:
        case kHTMLContent:
        case kExtensions:
        //case kFlowEntity:
        case kList:
        case kBlockEntity:
        case kHeading:
        case kHeadMisc:
        case kPreformatted:
        case kNone:
          result=eIgnore;
      }
      break;

    default:
      break;
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
nsresult CNavDTD::HandleDefaultStartToken(CToken* aToken,eHTMLTags aChildTag,nsIParserNode& aNode) {
  NS_PRECONDITION(0!=aToken,kNullToken);

  nsresult  result=NS_OK;

  PRBool  theCanContainResult=PR_FALSE;
  PRBool  theChildAgrees=PR_TRUE;
  PRInt32 theIndex=mBodyContext->GetCount();
  
  do {

    eHTMLTags theParentTag=mBodyContext->TagAt(--theIndex);
    if(CanOmit(theParentTag,aChildTag)) {
      result=HandleOmittedTag(aToken,aChildTag,theParentTag,aNode);
      return result;
    }

    eProcessRule theRule=eTest; //GetProcessRule(theParentTag,aChildTag);
    switch(theRule){
      case eTest:
        theCanContainResult=CanContain(theParentTag,aChildTag);
        theChildAgrees=PR_TRUE;
        if(theCanContainResult) {

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

          if(theChildAgrees) {
              //This code is here to allow DT/DD (or similar) to close arbitrary stuff between
              //this instance and a prior one on the stack. I had to add this because
              //(for now) DT is inline, and fontstyle's can contain them (until Res-style code works)
            if(gHTMLElements[aChildTag].HasSpecialProperty(kMustCloseSelf)){
              theChildAgrees=CanBeContained(aChildTag,mBodyContext->mStack);
            }
          }
        }

        if(!(theCanContainResult && theChildAgrees)) {
          if (!CanPropagate(theParentTag,aChildTag)) { 
            if(nsHTMLElement::IsContainer(aChildTag)){ 
              if(gHTMLElements[aChildTag].HasSpecialProperty(kDiscardMisplaced)) {
                // Closing the tags above might cause non-compatible results.
                // Ex. <TABLE><TR><TD><TBODY>Text</TD></TR></TABLE>. 
                // In the example above <TBODY> is badly misplaced. To be backwards
                // compatible we should not attempt to close the tags above it, for
                // the contents inside the table might get thrown out of the table.
                // The safest thing to do is to discard this tag.
                return result;
              }
              CloseContainersTo(theIndex,theParentTag,PR_TRUE);
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
  } while(!(theCanContainResult && theChildAgrees));


  if(nsHTMLElement::IsContainer(aChildTag)){
    result=OpenContainer(aNode,PR_TRUE);
  }
  else {  //we're writing a leaf...
    result=AddLeaf(aNode);
  }

  return result;
}


#ifdef  RICKG_DEBUG
void WriteTokenToLog(CToken* aToken) {

  static fstream outputStream("c:/temp/tokenlog.html",ios::out);
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

 /********************************************************** 
     THIS WILL ULTIMATELY BECOME THE REAL OBSERVER API... 
   **********************************************************/ 
  nsDeque*  theDeque= (mParser && aTag != eHTMLTag_unknown)?  (mParser->GetObserverDictionary()).GetObserversForTag(aTag):nsnull;
  if(theDeque){ 
    PRUint32 theDequeSize=theDeque->GetSize(); 
    if(0<theDequeSize){
      PRInt32 index = 0; 
      const PRUnichar* theKeys[50]  = {0,0,0,0,0}; // XXX -  should be dynamic
      const PRUnichar* theValues[50]= {0,0,0,0,0}; // XXX -  should be dynamic
      for(index=0; index<theAttrCount && index < 50; index++) {
        theKeys[index]   = aNode.GetKeyAt(index).GetUnicode(); 
        theValues[index] = aNode.GetValueAt(index).GetUnicode(); 
      } 
      nsAutoString charsetValue;
      nsCharsetSource charsetSource;
      nsAutoString theCharsetKey("charset"); 
      nsAutoString theSourceKey("charsetSource"); 
      nsAutoString intValue;
      mParser->GetDocumentCharset(charsetValue, charsetSource);
      // Add pseudo attribute in the end
      if(index < 50) {
        theKeys[index]=theCharsetKey.GetUnicode(); 
        theValues[index] = charsetValue.GetUnicode();
        index++;
      }
      if(index < 50) {
        theKeys[index]=theSourceKey.GetUnicode(); 
        PRInt32 sourceInt = charsetSource;
        intValue.Append(sourceInt,10);
        theValues[index] = intValue.GetUnicode();
	  	  index++;
      }
      nsAutoString theTagStr(nsHTMLTags::GetStringValue(aTag));
      CParserContext* pc=mParser->PeekContext(); 
      void* theDocID=(pc) ? pc-> mKey : 0; 
      nsObserverNotifier theNotifier(theTagStr.GetUnicode(),(PRUint32)theDocID,index,theKeys,theValues);
      theDeque->FirstThat(theNotifier); 
      result=theNotifier.mResult; 
     }//if 
  } 


  if(eHTMLTag_meta==aTag) { 
    PRInt32 theCount=aNode.GetAttributeCount(); 
    if(1<theCount){ 
  
      const nsString& theKey=aNode.GetKeyAt(0); 
      if(theKey.EqualsIgnoreCase("NAME")) { 
        const nsString& theValue1=aNode.GetValueAt(0); 
        if(theValue1.EqualsIgnoreCase("\"CRC\"")) { 
          const nsString& theKey2=aNode.GetKeyAt(1); 
          if(theKey2.EqualsIgnoreCase("CONTENT")) { 
            const nsString& theValue2=aNode.GetValueAt(1); 
            PRInt32 err=0; 
            mExpectedCRC32=theValue2.ToInteger(&err); 
          } //if 
        } //if 
      } //else 

    } //if 
  }//if 

  if(NS_OK==result) {
    result=gHTMLElements[aTag].HasSpecialProperty(kDiscardTag) ? 1 : NS_OK;
  }

  PRBool isHeadChild=gHTMLElements[eHTMLTag_head].IsChildOfHead(aTag);

    //this code is here to make sure the head is closed before we deal 
    //with any tags that don't belong in the head.
  if(NS_OK==result) {
    if(mHasOpenHead){
      static eHTMLTags skip2[]={eHTMLTag_newline,eHTMLTag_whitespace};
      if(!FindTagInSet(aTag,skip2,sizeof(skip2)/sizeof(eHTMLTag_unknown))){
        if(!isHeadChild){      
          CEndToken     theToken(eHTMLTag_head);
          nsCParserNode theNode(&theToken,mLineNumber);
          result=CloseHead(theNode);
        }
      }
    }
  }

  return result;
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
nsresult CNavDTD::HandleOmittedTag(CToken* aToken,eHTMLTags aChildTag,eHTMLTags aParent,nsIParserNode& aNode) {
  nsresult  result=NS_OK;

  //The trick here is to see if the parent can contain the child, but prefers not to.
  //Only if the parent CANNOT contain the child should we look to see if it's potentially a child
  //of another section. If it is, the cache it for later.
  //  1. Get the root node for the child. See if the ultimate node is the BODY, FRAMESET, HEAD or HTML
  PRInt32   theTagCount = mBodyContext->GetCount();
  PRInt32   attrCount   = aToken->GetAttributeCount();

  if(gHTMLElements[aParent].HasSpecialProperty(kBadContentWatch)) {
    eHTMLTags theTag;
    PRInt32   theBCIndex;
    PRBool    isNotWhiteSpace = PR_FALSE;
    
    while(theTagCount > 0) {
      theTag = mBodyContext->TagAt(--theTagCount);
      if(!gHTMLElements[theTag].HasSpecialProperty(kBadContentWatch)) {
        if(!gHTMLElements[theTag].CanContain(aChildTag))
          return result;
        theBCIndex = theTagCount;
        break;
      }
    }
    if(!FindTagInSet(aChildTag,gWhitespaceTags,sizeof(gWhitespaceTags)/sizeof(aChildTag))) {
      isNotWhiteSpace = mSaveBadTokens  = PR_TRUE;
    }
    if(mSaveBadTokens) {
      mBodyContext->SaveToken(aToken,theBCIndex);
      // If the token is attributed then save those attributes too.
      if(attrCount > 0) {
        nsCParserNode* theAttrNode = (nsCParserNode*)&aNode;
        while(attrCount > 0){ 
           mBodyContext->SaveToken(theAttrNode->PopAttributeToken(),theBCIndex);
           attrCount--;
        }
      }
      if(!IsContainer(aChildTag) && isNotWhiteSpace) {
        mSaveBadTokens = PR_FALSE;
      }
      result=NS_ERROR_HTMLPARSER_MISPLACED;
    }
  }

  if((aChildTag!=aParent) && (gHTMLElements[aParent].HasSpecialProperty(kSaveMisplaced))) {
    mMisplacedContent.Push(aToken);
    aToken->mRecycle=PR_FALSE;

    // If the token is attributed then save those attributes too.
    if(attrCount > 0) {
      nsCParserNode* theAttrNode = (nsCParserNode*)&aNode;
      while(attrCount > 0){ 
        CToken* theToken=theAttrNode->PopAttributeToken();
        if(theToken){
          mMisplacedContent.Push(theToken);
          theToken->mRecycle=PR_FALSE;
        }
        attrCount--;
      }
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
nsresult CNavDTD::HandleStartToken(CToken* aToken) {
  NS_PRECONDITION(0!=aToken,kNullToken);

  #ifdef  RICKG_DEBUG
    WriteTokenToLog(aToken);
  #endif

  //Begin by gathering up attributes...

  nsCParserNode* theNode=CreateNode();
  theNode->Init(aToken,mLineNumber,GetTokenRecycler());

  eHTMLTags     theChildTag=(eHTMLTags)aToken->GetTypeID();
  PRInt16       attrCount=aToken->GetAttributeCount();
  nsresult      result=(0==attrCount) ? NS_OK : CollectAttributes(*theNode,theChildTag,attrCount);
  eHTMLTags     theParent=mBodyContext->Last();

  if(NS_OK==result) {
    result=WillHandleStartTag(aToken,theChildTag,*theNode);
    if(NS_OK==result) {

      if(nsHTMLElement::IsSectionTag(theChildTag)){
        switch(theChildTag){
          case eHTMLTag_body:
           if(mHasOpenBody)
              return OpenContainer(*theNode,PR_FALSE);
            break;
          case eHTMLTag_head:
            if(mHadBody || mHadFrameset) {
              result=HandleOmittedTag(aToken,theChildTag,theParent,*theNode);
              if(result == NS_OK)
                return result;
            }
            break;
          default:
            break;
        }
      }

      PRBool theHeadIsParent=nsHTMLElement::IsChildOfHead(theChildTag);
      switch(theChildTag) { 

        case eHTMLTag_area:
          if (mHasOpenMap && mSink)
            result=mSink->AddLeaf(*theNode);
          break;

        case eHTMLTag_userdefined:
          break; //drop them on the floor for now...

        case eHTMLTag_script:
          theHeadIsParent=(!mHasOpenBody); //intentionally fall through...
          mHasOpenScript=PR_TRUE;

        default:
          {
            if(theHeadIsParent)
              result=AddHeadLeaf(*theNode);
            else result=HandleDefaultStartToken(aToken,theChildTag,*theNode); 
          }
          break;
      } //switch
      //now do any post processing necessary on the tag...
      if(NS_OK==result)
        DidHandleStartTag(*theNode,theChildTag);
    }
  } //if

  if(eHTMLTag_newline==theChildTag)
    mLineNumber++;

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
PRBool HasCloseablePeerAboveRoot(CTagList& aRootTagList,nsEntryStack& aTagStack,eHTMLTags aTag,PRBool anEndTag) {
  PRInt32 theRootIndex=aRootTagList.GetTopmostIndexOf(aTagStack);          
  CTagList* theCloseTags=(anEndTag) ? gHTMLElements[aTag].GetAutoCloseEndTags() : gHTMLElements[aTag].GetAutoCloseStartTags();
  PRInt32 theChildIndex=-1;

  if(theCloseTags) {
    theChildIndex=theCloseTags->GetTopmostIndexOf(aTagStack);
  }
  else {
    if((anEndTag) || (!gHTMLElements[aTag].CanContainSelf()))
      theChildIndex=aTagStack.GetTopmostIndexOf(aTag);
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
eHTMLTags FindAutoCloseTargetForEndTag(eHTMLTags aCurrentTag,nsEntryStack& aTagStack) {
  int theTopIndex=aTagStack.GetCount();
  eHTMLTags thePrevTag=aTagStack.Last();
 
  if(nsHTMLElement::IsContainer(aCurrentTag)){
    PRInt32 theChildIndex=GetIndexOfChildOrSynonym(aTagStack,aCurrentTag);
    
    if(kNotFound<theChildIndex) {
      if(thePrevTag==aTagStack[theChildIndex]){
        return aTagStack[theChildIndex];
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

        CTagList* theCloseTags=gHTMLElements[aCurrentTag].GetAutoCloseEndTags();
        CTagList* theRootTags=gHTMLElements[aCurrentTag].GetEndRootTags();
      
        if(theCloseTags){  
            //at a min., this code is needed for H1..H6
        
          while(theChildIndex<--theTopIndex) {
            eHTMLTags theNextTag=aTagStack[theTopIndex];
            if(PR_FALSE==theCloseTags->Contains(theNextTag)) {
              if(PR_TRUE==theRootTags->Contains(theNextTag)) {
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
          if(HasCloseablePeerAboveRoot(*theRootTags,aTagStack,aCurrentTag,PR_TRUE))
            return aCurrentTag;
          else return eHTMLTag_unknown;
        }
      } //if blockcloser
      else{
        //Ok, a much more sensible approach for non-block closers; use the tag group to determine closure:
        //For example: %phrasal closes %phrasal, %fontstyle and %special
        return gHTMLElements[aCurrentTag].GetCloseTargetForEndTag(aTagStack,theChildIndex);
      }
    }//if
  } //if
  return eHTMLTag_unknown;
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
  nsCParserNode theNode((CHTMLToken*)aToken,mLineNumber);

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

    case eHTMLTag_form:
    case eHTMLTag_head:
      result=CloseContainer(theNode,theChildTag,PR_FALSE);
      break;

    case eHTMLTag_br:
      {
        CToken* theToken=(CHTMLToken*)gRecycler->CreateTokenOfType(eToken_start,eHTMLTag_br);
        result=HandleStartToken(theToken);
      }
      break;
    
    default:
     {
        //now check to see if this token should be omitted, or 
        //if it's gated from closing by the presence of another tag.
        if(gHTMLElements[theChildTag].CanOmitEndTag()) {
          UpdateStyleStackForCloseTag(theChildTag,theChildTag);
        }
        else {
          if(kNotFound==GetIndexOfChildOrSynonym(mBodyContext->mStack,theChildTag)) {
            UpdateStyleStackForCloseTag(theChildTag,theChildTag);
            if(nsHTMLElement::IsBlockCloser(theChildTag)) {
              // Oh boy!! we found a "stray" block closer. Nav4.x and IE introduce line break in
              // such cases. So, let's simulate that effect for compatibility.
              // Ex. <html><body>Hello</P>There</body></html>
              CHTMLToken* theToken = (CHTMLToken*)gRecycler->CreateTokenOfType(eToken_start,theChildTag);
              result=HandleStartToken(theToken);
            }
            else return result;
          }
          if(result==NS_OK) {
            eHTMLTags theTarget=FindAutoCloseTargetForEndTag(theChildTag,mBodyContext->mStack);
            if(gHTMLElements[theTarget].HasSpecialProperty(kBadContentWatch)){
               result = HandleSavedTokensAbove(theTarget);
            }
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

nsresult CNavDTD::HandleSavedTokensAbove(eHTMLTags aTag)
{
    NS_PRECONDITION(mBodyContext != nsnull && mBodyContext->GetCount() > 0,"invalid context");

    CToken*   theToken;
    eHTMLTags theTag;
    PRInt32   attrCount;
    nsresult  result      = NS_OK;
    PRInt32   theTopIndex = GetTopmostIndexOf(aTag);
    PRInt32   theTagCount = mBodyContext->GetCount();

    if(theTopIndex == kNotFound)
      return result;

    PRInt32  theBadContentIndex = theTopIndex - 1;
    PRInt32  theBadTokenCount   = mBodyContext->TokenCountAt(theBadContentIndex);

    if(theBadTokenCount > 0) {
      // Pause the main context and switch to the new context.
      mSink->BeginContext(theBadContentIndex);

      nsDTDContext temp;
      
      // The body context should contain contents only upto the marked position.  
      PRInt32 i=0;
      for(i=0; i<(theTagCount - theTopIndex); i++)
        temp.Push(mBodyContext->Pop());
     
      // Now flush out all the bad contents.
      while(theBadTokenCount > 0){
        theToken       = mBodyContext->RestoreTokenFrom(theBadContentIndex);
        if(theToken) {
          theTag       = (eHTMLTags)theToken->GetTypeID();
          if(theTag != eHTMLTag_unknown) {
            attrCount    = theToken->GetAttributeCount();
            // Put back attributes, which once got popped out, into the tokenizer
            for(PRInt32 j=0;j<attrCount; j++){
              CToken* theAttrToken = mBodyContext->RestoreTokenFrom(theBadContentIndex);
              if(theAttrToken) {
                mTokenizer->PushTokenFront(theAttrToken);
              }
              theBadTokenCount--;
            }
            result = HandleStartToken(theToken);
          }
        }
        theBadTokenCount--;
      }
      if(theTopIndex != mBodyContext->GetCount()) {
         CloseContainersTo(theTopIndex,mBodyContext->TagAt(theTopIndex),PR_TRUE);
      }
     
      // Bad-contents were successfully processed. Now, itz time to get
      // back to the original body context state.
      for(PRInt32 k=0; k<(theTagCount - theTopIndex); k++)
        mBodyContext->Push(temp.Pop());
      
      // Terminate the new context and switch back to the main context
      mSink->EndContext(theBadContentIndex);
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

  nsresult      result=NS_OK;

  if(PR_FALSE==CanOmit(mBodyContext->Last(),eHTMLTag_entity)) {
    nsCParserNode aNode((CHTMLToken*)aToken,mLineNumber);

  #ifdef  RICKG_DEBUG
    WriteTokenToLog(aToken);
  #endif

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
nsresult CNavDTD::HandleCommentToken(CToken* aToken) {
  NS_PRECONDITION(0!=aToken,kNullToken);
  
  mLineNumber += (aToken->GetStringValueXXX()).CountChar(kNewLine);
  nsCParserNode aNode((CHTMLToken*)aToken,mLineNumber);

  #ifdef  RICKG_DEBUG
    WriteTokenToLog(aToken);
  #endif

  nsresult result=(mSink) ? mSink->AddComment(aNode) : NS_OK;  
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
nsresult CNavDTD::HandleScriptToken(nsCParserNode& aNode) {
  // PRInt32 attrCount=aNode.GetAttributeCount(PR_TRUE);

  nsresult result=AddLeaf(aNode);
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
nsresult CNavDTD::HandleStyleToken(CToken* aToken){
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
nsresult CNavDTD::HandleProcessingInstructionToken(CToken* aToken){
  NS_PRECONDITION(0!=aToken,kNullToken);
  nsCParserNode aNode((CHTMLToken*)aToken,mLineNumber);

  #ifdef  RICKG_DEBUG
    WriteTokenToLog(aToken);
  #endif

  nsresult result=(mSink) ? mSink->AddProcessingInstruction(aNode) : NS_OK; 
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
 * @update  gess5/11/98
 * @param   node to consume skipped-content
 * @param   holds the number of skipped content elements encountered
 * @return  Error condition.
 */
nsresult CNavDTD::CollectSkippedContent(nsCParserNode& aNode,PRInt32& aCount) {

  CTokenRecycler* theRecycler=(CTokenRecycler*)mTokenizer->GetTokenRecycler();

  int aIndex=0;
  int aMax=mSkippedContent.GetSize();
  nsAutoString theTempStr;
  nsAutoString theStr;
  for(aIndex=0;aIndex<aMax;aIndex++){
    CHTMLToken* theNextToken=(CHTMLToken*)mSkippedContent.PopFront();
    theNextToken->GetSource(theTempStr);
    theStr+=theTempStr;
    theRecycler->RecycleToken(theNextToken);
  }
  aNode.SetSkippedContent(theStr);
  return NS_OK;
}


/**
 *  Finds a tag handler for the given tag type.
 *  
 *  @update  gess 4/2/98
 *  @param   aTagType type of tag to be handled
 *  @return  valid tag handler (if found) or null
 */
CITokenHandler* CNavDTD::GetTokenHandler(eHTMLTokenTypes aType) const {
  CITokenHandler* result=0;
  if((aType>0) && (aType<eToken_last)) {
    result=mTokenHandlers[aType];
  } 
  else {
  }
  return result;
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
  if(mHasOpenForm) {
    if(aParent==aChild)
      return gHTMLElements[aParent].CanContainSelf();
    if(FindTagInSet(aChild,gFormElementTags,sizeof(gFormElementTags)/sizeof(eHTMLTag_unknown))) {
      return PR_TRUE;
    }//if
  }//if
  return gHTMLElements[aParent].CanContain((eHTMLTags)aChild);
} 

/**
 * Give rest of world access to our tag enums, so that CanContain(), etc,
 * become useful.
 */
NS_IMETHODIMP CNavDTD::StringTagToIntTag(nsString &aTag, PRInt32* aIntTag) const
{
  *aIntTag = nsHTMLTags::LookupTag(aTag);
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
PRBool CNavDTD::CanPropagate(eHTMLTags aParentTag,eHTMLTags aChildTag) const {
  PRBool result=PR_FALSE;
  PRBool parentCanContain=CanContain(aParentTag,aChildTag);

  if(aParentTag==aChildTag) {
    return result;
  }

  int thePropLevel=0;
  if(nsHTMLElement::IsContainer(aChildTag)){
    if(!gHTMLElements[aChildTag].HasSpecialProperty(kNoPropagate)){
      if(nsHTMLElement::IsBlockParent(aParentTag) || (gHTMLElements[aParentTag].GetSpecialChildren())) {
        while(eHTMLTag_unknown!=aChildTag) {
          if(parentCanContain){
            result=PR_TRUE;
            break;
          }//if
          CTagList* theTagList=gHTMLElements[aChildTag].GetRootTags();
          aChildTag=theTagList->GetTagAt(0);
          parentCanContain=CanContain(aParentTag,aChildTag);
          ++thePropLevel;
        }//while
      }//if
    }//if
    if(thePropLevel>gHTMLElements[aParentTag].mPropagateRange)
      result=PR_FALSE;
  }//if
  else result=parentCanContain;
  return result;
}     

/**
 *  This method gets called to determine whether a given 
 *  tag can be omitted from opening. Most cannot.
 *  
 *  @update  gess 3/25/98
 *  @param   aTag -- tag to test for containership
 *  @return  PR_TRUE if given tag can contain other tags
 */
PRBool CNavDTD::CanOmit(eHTMLTags aParent,eHTMLTags aChild) const {

  eHTMLTags theAncestor=gHTMLElements[aChild].mExcludingAncestor;
  if(eHTMLTag_unknown!=theAncestor){
    if(HasOpenContainer(theAncestor))
      return PR_TRUE;
  }

  theAncestor=gHTMLElements[aChild].mRequiredAncestor;
  if(eHTMLTag_unknown!=theAncestor){
    if(!HasOpenContainer(theAncestor)) {
      if(!CanPropagate(aParent,aChild)) {
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
  if(CanContain(aParent,aChild) || (aChild==aParent)){
    return PR_FALSE;
  }

  if(nsHTMLElement::IsBlockEntity(aParent)) {
    if(nsHTMLElement::IsInlineEntity(aChild)) {  //feel free to drop inlines that a block doesn't contain.
      return PR_TRUE;
    }
  }

  if(gHTMLElements[aParent].HasSpecialProperty(kBadContentWatch)) {
    if(!gHTMLElements[aParent].CanContain(aChild)){
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
PRBool CNavDTD::ForwardPropagate(nsEntryStack& aStack,eHTMLTags aParentTag,eHTMLTags aChildTag)  {
  PRBool result=PR_FALSE;

  switch(aParentTag) {
    case eHTMLTag_table:
      {
        static eHTMLTags tableTags[]={eHTMLTag_tr,eHTMLTag_td};
        if(FindTagInSet(aChildTag,tableTags,sizeof(tableTags)/sizeof(eHTMLTag_unknown))) {
          return BackwardPropagate(aStack,aParentTag,aChildTag);
        }
      }
      //otherwise, intentionally fall through...

    case eHTMLTag_tr:
      {  
        PRBool theCanContainResult=CanContain(eHTMLTag_td,aChildTag);
        if(PR_TRUE==theCanContainResult) {
          aStack.Push(eHTMLTag_td);
          result=BackwardPropagate(aStack,aParentTag,eHTMLTag_td);
  //        result=PR_TRUE;
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
PRBool CNavDTD::BackwardPropagate(nsEntryStack& aStack,eHTMLTags aParentTag,eHTMLTags aChildTag) const {

  eHTMLTags theParentTag=aParentTag; //just init to get past first condition...
  do {
    CTagList* theRootTags=gHTMLElements[aChildTag].GetRootTags();
    if(theRootTags) {
      theParentTag=theRootTags->GetTagAt(0);
      if(CanContain(theParentTag,aChildTag)) {
        //we've found a complete sequence, so push the parent...
        aChildTag=theParentTag;
        aStack.Push(theParentTag);
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
      result=(kNotFound!=GetTopmostIndexOf(aContainer)); break;
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


/**
 *  Determine whether the given tag is open anywhere
 *  in our context stack.
 *  
 *  @update  gess 4/2/98
 *  @param   eHTMLTags tag to be searched for in stack
 *  @return  topmost index of tag on stack
 */
PRInt32 CNavDTD::GetTopmostIndexOf(eHTMLTags aTagSet[],PRInt32 aCount) const {
  int theIndex=0;
  for(theIndex=mBodyContext->GetCount()-1;theIndex>=0;theIndex--){
    if(FindTagInSet((*mBodyContext)[theIndex],aTagSet,aCount)) {
      return theIndex;
    }
  }
  return kNotFound;
}

/**
 *  Determine whether the given tag is open anywhere
 *  in our context stack.
 *  
 *  @update  gess 4/2/98
 *  @param   eHTMLTags tag to be searched for in stack
 *  @return  topmost index of tag on stack
 */
PRInt32 CNavDTD::GetTopmostIndexOf(eHTMLTags aTag) const {
  return mBodyContext->mStack.GetTopmostIndexOf(aTag);
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

  //later, change this so that transients only open in containers that get leaked in to.

#ifdef  ENABLE_RESIDUALSTYLE
  eHTMLTags theParentTag=mBodyContext->Last();
  if(!gHTMLElements[theParentTag].HasSpecialProperty(kNoStyleLeaksIn)) {
    if(!FindTagInSet(aChildTag,gWhitespaceTags,sizeof(gWhitespaceTags)/sizeof(aChildTag))){ 
        //the following code builds the set of style tags to be opened...
      PRUint32 theCount=mBodyContext->GetCount();
      PRUint32 theIndex=0;
      for(theIndex=0;theIndex<theCount;theIndex++){
        nsEntryStack* theStack=mBodyContext->GetStylesAt(theIndex);
        if(theStack){
          PRUint32 scount=theStack->GetCount();
          PRUint32 sindex=0;
          for(sindex=0;sindex<scount;sindex++){
            eHTMLTags     theTag=theStack->TagAt(sindex);

            if(theTag==eHTMLTag_font) //XXX HACK DEBUG!
              theTag=eHTMLTag_b;

            CStartToken   theToken(theTag);
            nsCParserNode theNode(&theToken,mLineNumber);
            theToken.SetTypeID(theTag); 
            result=OpenContainer(theNode,PR_FALSE);
          }
        }
      }
    }
  }
#endif
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

#ifdef  ENABLE_RESIDUALSTYLE

  int theTagPos=0;
    //now iterate style set, and close the containers...

  nsDeque* theStyleDeque=mBodyContext->GetStyles();
  for(theTagPos=mBodyContext->mOpenStyles;theTagPos>0;theTagPos--){
    eHTMLTags theTag=GetTopNode();
    CStartToken   token(theTag);
    nsCParserNode theNode(&token,mLineNumber);
    token.SetTypeID(theTag); 
    result=CloseContainer(theNode,theTag,PR_FALSE);
  }
#endif
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
nsresult CNavDTD::OpenHTML(const nsIParserNode& aNode){
  NS_PRECONDITION(mBodyContext->GetCount() >= 0, kInvalidTagStackPos);

  nsresult result=(mSink) ? mSink->OpenHTML(aNode) : NS_OK; 
  mBodyContext->Push((eHTMLTags)aNode.GetNodeType());
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
nsresult CNavDTD::CloseHTML(const nsIParserNode& aNode){
  NS_PRECONDITION(mBodyContext->GetCount() > 0, kInvalidTagStackPos);
  nsresult result=(mSink) ? mSink->CloseHTML(aNode) : NS_OK; 
  mBodyContext->Pop();
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
nsresult CNavDTD::OpenHead(const nsIParserNode& aNode){
  //mBodyContext->Push(eHTMLTag_head);
  nsresult result=NS_OK;
  if(!mHasOpenHead++) {
    result=(mSink) ? mSink->OpenHead(aNode) : NS_OK; 
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
nsresult CNavDTD::CloseHead(const nsIParserNode& aNode){
  nsresult result=NS_OK;
  if(mHasOpenHead) {
    if(0==--mHasOpenHead){
      result=(mSink) ? mSink->CloseHead(aNode) : NS_OK; 
    }
  }
  //mBodyContext->Pop();
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
nsresult CNavDTD::OpenBody(const nsIParserNode& aNode){
  NS_PRECONDITION(mBodyContext->GetCount() >= 0, kInvalidTagStackPos);

  nsresult result=NS_OK;

  mHadBody=PR_TRUE;

  PRInt32 theHTMLPos=GetTopmostIndexOf(eHTMLTag_html);
  if(kNotFound==theHTMLPos){ //someone forgot to open HTML. Let's do it for them.
    nsAutoString  theEmpty;
    CHTMLToken    token(theEmpty,eHTMLTag_html);
    nsCParserNode htmlNode(&token,mLineNumber);
    result=OpenHTML(htmlNode);  //open the html container...
    theHTMLPos=GetTopmostIndexOf(eHTMLTag_html);
  }

  PRBool theBodyIsOpen=HasOpenContainer(eHTMLTag_body);
  if(!theBodyIsOpen){
    //body is not already open, but head may be so close it
    result=CloseContainersTo(theHTMLPos+1,eHTMLTag_body,PR_TRUE);  //close current stack containers.
  }

  if(NS_OK==result) {
    result=(mSink) ? mSink->OpenBody(aNode) : NS_OK; 
    if(!theBodyIsOpen) {
      mBodyContext->Push((eHTMLTags)aNode.GetNodeType());
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
nsresult CNavDTD::CloseBody(const nsIParserNode& aNode){
  NS_PRECONDITION(mBodyContext->GetCount() >= 0, kInvalidTagStackPos);
  nsresult result=(mSink) ? mSink->CloseBody(aNode) : NS_OK; 
  mBodyContext->Pop();
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
nsresult CNavDTD::OpenForm(const nsIParserNode& aNode){
  if(mHasOpenForm)
    CloseForm(aNode);
  nsresult result=(mSink) ? mSink->OpenForm(aNode) : NS_OK; 
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
nsresult CNavDTD::CloseForm(const nsIParserNode& aNode){
  NS_PRECONDITION(mBodyContext->GetCount() > 0, kInvalidTagStackPos);
  nsresult result=NS_OK;
  if(mHasOpenForm) {
    mHasOpenForm=PR_FALSE;
    result=(mSink) ? mSink->CloseForm(aNode) : NS_OK; 
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
nsresult CNavDTD::OpenMap(const nsIParserNode& aNode){
  if(mHasOpenMap)
    CloseMap(aNode);
  nsresult result=(mSink) ? mSink->OpenMap(aNode) : NS_OK; 
  if(NS_OK==result) {
    mBodyContext->Push((eHTMLTags)aNode.GetNodeType());
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
nsresult CNavDTD::CloseMap(const nsIParserNode& aNode){
  NS_PRECONDITION(mBodyContext->GetCount() > 0, kInvalidTagStackPos);
  nsresult result=NS_OK;
  if(mHasOpenMap) {
    mHasOpenMap=PR_FALSE;
    result=(mSink) ? mSink->CloseMap(aNode) : NS_OK; 
    mBodyContext->Pop();
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
nsresult CNavDTD::OpenFrameset(const nsIParserNode& aNode){
  NS_PRECONDITION(mBodyContext->GetCount() >= 0, kInvalidTagStackPos);

  mHadFrameset=PR_TRUE;
  nsresult result=(mSink) ? mSink->OpenFrameset(aNode) : NS_OK; 
  mBodyContext->Push((eHTMLTags)aNode.GetNodeType());
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
nsresult CNavDTD::CloseFrameset(const nsIParserNode& aNode){
  NS_PRECONDITION(mBodyContext->GetCount() > 0, kInvalidTagStackPos);
  nsresult result=(mSink) ? mSink->CloseFrameset(aNode) : NS_OK; 
  mBodyContext->Pop();
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
CNavDTD::OpenContainer(const nsIParserNode& aNode,PRBool aClosedByStartTag){
  NS_PRECONDITION(mBodyContext->GetCount() >= 0, kInvalidTagStackPos);
  
  nsresult   result=NS_OK; 
  eHTMLTags nodeType=(eHTMLTags)aNode.GetNodeType();

  #define K_OPENOP 100
  CRCStruct theStruct(nodeType,K_OPENOP);
  mComputedCRC32=AccumulateCRC(mComputedCRC32,(char*)&theStruct,sizeof(theStruct));

  switch(nodeType) {

    case eHTMLTag_html:
      result=OpenHTML(aNode); break;

    case eHTMLTag_head:
     // result=OpenHead(aNode); //open the head...
      result=OpenHead(aNode); 
      break;

    case eHTMLTag_body:
        mHasOpenBody=PR_TRUE;
        if(mHasOpenHead)
          mHasOpenHead=1;
        CloseHead(aNode); //do this just in case someone left it open...
        result=OpenBody(aNode); 
      break;

    case eHTMLTag_style:
    case eHTMLTag_title:
      break;

    case eHTMLTag_textarea:
      {
        nsCParserNode& theCNode=*(nsCParserNode*)&aNode;
        result=AddLeaf(theCNode);
      }
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
      {
        nsCParserNode& theCNode=*(nsCParserNode*)&aNode;
        if(mHasOpenHead)
          mHasOpenHead=1;
        CloseHead(aNode); //do this just in case someone left it open...
        result=HandleScriptToken(theCNode);
      }
      break;

    default:
      result=(mSink) ? mSink->OpenContainer(aNode) : NS_OK; 
      mBodyContext->Push((eHTMLTags)aNode.GetNodeType());
      break;
  }

  /*
  if((NS_OK==result) && (PR_TRUE==aClosedByStartTag)){
    UpdateStyleStackForOpenTag(nodeType,nodeType);
  }
  */

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
CNavDTD::CloseContainer(const nsIParserNode& aNode,eHTMLTags aTag,PRBool aClosedByStartTag){
  nsresult   result=NS_OK;
  eHTMLTags nodeType=(eHTMLTags)aNode.GetNodeType();

  #define K_CLOSEOP 200
  CRCStruct theStruct(nodeType,K_CLOSEOP);
  mComputedCRC32=AccumulateCRC(mComputedCRC32,(char*)&theStruct,sizeof(theStruct));

  if(!aClosedByStartTag)
    UpdateStyleStackForCloseTag(nodeType,aTag);

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
      result=CloseBody(aNode); break;

    case eHTMLTag_map:
      result=CloseMap(aNode);
      break;

    case eHTMLTag_form:
      result=CloseForm(aNode); break;

    case eHTMLTag_frameset:
      result=CloseFrameset(aNode); break;

    case eHTMLTag_title:
    default:
      result=(mSink) ? mSink->CloseContainer(aNode) : NS_OK; 
      mBodyContext->Pop();
      break;
  }

  if(aClosedByStartTag)
    UpdateStyleStackForOpenTag(nodeType,aTag);

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
nsresult CNavDTD::CloseContainersTo(PRInt32 anIndex,eHTMLTags aTag, PRBool aClosedByStartTag){
  NS_PRECONDITION(mBodyContext->GetCount() > 0, kInvalidTagStackPos);
  nsresult result=NS_OK;

  nsAutoString  theEmpty;
  CEndToken     theToken(theEmpty);
  nsCParserNode theNode(&theToken,mLineNumber);

  if((anIndex<mBodyContext->GetCount()) && (anIndex>=0)) {
    while(mBodyContext->GetCount()>anIndex) {
      eHTMLTags theTag=mBodyContext->Last();
      theToken.SetTypeID(theTag);
      result=CloseContainer(theNode,aTag,aClosedByStartTag);
    }
  }
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
nsresult CNavDTD::CloseContainersTo(eHTMLTags aTag,PRBool aClosedByStartTag){
  NS_PRECONDITION(mBodyContext->GetCount() > 0, kInvalidTagStackPos);

  PRInt32 pos=GetTopmostIndexOf(aTag);

  if(kNotFound!=pos) {
    //the tag is indeed open, so close it.
    return CloseContainersTo(pos,aTag,aClosedByStartTag);
  }

  eHTMLTags theTopTag=mBodyContext->Last();

  PRBool theTagIsSynonymous=((nsHTMLElement::IsStyleTag(aTag)) && (nsHTMLElement::IsStyleTag(theTopTag)));
  if(!theTagIsSynonymous){
    theTagIsSynonymous=((nsHTMLElement::IsHeadingTag(aTag)) && (nsHTMLElement::IsHeadingTag(theTopTag)));  
  }

  if(theTagIsSynonymous) {
    //if you're here, it's because we're trying to close one tag,
    //but a different (synonymous) one is actually open. Because this is NAV4x
    //compatibililty mode, we must close the one that's really open.
    aTag=theTopTag;    
    pos=GetTopmostIndexOf(aTag);
    if(kNotFound!=pos) {
      //the tag is indeed open, so close it.
      return CloseContainersTo(pos,aTag,aClosedByStartTag);
    }
  }
  
  nsresult result=NS_OK;
  CTagList* theRootTags=gHTMLElements[aTag].GetRootTags();
  eHTMLTags theParentTag=theRootTags->GetTagAt(0);
  pos=GetTopmostIndexOf(theParentTag);
  if(kNotFound!=pos) {
    //the parent container is open, so close it instead
    result=CloseContainersTo(pos+1,aTag,aClosedByStartTag);
  }
  return result;
}

/**
 * This method causes the topmost container on the stack
 * to be closed. The closure is ALWAYS due to a new start
 * tag be opened which forces the topmost tag closed.
 *
 * @update  gess 4/26/99
 * @return  TRUE if ok, FALSE if error
 */
nsresult CNavDTD::CloseTopmostContainer(){
  NS_PRECONDITION(mBodyContext->GetCount() > 0, kInvalidTagStackPos);

  nsAutoString  theEmpty;
  CEndToken     theToken(theEmpty);
  eHTMLTags     theTag=mBodyContext->Last();
  
  theToken.SetTypeID(theTag);
  nsCParserNode theNode(&theToken,mLineNumber);
  return CloseContainer(theNode,theTag,PR_TRUE);
}

/**
 * This method does two things: 1st, help construct
 * our own internal model of the content-stack; and
 * 2nd, pass this message on to the sink.
 * @update  gess4/6/98
 * @param   aNode -- next node to be added to model
 * @return  error code; 0 means OK
 */
nsresult CNavDTD::AddLeaf(const nsIParserNode& aNode){
  nsresult result=NS_OK;
  
  if(mSink){
    eHTMLTags theTag=(eHTMLTags)aNode.GetNodeType();
    OpenTransientStyles(theTag); 
    result=mSink->AddLeaf(aNode);
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
nsresult CNavDTD::AddHeadLeaf(nsIParserNode& aNode){
  nsresult result=NS_OK;

  static eHTMLTags gNoXTags[]={eHTMLTag_noframes,eHTMLTag_nolayer,eHTMLTag_noscript};
  
  //this code has been added to prevent <meta> tags from being processed inside
  //the document if the <meta> tag itself was found in a <noframe>, <nolayer>, or <noscript> tag.
  eHTMLTags theTag=(eHTMLTags)aNode.GetNodeType();
  if(eHTMLTag_meta==theTag)
    if(HasOpenContainer(gNoXTags,sizeof(gNoXTags)/sizeof(eHTMLTag_unknown))) {
      return result;
    }

  if(mSink) {
    result=OpenHead(aNode);
    if(NS_OK==result) {
      if(eHTMLTag_title==theTag) {
        mSink->SetTitle(aNode.GetSkippedContent());
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
nsresult CNavDTD::CreateContextStackFor(eHTMLTags aChildTag){
  
  static nsEntryStack kPropagationStack;
  kPropagationStack.Empty();
  
  nsresult  result=(nsresult)kContextMismatch;
  eHTMLTags theTop=mBodyContext->Last();
  PRBool    bResult=ForwardPropagate(kPropagationStack,theTop,aChildTag);
  
  if(PR_FALSE==bResult){

    if(eHTMLTag_unknown!=theTop) {
      if(theTop!=aChildTag) //dont even bother if we're already inside a similar element...
        bResult=BackwardPropagate(kPropagationStack,theTop,aChildTag);

     /*****************************************************************************
        OH NOOOO!...

        We found a pretty fundamental flaw in the backward propagation code.
        The previous version propagated from a child to a target parent, and
        then again from the target parent to the root. 
        Only thing is, that won't work in cases where a container exists that's 
        not in the usual hiearchy:
          
          <html><body>
            <div>
              <table>
                <!--missing TR>
                  <td>cell text</td>
                </tr>
              </table> ...etc...

        In this case, we'd propagate fine from the <TD> to the <TABLE>, and
        then from the table to the <html>. Unfortunately, the <DIV> won't show
        up in the propagated form, and then we get out of sync with the actual
        context stack when it comes to autogenerate containers.
      ******************************************************************************/
      
    } //if
    else bResult=BackwardPropagate(kPropagationStack,eHTMLTag_html,aChildTag);
  } //elseif

  if((0==mBodyContext->GetCount()) || (mBodyContext->Last()==kPropagationStack.Pop()))
    result=NS_OK;

  //now, build up the stack according to the tags 
  //you have that aren't in the stack...
  nsAutoString  theEmpty;
  CStartToken theToken(theEmpty);
  PRInt32 count = kPropagationStack.GetCount();
  if(PR_TRUE==bResult){
    while(count>0) {
      eHTMLTags theTag=kPropagationStack.Pop();
      theToken.SetTypeID(theTag);  //open the container...
      HandleStartToken(&theToken);
      count--;
    }
    result=NS_OK;
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
nsresult CNavDTD::ReduceContextStackFor(eHTMLTags aChildTag){
  nsresult  result=NS_OK;
  eHTMLTags theTopTag=mBodyContext->Last();

  while( (theTopTag!=kNotFound) && 
         (PR_FALSE==CanContain(theTopTag,aChildTag)) &&
         (PR_FALSE==CanPropagate(theTopTag,aChildTag))){
    CloseTopmostContainer();
    theTopTag=mBodyContext->Last();
  }
  return result;
}


/**
 * This method causes all explicit style-tag containers that
 * are opened to be reflected on our internal style-stack.
 *
 * @update  gess6/4/98
 * @param   aTag is the id of the html container being opened
 * @return  0 if all is well.
 */
nsresult CNavDTD::UpdateStyleStackForOpenTag(eHTMLTags aTag,eHTMLTags anActualTag){
  nsresult   result=0;

#ifdef  ENABLE_RESIDUALSTYLE
  if(anActualTag!=eHTMLTag_font){
    if(nsHTMLElement::IsStyleTag(aTag)) {
      mBodyContext->PushStyle(aTag);
    }
  }
#endif
  return result;
} //update..


/**
 * This method gets called when an explicit style close-tag is encountered.
 * It results in the style tag id being popped from our internal style stack.
 *
 * @update  gess6/4/98
 * @param 
 * @return  0 if all went well (which it always does)
 */
nsresult
CNavDTD::UpdateStyleStackForCloseTag(eHTMLTags aTag,eHTMLTags anActualTag){
  nsresult result=0;

#ifdef  ENABLE_RESIDUALSTYLE
  eHTMLTags theTag=mBodyContext->PopStyle();
#endif
  return result;
}

/**
 * 
 * @update	gess8/4/98
 * @param 
 * @return
 */
nsITokenRecycler* CNavDTD::GetTokenRecycler(void){
  nsITokenizer* theTokenizer=GetTokenizer();
  return theTokenizer->GetTokenRecycler();
}

/**
 * Retrieve the preferred tokenizer for use by this DTD.
 * @update	gess12/28/98
 * @param   none
 * @return  ptr to tokenizer
 */
nsITokenizer* CNavDTD::GetTokenizer(void) {
  if(!mTokenizer)
    mTokenizer=new nsHTMLTokenizer();
  return mTokenizer;
}

/**
 * 
 * @update  gess5/18/98
 * @param 
 * @return
 */
nsresult CNavDTD::WillResumeParse(void){
  nsresult result=(mSink) ? mSink->WillResume() : NS_OK; 
  return result;
}

/**
 * This method gets called when the parsing process is interrupted
 * due to lack of data (waiting for netlib).
 * @update  gess5/18/98
 * @return  error code
 */
nsresult CNavDTD::WillInterruptParse(void){
  nsresult result=(mSink) ? mSink->WillInterrupt() : NS_OK; 
  return result;
}


/**
 * 
 * @update	gpk03/14/99
 * @param 
 * @return
 */
nsresult CNavDTD::DoFragment(PRBool aFlag) 
{
  return NS_OK;
}

