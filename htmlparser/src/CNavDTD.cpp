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
#include "nsViewSourceHTML.h"
#include "nsHTMLTokenizer.h"
#include "nsTime.h"

#ifdef XP_PC
#include <direct.h> //this is here for debug reasons...
#include <iostream.h>
#endif
#include "prmem.h"

#define RICKG_DEBUG 0
#ifdef  RICKG_DEBUG
#include  <fstream.h>  
#endif


static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);                 
static NS_DEFINE_IID(kIDTDIID,      NS_IDTD_IID);
static NS_DEFINE_IID(kClassIID,     NS_INAVHTML_DTD_IID); 
 
static const  char* kNullToken = "Error: Null token given";
static const  char* kInvalidTagStackPos = "Error: invalid tag stack position";
static char*        kVerificationDir = "c:/temp";
static char         gShowCRC=0;
 

static eHTMLTags gFormElementTags[]= {  
    eHTMLTag_button,  eHTMLTag_fieldset,  eHTMLTag_input,
    eHTMLTag_isindex, eHTMLTag_label,     eHTMLTag_legend,
    eHTMLTag_option,  eHTMLTag_optgroup,  eHTMLTag_select,
    eHTMLTag_textarea};

static eHTMLTags gTableChildTags[]={ 
  eHTMLTag_caption, eHTMLTag_col,     eHTMLTag_colgroup,  
  eHTMLTag_tbody,   eHTMLTag_tfoot,   eHTMLTag_tr,  
  eHTMLTag_th,      eHTMLTag_thead,   eHTMLTag_td};
  
static eHTMLTags gWhitespaceTags[]={
  eHTMLTag_newline, eHTMLTag_whitespace};

static eHTMLTags gHeadChildTags[]={ 
  eHTMLTag_caption, eHTMLTag_col, eHTMLTag_colgroup,  eHTMLTag_tbody,   
  eHTMLTag_tfoot,   eHTMLTag_tr,  eHTMLTag_thead,     eHTMLTag_td};

static eHTMLTags gNonPropagatedTags[]={
  eHTMLTag_head,  eHTMLTag_html,  eHTMLTag_body};


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
      case eToken_skippedcontent:
        result=theDTD->HandleSkippedContentToken(aToken); break;
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
CNavDTD::CNavDTD() : nsIDTD(), mMisplacedContent(0) {
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
//  DebugDumpContainmentRules2(*this,"c:/temp/DTDRules.new","New CNavDTD Containment Rules");
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

  if(!mDTDDebug){;
    nsresult rval = NS_NewDTDDebug(&mDTDDebug);
    if (NS_OK != rval) {
      fputs("Cannot create parser debugger.\n", stdout);
      result=-PR_FALSE;
    }
    else mDTDDebug->SetVerificationDirectory(kVerificationDir);
  }
  if(mDTDDebug) {
    // mDTDDebug->Verify(this,aParser,mBodyContext->GetCount(),mBodyContext->mTags,aURLRef);
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
  mHadBodyOrFrameset=PR_FALSE;
  mLineNumber=1;
  mHasOpenScript=PR_FALSE;
  mSink=(nsIHTMLContentSink*)aSink;
  if((aNotifySink) && (mSink)) {

#ifdef RGESS_DEBUG
    gStartTime = PR_Now();
    printf("Begin parsing...\n");
#endif

    result = mSink->WillBuildModel();

    mComputedCRC32=0;
    mExpectedCRC32=0;
  }
  return result;
}

CTokenRecycler* gRecycler=0;

/**
  * This is called when it's time to read as many tokens from the tokenizer
  * as you can. Not all tokens may make sense, so you may not be able to
  * read them all (until more come in later).
  *
  * @update	gess5/18/98
  * @param	aParser is the parser object that's driving this process
  * @return	error code (almost always 0)
  */
nsresult CNavDTD::BuildModel(nsIParser* aParser,nsITokenizer* aTokenizer,nsITokenObserver* anObserver,nsIContentSink* aSink) {
  nsresult result=NS_OK;

  if(aTokenizer) {
    nsITokenizer*  oldTokenizer=mTokenizer;
    mTokenizer=aTokenizer;
    mParser=(nsParser*)aParser;
    mSink=(nsIHTMLContentSink*)aSink;
    gRecycler=(CTokenRecycler*)mTokenizer->GetTokenRecycler();
    while(NS_OK==result){
      CToken* theToken=mTokenizer->PopToken();
      if(theToken) { 
        result=HandleToken(theToken,aParser);
      }
      else break;
    }//while
    mTokenizer=oldTokenizer;
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
  nsresult result= NS_OK;

  /*
  if((NS_OK==anErrorCode) && (!mHadBodyOrFrameset)) {
    CStartToken theToken(eHTMLTag_body);  //open the body container...
    result=HandleStartToken(&theToken);

    if(NS_SUCCEEDED(result)) {
      result=BuildModel(aParser,mTokenizer,0,aSink);
    }
  }
*/

  if(aParser){
    mSink=(nsIHTMLContentSink*)aSink;
    if(aNotifySink && mSink){
      if((NS_OK==anErrorCode) && (mBodyContext->GetCount()>0)) {
        result = CloseContainersTo(0,eHTMLTag_unknown,PR_FALSE);
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
            result = mSink->DidBuildModel(2);
          } 
          else {
            printf("Computed CRC: %u.\n",mComputedCRC32);
            result = mSink->DidBuildModel(3);
          }
        }
        else result = mSink->DidBuildModel(0);
      }
      else result=mSink->DidBuildModel(0);

      if(mDTDDebug) {
        mDTDDebug->DumpVectorRecord();
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
  nsresult result=NS_OK;

  if(aToken) {
    CHTMLToken*     theToken= (CHTMLToken*)(aToken);
    eHTMLTokenTypes theType=eHTMLTokenTypes(theToken->GetTokenType());
    eHTMLTags       theTag=(eHTMLTags)theToken->GetTypeID();

    static eHTMLTags passThru[]= {eHTMLTag_html,eHTMLTag_comment,eHTMLTag_newline,eHTMLTag_whitespace,eHTMLTag_script};
    if(!FindTagInSet(theTag,passThru,sizeof(passThru)/sizeof(eHTMLTag_unknown))){
      if(!gHTMLElements[eHTMLTag_html].SectionContains(theTag,PR_FALSE)) {
        if(!mHadBodyOrFrameset){
          if(mHasOpenHead) {
            //just fall through and handle current token
            if(!gHTMLElements[eHTMLTag_head].IsChildOfHead(theTag)){
              mMisplacedContent.Push(aToken);
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
    
    if(theToken){
      CITokenHandler* theHandler=GetTokenHandler(theType);
      if(theHandler) {
        mParser=(nsParser*)aParser;
        result=(*theHandler)(theToken,this);
        if(NS_SUCCEEDED(result) || (NS_ERROR_HTMLPARSER_BLOCK==result)) {
          gRecycler->RecycleToken(theToken);
        }
        else if(NS_ERROR_HTMLPARSER_MISPLACED!=result)
          mTokenizer->PushTokenFront(theToken);
        else result=NS_OK;
        if (mDTDDebug) {
           //mDTDDebug->Verify(this, mParser, mBodyContext->GetCount(), mBodyContext->mTags, mFilename);
        }
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

  CToken* theNextToken=mTokenizer->PeekToken();
  if(theNextToken){
    switch(aChildTag){
      case eHTMLTag_body:
      case eHTMLTag_frameset:
        mHadBodyOrFrameset=PR_TRUE;
        break;


      case eHTMLTag_pre:
      case eHTMLTag_listing:
        {
          if(theNextToken)  {
            eHTMLTokenTypes theType=eHTMLTokenTypes(theNextToken->GetTokenType());
            if(eToken_newline==theType){

              switch(aChildTag){
                case eHTMLTag_pre:
                case eHTMLTag_listing:
                    //we skip the first newline token inside PRE and LISTING
                  mTokenizer->PopToken();
                  break;
                default:
                  break;
              }//switch

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
            CViewSourceHTML::WriteText(theText,*mSink,PR_TRUE);
          }
        }
        break;
      default:
        break;
    }//switch 
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
static
PRInt32 GetTopmostIndexOf(eHTMLTags aTag,nsTagStack& aTagStack) {
  int i=0;
  int count = aTagStack.mTags->GetSize();
  for(i=(count-1);i>=0;i--){
    if(aTagStack[i]==aTag)
      return i;
  }
  return kNotFound;
}


/** 
 *  This method is called to determine whether or not a START tag
 *  can be autoclosed. This means that based on the current
 *  context, the stack should be closed to the nearest matching
 *  tag.  
 *    
 *  @param   aTag -- tag enum of child to be tested
 *  @return  PR_TRUE if autoclosure should occur
 */  
static
eHTMLTags FindAutoCloseTargetForStartTag(eHTMLTags aCurrentTag,nsTagStack& aTagStack) {
  int theTopIndex = aTagStack.mTags->GetSize();
  eHTMLTags thePrevTag=aTagStack.Last();
 
  if(nsHTMLElement::IsContainer(aCurrentTag)){
    if(thePrevTag==aCurrentTag) {
      return (gHTMLElements[aCurrentTag].CanContainSelf()) ? eHTMLTag_unknown: aCurrentTag;
    }
    if(nsHTMLElement::IsBlockCloser(aCurrentTag)) {

      PRInt32 theRootIndex=kNotFound;
      CTagList* theRootTags=gHTMLElements[aCurrentTag].GetRootTags();
      if(theRootTags) {
        theRootIndex=theRootTags->GetTopmostIndexOf(aTagStack);
        CTagList* theStartTags=gHTMLElements[aCurrentTag].GetAutoCloseStartTags();
        PRInt32 thePeerIndex=kNotFound;
        if(theStartTags){
          thePeerIndex=theStartTags->GetBottommostIndexOf(aTagStack,theRootIndex+1);
        }
        else {
          //this extra check is need to handle case like this: <DIV><P><DIV>
          //the new div can close the P,but doesn't close the top DIV.
          thePeerIndex=GetTopmostIndexOf(aCurrentTag,aTagStack);
          if(gHTMLElements[aCurrentTag].CanContainSelf()) {
            thePeerIndex++;
          }
        }
        if(theRootIndex<thePeerIndex) {
          return aTagStack[thePeerIndex]; //return the tag that was used in peer test.
        }
      }
 
      CTagList* theCloseTags=gHTMLElements[aCurrentTag].GetAutoCloseStartTags();
      if(theCloseTags){
        PRInt32 thePeerIndex=theCloseTags->GetTopmostIndexOf(aTagStack);
        if(kNotFound!=thePeerIndex){
          if(thePeerIndex==theTopIndex-1) {
            //the guy you can autoclose is on the top of the stack...
            return thePrevTag;
          } //if
        } //if
      }//if
      else if(kNotFound<theRootIndex) {
        //This block handles our fallback cases like: <html><body><center><p>  <- <table> 
        while((theRootIndex<--theTopIndex) && (!gHTMLElements[aTagStack[theTopIndex]].CanContain(aCurrentTag))) {
        }
        return aTagStack[theTopIndex+1];
        //return aTagStack.mTags[theRootIndex+1];
      }

    } //if
  } //if
  return eHTMLTag_unknown;
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
PRInt32 GetIndexOfChildOrSynonym(nsTagStack& aTagStack,eHTMLTags aChildTag) {
  PRInt32 theChildIndex=aTagStack.GetTopmostIndexOf(aChildTag);
  if(kNotFound==theChildIndex) {
    CTagList* theSynTags=gHTMLElements[aChildTag].GetSynonymousTags(); //get the list of tags that THIS tag can close
    if(theSynTags) {
      theChildIndex=theSynTags->GetTopmostIndexOf(aTagStack);
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
PRBool CanBeContained(eHTMLTags aParentTag,eHTMLTags aChildTag,nsTagStack& aTagStack) {
  PRBool result=PR_TRUE;

  /* #    Interesting test cases:       Result:
   * 1.   <UL><LI>..<B>..<LI>           inner <LI> closes outer <LI>
   * 2.   <CENTER><DL><DT><A><CENTER>   allow nested <CENTER>
   * 3.   <TABLE><TR><TD><TABLE>...     allow nested <TABLE>
   * 4.   <FRAMESET> ... <FRAMESET>
   */

  //Note: This method is going away. First we need to get the elementtable to do closures right, and
  //      therefore we must get residual style handling to work.

  if(nsHTMLElement::IsStyleTag(aParentTag))
    if(nsHTMLElement::IsStyleTag(aChildTag))
      return PR_TRUE;

  CTagList* theRootTags=gHTMLElements[aChildTag].GetRootTags();
  if(theRootTags) {
    PRInt32 theRootIndex=theRootTags->GetTopmostIndexOf(aTagStack);          
    PRInt32 theChildIndex=GetIndexOfChildOrSynonym(aTagStack,aChildTag);
    PRInt32 theBaseIndex=(theRootIndex<theChildIndex) ? theRootIndex : theChildIndex;

    if((theRootIndex==theChildIndex) && (gHTMLElements[aChildTag].CanContainSelf()))
      result=PR_TRUE;
    else result=PRBool(theRootIndex>theChildIndex);
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

    //Sick as it sounds, I have to make sure the body has been
    //opened before other tags can be added to the content sink...

  PRBool rickgSkip=PR_FALSE;
  if(!rickgSkip) {
/*
    TRYING TO MAKE THIS CODE GO AWAY!...

    static eHTMLTags gBodyBlockers[]={eHTMLTag_body,eHTMLTag_frameset,eHTMLTag_map};
    PRInt32 theBodyBlocker=GetTopmostIndexOf(gBodyBlockers,sizeof(gBodyBlockers)/sizeof(eHTMLTag_unknown));
    if((kNotFound==theBodyBlocker) && (!mHasOpenHead)){
      if(CanPropagate(eHTMLTag_body,aChildTag)) {
        mHasOpenBody=PR_TRUE;
        result=CreateContextStackFor(aChildTag);
        //CStartToken theToken(eHTMLTag_body);  //open the body container...
        //result=HandleStartToken(&theToken);
      } 
    }//if                      
*/
    /***********************************************************************
      Subtlety alert: 

      The REAL story on when tags are opened is somewhat confusing, but it's
      important to understand. Since this is where we deal with it, it's time
      for a quick disseration. Here goes:

      Given a stack of open tags, a new (child) tag comes along and we need 
      to see if it can be opened in place. There are at least 2 reasons why
      it cannot be opened: 1) the parent says so; 2) the child says so.

      Parents refuse to take children they KNOW they can't contain. Consider
      that the <HTML> tag is only *supposed* to contain certain tags -- 
      no one would expect it to accept a rogue <LI> tag (for example). 

      Here's an interested case we should not break:

      <HTML>
        <BODY>
          <DL> 
            <DT> 
              <DD>
                <LI>
                  <DT>
 
      The DT is not a child of the LI, so the LI closes. Then the DT also
      closes the DD, and it's parent DT. At last, it reopens itself below DL.

     ***********************************************************************/

    eHTMLTags theParentTag=mBodyContext->Last();
    PRBool theCanContainResult=CanContain(theParentTag,aChildTag);
    PRBool theChildAgrees=(theCanContainResult) ? CanBeContained(theParentTag,aChildTag,mBodyContext->mTags) : PR_FALSE;

    if(!(theCanContainResult && theChildAgrees)) {
      eHTMLTags theTarget=FindAutoCloseTargetForStartTag(aChildTag,mBodyContext->mTags);
      if(eHTMLTag_unknown!=theTarget){
        result=CloseContainersTo(theTarget,PR_TRUE);
        theParentTag=mBodyContext->Last();
        theCanContainResult=CanContain(theParentTag,aChildTag);
      }
    }

    if(PR_FALSE==theCanContainResult){
      if(CanPropagate(theParentTag,aChildTag))
        result=CreateContextStackFor(aChildTag);
      else result=kCantPropagate;
      if(NS_OK!=result) { 
        //if you're here, then the new topmost container can't contain aToken.
        //You must determine what container hierarchy you need to hold aToken,
        //and create that on the parsestack.
        result=ReduceContextStackFor(aChildTag);

        PRBool theCanContainResult=CanContain(mBodyContext->Last(),aChildTag);

        if(PR_FALSE==theCanContainResult) {
          //we unwound too far; now we have to recreate a valid context stack.
          result=CreateContextStackFor(aChildTag);
        }
      }
    }

  }//if(!rickGSkip)...

  if(nsHTMLElement::IsContainer(aChildTag)){
      //first, let's see if it's a style element...
    if(!nsHTMLElement::IsStyleTag(aChildTag)) {
        //it wasn't a style container, so open the element container...
      if(mBodyContext->mOpenStyles) {
        CloseTransientStyles(aChildTag);
      }
    } 
    result=OpenContainer(aNode,PR_TRUE);
  }
  else {  //we're writing a leaf...
    OpenTransientStyles(aChildTag);
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
  PRInt32  theAttrCount;

    //first let's see if there's some skipped content to deal with...
  if(gHTMLElements[aTag].mSkipTarget) {
    result=CollectSkippedContent(aNode,theAttrCount);
  }

  //**********************************************************
  //XXX Hack until I get the node observer API in place...

  if(eHTMLTag_meta==aTag) {
    PRInt32 theCount=aNode.GetAttributeCount();
    if(1<theCount){
 
        //<META HTTP-EQUIV="Content-Type" CONTENT="text/html; charset=iso-8859-1">
      const nsString& theKey=aNode.GetKeyAt(0);
      if(theKey.EqualsIgnoreCase("HTTP-EQUIV")) {
        const nsString& theKey2=aNode.GetKeyAt(1);
        if(theKey2.EqualsIgnoreCase("CONTENT")) {
            nsScanner* theScanner=mParser->GetScanner();
            if(theScanner) {
              const nsString& theValue=aNode.GetValueAt(1);
              PRInt32 charsetValueStart = theValue.RFind("charset=", PR_TRUE ) ;
              if(kNotFound != charsetValueStart) {	
                 charsetValueStart += 8; // 8 = "charset=".length 
                 PRInt32 charsetValueEnd = theValue.FindCharInSet("\'\";", charsetValueStart  );
                 if(kNotFound == charsetValueEnd ) 
                    charsetValueEnd = theValue.Length();
                 nsAutoString theCharset;
                 theValue.Mid(theCharset, charsetValueStart, charsetValueEnd - charsetValueStart);
                 theScanner->SetDocumentCharset(theCharset, kCharsetFromMetaTag);
				 // XXX this should be delete after META charset really work
				 nsParser::gHackMetaCharset = theCharset;
              } //if
          } //if
        }
      } //if

      else if(theKey.EqualsIgnoreCase("NAME")) {
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

  //XXX Hack until I get the node observer API in place...
  //**********************************************************

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

  // Some tags need no to be opened regardless of what the parent says.
  if(gHTMLElements[aChildTag].HasSpecialProperty(kLegalOpen)) {
    return !NS_OK;
  }
  //The trick here is to see if the parent can contain the child, but prefers not to.
  //Only if the parent CANNOT contain the child should we look to see if it's potentially a child
  //of another section. If it is, the cache it for later.
  //  1. Get the root node for the child. See if the ultimate node is the BODY, FRAMESET, HEAD or HTML
  PRInt32   theTagCount = mBodyContext->GetCount();

  if(gHTMLElements[aParent].HasSpecialProperty(kBadContentWatch)) {
    eHTMLTags theTag;
    PRInt32   theBCIndex;
    PRBool    isNotWhiteSpace = PR_FALSE;
    PRInt32   attrCount   = aToken->GetAttributeCount();
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
    if(mSaveBadTokens &&(isNotWhiteSpace || mBodyContext->mTags.TokenCountAt(theBCIndex) > 0)) {
      mBodyContext->mTags.SaveToken(aToken,theBCIndex);
      if(attrCount > 0) {
        nsCParserNode* theAttrNode = (nsCParserNode*)&aNode;
        while(attrCount > 0){ 
           mBodyContext->mTags.SaveToken(theAttrNode->PopAttributeToken(),theBCIndex);
           attrCount--;
        }
      }
      if(!IsContainer(aChildTag) && isNotWhiteSpace) {
        mSaveBadTokens = PR_FALSE;
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
  eHTMLTags     theChildTag=(eHTMLTags)aToken->GetTypeID();
  nsCParserNode attrNode((CHTMLToken*)aToken,mLineNumber,GetTokenRecycler()); 
  PRInt16       attrCount=aToken->GetAttributeCount();
  nsresult      result=(0==attrCount) ? NS_OK : CollectAttributes(attrNode,attrCount);
  eHTMLTags     theParent=mBodyContext->Last();

  if(NS_OK==result) {
    if(NS_OK==WillHandleStartTag(aToken,theChildTag,attrNode)) {

      if(nsHTMLElement::IsSectionTag(theChildTag)){
        switch(theChildTag){
          case eHTMLTag_head:
          case eHTMLTag_body:
            if(mHadBodyOrFrameset) {
              result=HandleOmittedTag(aToken,theChildTag,theParent,attrNode);
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
            result=mSink->AddLeaf(attrNode);
          break;

        case eHTMLTag_comment:
        case eHTMLTag_userdefined:
          break; //drop them on the floor for now...

        case eHTMLTag_script:
          theHeadIsParent=(!mHasOpenBody); //intentionally fall through...
          mHasOpenScript=PR_TRUE;

        default:
          {
            if(theHeadIsParent)
              result=AddHeadLeaf(attrNode);
            else if(CanOmit(theParent,theChildTag))
              result=HandleOmittedTag(aToken,theChildTag,theParent,attrNode);
            else result=HandleDefaultStartToken(aToken,theChildTag,attrNode); 
          }
          break;
      } //switch
      //now do any post processing necessary on the tag...
      if(NS_OK==result)
        DidHandleStartTag(attrNode,theChildTag);
    }
  } //if

  if(eHTMLTag_newline==theChildTag)
    mLineNumber++;

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
PRBool HasCloseablePeerAboveRoot(CTagList& aRootTagList,nsTagStack& aTagStack,eHTMLTags aTag,PRBool anEndTag) {
  PRInt32 theRootIndex=aRootTagList.GetTopmostIndexOf(aTagStack);          
  CTagList* theCloseTags=(anEndTag) ? gHTMLElements[aTag].GetAutoCloseEndTags() : gHTMLElements[aTag].GetAutoCloseStartTags();
  PRInt32 theChildIndex=-1;
  PRBool  result=PR_FALSE;
  if(theCloseTags) {
    theChildIndex=theCloseTags->GetTopmostIndexOf(aTagStack);
  }
  else {
    theChildIndex=aTagStack.GetTopmostIndexOf(aTag);
  }
  return PRBool(theRootIndex<theChildIndex);
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
eHTMLTags FindAutoCloseTargetForEndTag(eHTMLTags aCurrentTag,nsTagStack& aTagStack) {
  int theTopIndex=aTagStack.mTags->GetSize();
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
      } //if
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

    case eHTMLTag_map:
    case eHTMLTag_form:
    case eHTMLTag_head:
      result=CloseContainer(theNode,theChildTag,PR_FALSE);
      break;

    default:
      {
        //now check to see if this token should be omitted, or 
        //if it's gated from closing by the presence of another tag.
        if(PR_TRUE==CanOmitEndTag(mBodyContext->Last(),theChildTag)) {
          UpdateStyleStackForCloseTag(theChildTag,theChildTag);
        }
        else {
          eHTMLTags theTarget=FindAutoCloseTargetForEndTag(theChildTag,mBodyContext->mTags);
          if(gHTMLElements[theTarget].HasSpecialProperty(kBadContentWatch)){
             result = HandleSavedTokensAbove(theTarget);
          }
          if(eHTMLTag_unknown!=theTarget) {
            result=CloseContainersTo(theTarget,PR_TRUE);
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

    CToken*       theToken;
    eHTMLTags     theTag;
    nsDTDContext  temp;
    PRInt32       attrCount;
    nsresult      result      = NS_OK;
    PRInt32       theTopIndex = GetTopmostIndexOf(aTag);
    PRInt32       theTagCount = mBodyContext->GetCount();

 
    if(theTopIndex == kNotFound)
      return result;

    PRInt32  theBadContentIndex = theTopIndex - 1;
    PRInt32  theBadTokenCount   = mBodyContext->mTags.TokenCountAt(theBadContentIndex);

    if(theBadTokenCount > 0) {
      // Pause the main context and switch to the new context.
      mSink->BeginContext(theBadContentIndex);
 
      // The body context should contain contents only upto the marked position.  
      for(PRInt32 i=0; i<(theTagCount - theTopIndex); i++)
        temp.Push(mBodyContext->Pop());
     
      // Now flush out all the bad contents.
      while(theBadTokenCount > 0){
        theToken       = mBodyContext->mTags.RestoreTokenFrom(theBadContentIndex);
        if(theToken) {
          theTag       = (eHTMLTags)theToken->GetTypeID();
          if(theTag != eHTMLTag_unknown) {
            attrCount    = theToken->GetAttributeCount();
            // Put back attributes, which once got popped out, into the tokenizer
            if(attrCount > 0) {
              PRInt32 theAttrCount  = 0;
              for(PRInt32 i=0;i<attrCount; i++){
                CToken* theAttrToken = mBodyContext->mTags.RestoreTokenFrom(theBadContentIndex);
                if(theAttrToken) {
                  mTokenizer->PushTokenFront(theAttrToken);
                  theAttrCount++;
                }
                theBadTokenCount--;
              }
              // XXX Hack. Wonder why the attribute count changes, sometimes!!!!!!
              theToken->SetAttributeCount(theAttrCount);
            }
            result = HandleStartToken(theToken);
          }
        }
        theBadTokenCount--;
      }
      if(theTopIndex != mBodyContext->GetCount()) {
         CloseContainersTo(mBodyContext->TagAt(theTopIndex),PR_TRUE);
      }
     
      // Put back tags, in temp context, into body context
      for(PRInt32 j=0; j<(theTagCount - theTopIndex); j++)
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

  CEntityToken* et = (CEntityToken*)(aToken);
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
  nsCParserNode aNode((CHTMLToken*)aToken,mLineNumber);

  #ifdef  RICKG_DEBUG
    WriteTokenToLog(aToken);
  #endif

  // You may find this hard to beleive, but this has to be here
  // so that the TBODY doesnt die when it sees a comment.
  // This case occurs on WWW.CREAF.COM
  eHTMLTags theTag=mBodyContext->mTags.Last();
  nsresult result=NS_OK;

  switch(theTag) {
    case eHTMLTag_table:
    case eHTMLTag_tr:
    case eHTMLTag_tbody:
    case eHTMLTag_td:
      break;
    default:
      if(mHasOpenBody){
        result=(mSink) ? mSink->AddComment(aNode) : NS_OK; 
      }
  }
  return result;
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
nsresult CNavDTD::HandleSkippedContentToken(CToken* aToken) {
  NS_PRECONDITION(0!=aToken,kNullToken);

  nsresult result=NS_OK;

  if(HasOpenContainer(eHTMLTag_body)) {
    nsCParserNode aNode((CHTMLToken*)aToken,mLineNumber);
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
  PRInt32 pos=GetTopmostIndexOf(eHTMLTag_body);
  PRInt32 attrCount=aNode.GetAttributeCount(PR_TRUE);

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
nsresult CNavDTD::CollectAttributes(nsCParserNode& aNode,PRInt32 aCount){
  int attr=0;

  nsresult result=NS_OK;
  if(aCount<=mTokenizer->GetCount()) {
    for(attr=0;attr<aCount;attr++){
      CToken* theToken=mTokenizer->PopToken();
      if(theToken)  {

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
  nsresult result=NS_OK;
  eHTMLTokenTypes theType=eToken_unknown;
  aCount=0;

  CToken* theToken=mTokenizer->PopToken();
  if(theToken) {
    theType=eHTMLTokenTypes(theToken->GetTokenType());
    if(eToken_skippedcontent==theType) {

  #ifdef  RICKG_DEBUG
    WriteTokenToLog(theToken);
  #endif

      aNode.SetSkippedContent(theToken);
      aCount++;
    } 
  }
  else {
    result=kEOF;
  }
  return result;
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

/**
 *  
 *  
 *  @update  gess 4/01/99
 *  @param   aTokenizer 
 *  @return  
 */
void CNavDTD::EmitMisplacedContent(nsITokenizer* aTokenizer){
/*
  if(aTokenizer){
    if(!mHadBodyOrFrameset){
      int index=0;
      int max=mMisplacedContent.GetSize();
      PRBool isBodyContent=PR_TRUE;
      for(index=0;index<max;index++){
        CToken* theToken=(CToken*)mMisplacedContent.ObjectAt(index);
        if(theToken){
          eHTMLTags theTag=(eHTMLTags)theToken->GetTypeID();    
          if(gHTMLElements[theTag].IsWhitespaceTag(theTag)){
            //ignore it...
          }
          if(gHTMLElements[eHTMLTag_body].SectionContains(theTag,PR_TRUE)){
            break; //stop, since you now know for sure to open the body...
          }
          else {
            static eHTMLTags frameTags[]={eHTMLTag_frame,eHTMLTag_noframes,eHTMLTag_frameset};
            if(FindTagInSet(theTag,frameTags,sizeof(frameTags)/sizeof(eHTMLTag_unknown))) {
              isBodyContent=PR_FALSE;
              break;
            }
          }
        }
      } //for
      if(isBodyContent){
        CTokenRecycler* theRecycler=(CTokenRecycler*)aTokenizer->GetTokenRecycler();
        CToken* theBodyToken=theRecycler->CreateTokenOfType(eToken_start,eHTMLTag_body);
        mMisplacedContent.PushFront(theBodyToken);
        //mMisplacedContent.PushFront(theBodyToken);
      }
    }
  }
*/
  aTokenizer->PrependTokens(mMisplacedContent);
}

/**
 *  This method is called to determine whether or not a tag
 *  can contain an explict style tag (font, italic, bold, etc.)
 *  Most can -- but some, like option, cannot. Therefore we
 *  don't bother to open transient styles within these elements.
 *  
 *  @update  gess 4/8/98
 *  @param   aParent -- tag enum of parent container
 *  @param   aChild -- tag enum of child container
 *  @return  PR_TRUE if parent can contain child
 */
PRBool CNavDTD::CanContainStyles(eHTMLTags aParent) const {
  PRBool result=PR_TRUE;
  switch(aParent) {
    case eHTMLTag_option:
      result=PR_FALSE; break;
    default:
      break;
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

  if(nsHTMLElement::IsContainer(aChildTag)){
    if(nsHTMLElement::IsBlockParent(aParentTag) || (gHTMLElements[aParentTag].GetSpecialChildren())) {
      while(eHTMLTag_unknown!=aChildTag) {
        if(parentCanContain){
          result=PR_TRUE;
          break;
        }//if
        CTagList* theTagList=gHTMLElements[aChildTag].GetRootTags();
        aChildTag=theTagList->mTags[0];
        parentCanContain=CanContain(aParentTag,aChildTag);
      }//while
    }//if
  }//if
  else if(nsHTMLElement::IsTextTag(aChildTag)){
    result=PR_TRUE;
  }
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
    return !HasOpenContainer(theAncestor);
  }

  if(gHTMLElements[aParent].HasSpecialProperty(kOmitWS)) {
    if(nsHTMLElement::IsTextTag(aChild)) {
      return PR_TRUE;
    }
  }

    //Now the obvious test: if the parent can contain the child, don't omit.
  if(gHTMLElements[aParent].CanContain(aChild)){
    return PR_FALSE;
  }

  if(nsHTMLElement::IsBlockElement(aParent)) {
    if(nsHTMLElement::IsInlineElement(aChild)) {  //feel free to drop inlines that a block doesn't contain.
      return PR_TRUE;
    }
  }

  if(gHTMLElements[aParent].HasSpecialProperty(kBadContentWatch)) {
    if(!gHTMLElements[aParent].CanContain(aChild)){
      return PR_TRUE;
    }
  }
  return PR_FALSE;
}
     

/**
 *  This method gets called to determine whether a given
 *  ENDtag can be omitted. Admittedly,this is a gross simplification.
 *  
 *  @update  gess 3/25/98
 *  @param   aTag -- tag to test for containership
 *  @return  PR_TRUE if given tag can contain other tags
 */
PRBool CNavDTD::CanOmitEndTag(eHTMLTags aParent,eHTMLTags aChild) const {
  PRBool  result=PR_FALSE;

  if(gHTMLElements[aChild].CanOmitEndTag(aParent)) {
    return PR_TRUE;
  }

  PRInt32 theChildIndex=GetIndexOfChildOrSynonym(mBodyContext->mTags,aChild);
  result=PRBool(kNotFound==theChildIndex);

  return result;
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
PRBool CNavDTD::ForwardPropagate(nsTagStack& aStack,eHTMLTags aParentTag,eHTMLTags aChildTag)  {
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
PRBool CNavDTD::BackwardPropagate(nsTagStack& aStack,eHTMLTags aParentTag,eHTMLTags aChildTag) const {

  eHTMLTags theParentTag=aChildTag;

  do {
    CTagList* theRootTags=gHTMLElements[theParentTag].GetRootTags();
    theParentTag=theRootTags->mTags[0];
    if(theParentTag!=eHTMLTag_unknown) {
      aStack.Push(theParentTag);
    }

    PRBool theCanContainResult=CanContain(aParentTag,theParentTag);

    if(theCanContainResult) {
      //we've found a complete sequence, so push the parent...
      theParentTag=aParentTag;
      aStack.Push(theParentTag);
    }
  } while((theParentTag!=eHTMLTag_unknown) && (theParentTag!=aParentTag));
  
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
 * Finds the topmost occurance of given tag within context vector stack.
 * @update	gess5/11/98
 * @param   tag to be found
 * @return  index of topmost tag occurance -- may be -1 (kNotFound).
 */
static
PRInt32 GetTopmostIndexOfBelowOffset(eHTMLTags aTag,PRInt32 anOffset){
  PRInt32 result=-1;
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
  return mBodyContext->mTags.GetTopmostIndexOf(aTag);
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
nsresult CNavDTD::OpenTransientStyles(eHTMLTags aTag){
  nsresult result=NS_OK;

  if(!FindTagInSet(aTag,gWhitespaceTags,sizeof(gWhitespaceTags)/sizeof(aTag))){ 
      //the following code builds the set of style tags to be opened...
    eHTMLTags theStyles[50];
    int theCount=0;

    int theStackPos=0;
    for(theStackPos=0;theStackPos<mBodyContext->GetCount();theStackPos++){
      nsTagStack* theStyleStack=mBodyContext->mStyles[theStackPos];
      if(theStyleStack) {
        int theTagPos=0;
        int count = theStyleStack->mTags->GetSize();
        for(theTagPos=0;theTagPos<count;theTagPos++){
          if((0==theCount) || (!FindTagInSet(theStyleStack->TagAt(theTagPos),theStyles,theCount))) {
            theStyles[theCount++]=theStyleStack->TagAt(theTagPos);
          }
        }
      }
    }
    theStyles[theCount]=eHTMLTag_unknown;

      //now iterate style set, and open the containers...
    for(theStackPos=0;theStackPos<theCount;theStackPos++){
      CStartToken   token(theStyles[theStackPos]);
      nsCParserNode theNode(&token,mLineNumber);
      token.SetTypeID(theStyles[theStackPos]); 
      result=OpenContainer(theNode,PR_FALSE);
    }
    mBodyContext->mOpenStyles=theCount;
  }
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
nsresult CNavDTD::CloseTransientStyles(eHTMLTags aTag){
  nsresult result=NS_OK;

  int theTagPos=0;

    //now iterate style set, and close the containers...
  for(theTagPos=mBodyContext->mOpenStyles;theTagPos>0;theTagPos--){
    eHTMLTags theTag=GetTopNode();
    CStartToken   token(theTag);
    nsCParserNode theNode(&token,mLineNumber);
    token.SetTypeID(theTag); 
    result=CloseContainer(theNode,theTag,PR_FALSE);
  }
  mBodyContext->mOpenStyles=0;
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
  if(!mHasOpenHead++) {
    nsresult result=(mSink) ? mSink->OpenHead(aNode) : NS_OK; 
  }
  return NS_OK;
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
  if(mHasOpenHead) {
    if(0==--mHasOpenHead){
      nsresult result=(mSink) ? mSink->CloseHead(aNode) : NS_OK; 
    }
  }
  //mBodyContext->Pop();
  return NS_OK;
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
  eHTMLTags topTag=mBodyContext->Last();

  if(eHTMLTag_html!=topTag) {
    
    //ok, there are two cases:
    //  1. Nobody opened the html container
    //  2. Someone left the head (or other) open
    PRInt32 pos=GetTopmostIndexOf(eHTMLTag_html);
    if(kNotFound!=pos) {
      //if you're here, it means html is open, 
      //but some other tag(s) are in the way.
      //So close other tag(s).
      result=CloseContainersTo(pos+1,eHTMLTag_body,PR_TRUE);
    } else {
      //if you're here, it means that there is
      //no HTML tag in document. Let's open it.

      result=CloseContainersTo(0,eHTMLTag_html,PR_TRUE);  //close current stack containers.
      nsAutoString  theEmpty;
      CHTMLToken    token(theEmpty,eHTMLTag_html);
      nsCParserNode htmlNode(&token,mLineNumber);
      result=OpenHTML(htmlNode);  //open the html container...
    }
  }

  if(NS_OK==result) {
    result=(mSink) ? mSink->OpenBody(aNode) : NS_OK; 
    mBodyContext->Push((eHTMLTags)aNode.GetNodeType());

    /*now THIS is a hack to support plaintext documents in this DTD...
    if((NS_OK==result) && mIsPlaintext) {
      CStartToken theToken(eHTMLTag_pre);  //open the body container...
      result=HandleStartToken(&theToken);      
    }
    */
  }

  mTokenizer->PrependTokens(mMisplacedContent);
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
  nsresult result=(mSink) ? mSink->OpenFrameset(aNode) : NS_OK; 
  mBodyContext->Push((eHTMLTags)aNode.GetNodeType());
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
 * @return  TRUE if ok, FALSE if error
 */
nsresult
CNavDTD::OpenContainer(const nsIParserNode& aNode,PRBool aUpdateStyleStack){
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
        PRInt32 theCount;
        nsCParserNode& theCNode=*(nsCParserNode*)&aNode;
        CollectSkippedContent(theCNode,theCount);
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

  if((NS_OK==result) && (PR_TRUE==aUpdateStyleStack)){
    UpdateStyleStackForOpenTag(nodeType,nodeType);
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
nsresult
CNavDTD::CloseContainer(const nsIParserNode& aNode,eHTMLTags aTag,
                        PRBool aUpdateStyles){
  nsresult   result=NS_OK;
  eHTMLTags nodeType=(eHTMLTags)aNode.GetNodeType();
  
  #define K_CLOSEOP 200
  CRCStruct theStruct(nodeType,K_CLOSEOP);
  mComputedCRC32=AccumulateCRC(mComputedCRC32,(char*)&theStruct,sizeof(theStruct));

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

  if((NS_OK==result) && (PR_TRUE==aUpdateStyles)){
    UpdateStyleStackForCloseTag(nodeType,aTag);
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
nsresult CNavDTD::CloseContainersTo(PRInt32 anIndex,eHTMLTags aTag, PRBool aUpdateStyles){
  NS_PRECONDITION(mBodyContext->GetCount() > 0, kInvalidTagStackPos);
  nsresult result=NS_OK;

  nsAutoString  theEmpty;
  CEndToken     theToken(theEmpty);
  nsCParserNode theNode(&theToken,mLineNumber);

  if((anIndex<mBodyContext->GetCount()) && (anIndex>=0)) {
    while(mBodyContext->GetCount()>anIndex) {
      eHTMLTags theTag=mBodyContext->Last();
      theToken.SetTypeID(theTag);
      result=CloseContainer(theNode,aTag,aUpdateStyles);
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
nsresult CNavDTD::CloseContainersTo(eHTMLTags aTag,PRBool aUpdateStyles){
  NS_PRECONDITION(mBodyContext->GetCount() > 0, kInvalidTagStackPos);

  PRInt32 pos=GetTopmostIndexOf(aTag);

  if(kNotFound!=pos) {
    //the tag is indeed open, so close it.
    return CloseContainersTo(pos,aTag,aUpdateStyles);
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
      return CloseContainersTo(pos,aTag,aUpdateStyles);
    }
  }
  
  nsresult result=NS_OK;
  CTagList* theTagList=gHTMLElements[aTag].GetRootTags();
  eHTMLTags theParentTag=theTagList->mTags[0];
  pos=GetTopmostIndexOf(theParentTag);
  if(kNotFound!=pos) {
    //the parent container is open, so close it instead
    result=CloseContainersTo(pos+1,aTag,aUpdateStyles);
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
nsresult CNavDTD::CloseTopmostContainer(){
  NS_PRECONDITION(mBodyContext->GetCount() > 0, kInvalidTagStackPos);

  nsAutoString  theEmpty;
  CEndToken     theToken(theEmpty);
  eHTMLTags theTag=mBodyContext->Last();
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
  nsresult result=(mSink) ? mSink->AddLeaf(aNode) : NS_OK; 
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
nsresult CNavDTD::AddHeadLeaf(const nsIParserNode& aNode){
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
      if(eHTMLTag_title==theTag)
        mSink->SetTitle(aNode.GetSkippedContent());
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
  
  static nsTagStack kPropagationStack;
  kPropagationStack.Empty();
  
  nsresult  result=(nsresult)kContextMismatch;
  PRInt32   cnt=0;
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
  PRInt32 count = kPropagationStack.mTags->GetSize();
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

  if(nsHTMLElement::IsStyleTag(aTag)) {
    nsTagStack* theStyleStack=mBodyContext->mStyles[mBodyContext->GetCount()-1];
    if(theStyleStack){
      theStyleStack->Push(aTag);
    }
  }
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
  
  if(mBodyContext->mStyles){
    nsTagStack* theStyleStack=mBodyContext->mStyles[mBodyContext->GetCount()-1];
    if(theStyleStack){
      if(nsHTMLElement::IsStyleTag(aTag)) {
        if(aTag==anActualTag) {
          theStyleStack->Pop();
          mBodyContext->mOpenStyles--;
        }
      }
    }//if
  }//if
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

