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
#include "nsVoidArray.h"
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

#ifdef XP_PC
#include <direct.h> //this is here for debug reasons...
#endif
#include "prmem.h"


static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);                 
static NS_DEFINE_IID(kIDTDIID,      NS_IDTD_IID);
static NS_DEFINE_IID(kClassIID,     NS_INAVHTML_DTD_IID); 
 
//static const char* kNullURL = "Error: Null URL given";
//static const char* kNullFilename= "Error: Null filename given";
static const char* kNullToken = "Error: Null token given";
static const char* kInvalidTagStackPos = "Error: invalid tag stack position";
static const char* kHTMLTextContentType = "text/html";
static char* kVerificationDir = "c:/temp";
static const char* kViewSourceCommand= "view-source";
 
static nsAutoString gEmpty;

static eHTMLTags gFormElementTags[]= {  
    eHTMLTag_button,  eHTMLTag_fieldset,  eHTMLTag_input,
    eHTMLTag_isindex, eHTMLTag_label,     eHTMLTag_legend,
    eHTMLTag_option,  eHTMLTag_optgroup,  eHTMLTag_select,
    eHTMLTag_textarea};

static eHTMLTags gHeadingTags[]={
  eHTMLTag_h1,  eHTMLTag_h2,  eHTMLTag_h3,  
  eHTMLTag_h4,  eHTMLTag_h5,  eHTMLTag_h6};

  
static eHTMLTags gStyleTags[]={
  eHTMLTag_a,       eHTMLTag_acronym,   eHTMLTag_b,
  eHTMLTag_bdo,     eHTMLTag_big,       eHTMLTag_blink,
  eHTMLTag_center,  eHTMLTag_cite,      eHTMLTag_code,
  eHTMLTag_del,     eHTMLTag_dfn,       eHTMLTag_em,
  eHTMLTag_font,    eHTMLTag_i,         eHTMLTag_ins,
  eHTMLTag_kbd,     eHTMLTag_nobr,      eHTMLTag_q,
  eHTMLTag_s,       eHTMLTag_samp,      eHTMLTag_small,
  eHTMLTag_span,    eHTMLTag_strike,    eHTMLTag_strong,
  eHTMLTag_sub,     eHTMLTag_sup,       eHTMLTag_tt,
  eHTMLTag_u,       eHTMLTag_var};


static eHTMLTags gTableChildTags[]={ 
  eHTMLTag_caption, eHTMLTag_col, eHTMLTag_colgroup,  eHTMLTag_tbody,   
  eHTMLTag_tfoot,   eHTMLTag_tr,  eHTMLTag_thead,     eHTMLTag_td};
  
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
  CTagHandlerRegister() : mDeallocator(), mTagHandlerDeque(mDeallocator) {
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

  CTagHandlerDeallocator  mDeallocator;
  nsDeque                 mTagHandlerDeque;
  CTagFinder              mTagFinder;
};


/************************************************************************
  The CTagHandlerRegister for a CNavDTD.
  This is where special taghanders for our tags can be managed and called from
  Note:  This can also be attached to some object so it can be refcounted 
  and destroyed if you want this to go away when not imbedded.
 ************************************************************************/
CTagHandlerRegister gTagHandlerRegister;


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
 *  Default constructor
 *   
 *  @update  gess 4/9/98
 *  @param   
 *  @return  
 */
CNavDTD::CNavDTD() : nsIDTD(){
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
  mAllowUnknownTags=PR_FALSE;
  InitializeDefaultTokenHandlers(); 
  mHeadContext=new nsDTDContext();
  mBodyContext=new nsDTDContext();
  mFormContext=0;
  mMapContext=0;
  mTokenizer=0;

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
PRBool CNavDTD::CanParse(nsString& aContentType, nsString& aCommand, PRInt32 aVersion){
  PRBool result=PR_FALSE;
  if(!aCommand.Equals(kViewSourceCommand)) {
    result=aContentType.Equals(kHTMLTextContentType);
  }
  return result;
}

/**
 * 
 * @update  gess7/7/98
 * @param 
 * @return
 */
eAutoDetectResult CNavDTD::AutoDetectContentType(nsString& aBuffer,nsString& aType){
  eAutoDetectResult result=eUnknownDetect;
  if(PR_TRUE==aType.Equals(kHTMLTextContentType)) 
    result=eValidDetect;
  else {
    //otherwise, look into the buffer to see if you recognize anything...
    if(BufferContainsHTML(aBuffer)){
      result=eValidDetect;
      if(0==aType.Length())
        aType=kHTMLTextContentType;
    }
  }
  return result;
}

/**
 * 
 * @update  gess5/18/98
 * @param 
 * @return
 */
nsresult CNavDTD::WillBuildModel(nsString& aFilename,PRBool aNotifySink,nsIParser* aParser){
  nsresult result=NS_OK;

  mFilename=aFilename;
  if(aParser){
    mSink=(nsIHTMLContentSink*)aParser->GetContentSink();
    if((aNotifySink) && (mSink)) {
      mHasOpenBody=PR_FALSE;
      mHadBodyOrFrameset=PR_FALSE;
      mLineNumber=1;
      result = mSink->WillBuildModel();
    }
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
  * @return	error code (almost always 0)
  */
nsresult CNavDTD::BuildModel(nsIParser* aParser,nsITokenizer* aTokenizer) {
  nsresult result=NS_OK;

  if(aTokenizer) {
    nsITokenizer*  oldTokenizer=mTokenizer;
    mTokenizer=aTokenizer;
    nsITokenRecycler* theRecycler=aTokenizer->GetTokenRecycler();

    while(NS_OK==result){
      CToken* theToken=mTokenizer->PopToken();
      if(theToken) {
        result=HandleToken(theToken,aParser);
        if(NS_SUCCEEDED(result)) {
          theRecycler->RecycleToken(theToken);
        }
        else if(NS_ERROR_HTMLPARSER_BLOCK!=result){
          mTokenizer->PushTokenFront(theToken);
        }
        // theRootDTD->Verify(kEmptyString,aParser);
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
nsresult CNavDTD::DidBuildModel(nsresult anErrorCode,PRBool aNotifySink,nsIParser* aParser){
  nsresult result= NS_OK;

  if((NS_OK==anErrorCode) && (!mHadBodyOrFrameset)) {
    CStartToken theToken(eHTMLTag_body);  //open the body container...
    result=HandleStartToken(&theToken);
  }

  if(aParser){
    mSink=(nsIHTMLContentSink*)aParser->GetContentSink();

    if(aNotifySink && mSink){
      if((NS_OK==anErrorCode) && (mBodyContext->GetCount()>0)) {
        result = CloseContainersTo(0,eHTMLTag_unknown,PR_FALSE);
      }

      result = mSink->DidBuildModel(1);

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
 *  @param   aType
 *  @param   aToken
 *  @param   aParser
 *  @return  
 */
nsresult CNavDTD::HandleToken(CToken* aToken,nsIParser* aParser){
  nsresult result=NS_OK;

  if(aToken) {
    CHTMLToken*     theToken= (CHTMLToken*)(aToken);
    eHTMLTokenTypes theType=eHTMLTokenTypes(theToken->GetTokenType());
    CITokenHandler* theHandler=GetTokenHandler(theType);

    if(theHandler) {
      mParser=(nsParser*)aParser;
      mSink=(nsIHTMLContentSink*)mParser->GetContentSink();
      result=(*theHandler)(theToken,this);
      if (mDTDDebug) {
         //mDTDDebug->Verify(this, mParser, mBodyContext->GetCount(), mBodyContext->mTags, mFilename);
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
nsresult CNavDTD::DidHandleStartTag(CToken* aToken,eHTMLTags aChildTag){
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
        if(theNextToken){
          eHTMLTokenTypes theType=eHTMLTokenTypes(theNextToken->GetTokenType());
          if(eToken_skippedcontent==theType){
            nsString& theText=((CAttributeToken*)theNextToken)->GetKey();
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
PRInt32 GetTopmostIndexOf(eHTMLTags aTag,nsTagStack& aTagStack) {
  int i=0;
  for(i=aTagStack.mCount-1;i>=0;i--){
    if(aTagStack.mTags[i]==aTag)
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
eHTMLTags FindAutoCloseTargetForStartTag(eHTMLTags aCurrentTag,nsTagStack& aTagStack) {
  int theTopIndex=aTagStack.mCount;
  eHTMLTags aPrevTag=aTagStack.mTags[theTopIndex-1];
 
  if(nsHTMLElement::IsContainer(aCurrentTag)){
    if(aPrevTag==aCurrentTag) {
      return (gHTMLElements[aCurrentTag].CanSelfContain()) ? eHTMLTag_unknown: aCurrentTag;
    }
    if(nsHTMLElement::IsBlockCloser(aCurrentTag)) {

      CTagList* theRootTags=gHTMLElements[aCurrentTag].GetRootTags();
      if(theRootTags) {
        PRInt32 theRootIndex=theRootTags->GetTopmostIndexOf(aTagStack);
        if(theRootIndex<GetTopmostIndexOf(aCurrentTag,aTagStack)) {
          return aCurrentTag;
        }
      }
 
      CTagList* theCloseTags=gHTMLElements[aCurrentTag].GetAutoCloseStartTags();
      if(theCloseTags){
        PRInt32 thePeerIndex=theCloseTags->GetTopmostIndexOf(aTagStack);
        if(kNotFound!=thePeerIndex){
          if(thePeerIndex==theTopIndex-1) {
            //the guy you can autoclose is on the top of the stack...
            return aPrevTag;
          } //if
        } //if
      }//if

    } //if
    else if(nsHTMLElement::IsInlineElement(aCurrentTag)) {
    }//if 
  } //if
  return eHTMLTag_unknown;
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
 
  static eHTMLTags gBodyBlockers[]={eHTMLTag_body,eHTMLTag_frameset,eHTMLTag_map};
  PRInt32 theBodyBlocker=GetTopmostIndexOf(gBodyBlockers,sizeof(gBodyBlockers)/sizeof(eHTMLTag_unknown));
  if((kNotFound==theBodyBlocker) && (!mHasOpenHead)){
    if(CanPropagate(eHTMLTag_body,aChildTag)) {
      mHasOpenBody=PR_TRUE;
      CStartToken theToken(eHTMLTag_body);  //open the body container...
      result=HandleStartToken(&theToken);
    }
  }//if                      

  eHTMLTags theParentTag=mBodyContext->Last();
  PRBool theCanContainResult=CanContain(theParentTag,aChildTag);

  //  Ok Genius answer this:  
  //  If the parent can contain the child, why would it be necessary
  //  to find an autoclose target?          

  if(!theCanContainResult) {
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

  if(IsContainer(aChildTag)){
      //first, let's see if it's a style element...
    if(!FindTagInSet(aChildTag,gStyleTags,sizeof(gStyleTags)/sizeof(eHTMLTag_unknown))) {
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

  //now do any post processing necessary on the tag...
  if(NS_OK==result)
    DidHandleStartTag(aToken,aChildTag);
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
 *  @update  vidur 11/14/98
 *  @param   aToken -- next (start) token to be handled
 *  @param   aNode -- CParserNode representing this start token
 *  @return  PR_TRUE if all went well; PR_FALSE if error occured
 */      
nsresult CNavDTD::HandleStartToken(CToken* aToken) {
  NS_PRECONDITION(0!=aToken,kNullToken);

  //Begin by gathering up attributes...
  eHTMLTags     theChildTag=(eHTMLTags)aToken->GetTypeID();
  nsCParserNode attrNode((CHTMLToken*)aToken,mLineNumber,GetTokenRecycler()); 
  PRInt16       attrCount=aToken->GetAttributeCount();
  nsresult      result=(0==attrCount) ? NS_OK : CollectAttributes(attrNode,attrCount);
  eHTMLTags     theParent=mBodyContext->Last();
  PRInt32       theAttrCount;
 
  if(NS_OK==result) {
    switch(theChildTag) { 

      case eHTMLTag_title:
        {
          result=OpenHead(attrNode);
          if(NS_OK==result) {
            result=CollectSkippedContent(attrNode,theAttrCount);
            mSink->SetTitle(attrNode.GetSkippedContent());
            if(NS_OK==result)
              result=CloseHead(attrNode);
          }
        }
        break;

      case eHTMLTag_base:
      case eHTMLTag_link:
      case eHTMLTag_meta:
        result=AddHeadLeaf(attrNode);
        break;

      case eHTMLTag_style:
        {
          result=OpenHead(attrNode);
          if(NS_OK==result) {
            PRInt32 theCount;
            CollectSkippedContent(attrNode,theCount);
            if(NS_OK==result) {
              result=AddLeaf(attrNode);
              // XXX If the return value tells us to block, go
              // ahead and close the tag out anyway, since its
              // contents will be consumed.
              if (NS_SUCCEEDED(result)) {
                nsresult rv = CloseHead(attrNode);
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
        } 
        break;

      case eHTMLTag_area:
        if (mHasOpenMap) {
          result = mSink->AddLeaf(attrNode);
        }
        break;

      default:
        if(PR_FALSE==CanOmit(theParent,theChildTag)) {
          result=HandleDefaultStartToken(aToken,theChildTag,attrNode);
        }
        break;
    } //switch
  } //if

  if(eHTMLTag_newline==theChildTag)
    mLineNumber++;

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
nsresult CNavDTD::HandleEndToken(CToken* aToken) {
  NS_PRECONDITION(0!=aToken,kNullToken);

  nsresult    result=NS_OK;
  CEndToken*  et = (CEndToken*)(aToken);
  eHTMLTags   tokenTagType=(eHTMLTags)et->GetTypeID();
  

  // Here's the hacky part: 
  // Because we're trying to be backward compatible with Nav4/5, 
  // we have to handle explicit styles the way it does. That means
  // that we keep an internal style stack.When an EndToken occurs, 
  // we should see if it is an explicit style tag. If so, we can 
  // close the explicit style tag (goofy, huh?)


  //now check to see if this token should be omitted, or 
  //if it's gated from closing by the presence of another tag.
  if(PR_TRUE==CanOmitEndTag(mBodyContext->Last(),tokenTagType)) {
    UpdateStyleStackForCloseTag(tokenTagType,tokenTagType);
    return result;
  }

  nsCParserNode theNode((CHTMLToken*)aToken,mLineNumber);
  switch(tokenTagType) {

    case eHTMLTag_style:
    case eHTMLTag_link:
    case eHTMLTag_meta:
    case eHTMLTag_textarea:
    case eHTMLTag_title:
    case eHTMLTag_script:
      break;

    case eHTMLTag_map:
    case eHTMLTag_form:
      result=CloseContainer(theNode,tokenTagType,PR_FALSE);
      break;

    case eHTMLTag_td:
    case eHTMLTag_th:
      result=CloseContainersTo(tokenTagType,PR_TRUE); 
      // Empty the transient style stack (we just closed any extra
      // ones off so it's safe to do it now) because they don't carry
      // forward across table cell boundaries.
      //mBodyContext->mStyles->mCount=0;
      break;

    default:
      if(IsContainer(tokenTagType)){
        result=CloseContainersTo(tokenTagType,PR_TRUE); 
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
nsresult CNavDTD::HandleEntityToken(CToken* aToken) {
  NS_PRECONDITION(0!=aToken,kNullToken);

  CEntityToken* et = (CEntityToken*)(aToken);
  nsresult      result=NS_OK;

  if(PR_FALSE==CanOmit(mBodyContext->Last(),eHTMLTag_entity)) {
    nsCParserNode aNode((CHTMLToken*)aToken,mLineNumber);
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
  nsresult result=mSink->AddComment(aNode); 
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
 *  encountered in the parse process. 
 *  
 *  @update  gess 3/25/98
 *  @param   aToken -- next (start) token to be handled
 *  @return  PR_TRUE if all went well; PR_FALSE if error occured
 */
nsresult CNavDTD::HandleScriptToken(nsCParserNode& aNode) {
  nsresult result=NS_OK;

  PRInt32 pos=GetTopmostIndexOf(eHTMLTag_body);
  PRInt32 attrCount=aNode.GetAttributeCount(PR_TRUE);

  if (kNotFound == pos) {
    // We're in the HEAD, but don't bother to open it.
    if(NS_OK==result) {
      CollectSkippedContent(aNode,attrCount);
      CloseHead(aNode);
      result=AddLeaf(aNode);
    }//if
  }//if
  else {
    // We're in the BODY
    CollectSkippedContent(aNode,attrCount);
    result=AddLeaf(aNode);
  }
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
  nsresult result=mSink->AddProcessingInstruction(aNode); 
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
      return gHTMLElements[aParent].CanSelfContain();
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

  if(IsContainer(aChildTag)){
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
  PRBool result=PR_FALSE;

  if((eHTMLTag_userdefined==aParent) && mAllowUnknownTags)
    return result;

  if(eHTMLTag_head==aChild) {
    if(HasOpenContainer(eHTMLTag_body))
      return PR_TRUE;
  }

  static eHTMLTags canSkip[]={eHTMLTag_body,eHTMLTag_html,eHTMLTag_head};
  if(FindTagInSet(aChild,canSkip,sizeof(canSkip)/sizeof(eHTMLTag_unknown))) {
    if(HasOpenContainer(aChild))
      return PR_TRUE;
  }

  //begin with some simple (and obvious) cases...
  switch(aParent) {
    case eHTMLTag_table:
    case eHTMLTag_tbody:
      if((eHTMLTag_form==aChild) || (eHTMLTag_table==aChild))
        result=PR_FALSE;
      else if(FindTagInSet(aChild,gFormElementTags,sizeof(gFormElementTags)/sizeof(eHTMLTag_unknown)))
        result=!HasOpenContainer(eHTMLTag_form);
      else result=!FindTagInSet(aChild,gTableChildTags,sizeof(gTableChildTags)/sizeof(eHTMLTag_unknown));
      break;

    case eHTMLTag_tr:
      switch(aChild) {
        case eHTMLTag_td:
        case eHTMLTag_th:
        case eHTMLTag_form:
        case eHTMLTag_tr:
          result=PR_FALSE;
          break;
        default:
          if(FindTagInSet(aChild,gFormElementTags,sizeof(gFormElementTags)/sizeof(eHTMLTag_unknown)))
            result=!HasOpenContainer(eHTMLTag_form);
          else result=FindTagInSet(aChild,gWhitespaceTags,sizeof(gWhitespaceTags)/sizeof(eHTMLTag_unknown));
      }
      break;

    case eHTMLTag_unknown:
      {
        static eHTMLTags canSkip2[]={eHTMLTag_newline,eHTMLTag_whitespace};
        if(FindTagInSet(aChild,canSkip2,sizeof(canSkip2)/sizeof(eHTMLTag_unknown))) {
          result=PR_TRUE;
        }
      }
      break;

    case eHTMLTag_head:
      if(eHTMLTag_body==aChild)
        result=PR_FALSE;
      else result=!gHeadKids.Contains(aChild);
      break;

    default:

        //ok, since no parent claimed it, test based on the child...
      switch(aChild) {

        case eHTMLTag_userdefined:
        case eHTMLTag_comment:
          result=PR_TRUE; 
          break;

        case eHTMLTag_body:
          result=HasOpenContainer(eHTMLTag_frameset);
          break;

        case eHTMLTag_fieldset:
        case eHTMLTag_input:        case eHTMLTag_isindex:
        case eHTMLTag_label:        case eHTMLTag_legend:
        case eHTMLTag_select:       case eHTMLTag_textarea:
        case eHTMLTag_option:
          result=!HasOpenContainer(eHTMLTag_form);
          break;

        case eHTMLTag_newline:    
        case eHTMLTag_whitespace:

          switch(aParent) {
            case eHTMLTag_html:     case eHTMLTag_head:   
            case eHTMLTag_title:    case eHTMLTag_map:    
            case eHTMLTag_tr:       case eHTMLTag_table:  
            case eHTMLTag_thead:    case eHTMLTag_tfoot:  
            case eHTMLTag_tbody:    case eHTMLTag_col:    
            case eHTMLTag_colgroup: case eHTMLTag_unknown:
            case eHTMLTag_select:   case eHTMLTag_fieldset:
            case eHTMLTag_frameset: case eHTMLTag_dl:
              result=PR_TRUE;
            default:
              break;
          } //switch
          break;

          //this code prevents table container elements from
          //opening unless a table is actually already opened.
        case eHTMLTag_tr:       case eHTMLTag_thead:    
        case eHTMLTag_tfoot:    case eHTMLTag_tbody:    
        case eHTMLTag_td:       case eHTMLTag_th:
        case eHTMLTag_caption:
          if(PR_FALSE==HasOpenContainer(eHTMLTag_table))
            result=PR_TRUE;
          break;

        case eHTMLTag_entity:
          switch(aParent) {
            case eHTMLTag_tr:       case eHTMLTag_table:  
            case eHTMLTag_thead:    case eHTMLTag_tfoot:  
            case eHTMLTag_tbody:    
              result=PR_TRUE;
            default:
              break;
          } //switch
          break;

        case eHTMLTag_frame:
          if(eHTMLTag_iframe==aParent)
            result=PR_TRUE;
          break;

        default:

          static eHTMLTags kNonStylizedTabletags[]={eHTMLTag_table,eHTMLTag_tbody,eHTMLTag_tr};
          if(FindTagInSet(aChild,gStyleTags,sizeof(gStyleTags)/sizeof(eHTMLTag_unknown))) {
            if(FindTagInSet(aParent,kNonStylizedTabletags,sizeof(kNonStylizedTabletags)/sizeof(eHTMLTag_unknown)))
              return PR_TRUE;
          }
          break;
      } //switch
      break;
  }
  return result;
}


/**
 * This method is called when you want to determine if one tag is
 * synonymous with another. Cases where this are true include style
 * tags (where <i> is allowed to close <b> for example). Another
 * is <H?>, where any open heading tag can be closed by any close heading tag.
 * @update  gess6/16/98
 * @param 
 * @return
 */
PRBool IsCompatibleTag(eHTMLTags aTag1,eHTMLTags aTag2) {
  PRBool result=PR_FALSE;

  if(FindTagInSet(aTag1,gStyleTags,sizeof(gStyleTags)/sizeof(eHTMLTag_unknown))) {
    result=FindTagInSet(aTag2,gStyleTags,sizeof(gStyleTags)/sizeof(eHTMLTag_unknown));
  }
  if(FindTagInSet(aTag1,gHeadingTags,sizeof(gHeadingTags)/sizeof(eHTMLTag_unknown))) {
    result=FindTagInSet(aTag2,gHeadingTags,sizeof(gHeadingTags)/sizeof(eHTMLTag_unknown));
  }
  return result;
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
eHTMLTags FindAutoCloseTargetForEndTag(eHTMLTags aCurrentTag,nsTagStack& aTagStack,PRInt32 aChildIndex) {
  int theTopIndex=aTagStack.mCount;
  eHTMLTags aPrevTag=aTagStack.mTags[theTopIndex-1];
 
  if(nsHTMLElement::IsContainer(aCurrentTag)){
    if(aPrevTag==aCurrentTag){
      return aCurrentTag;
    }
    
    if(nsHTMLElement::IsBlockCloser(aCurrentTag)) {

        /*here's what to do: 
            Our here is sitting at aChildIndex. There are other tags above it
            on the stack. We have to try to close them out, but we may encounter
            one that can block us. The way to tell is by comparing each tag on
            the stack against our closeTag and rootTag list. 

            For each tag above our here on the stack, ask 3 questions:
              1. Is it in the closeTag list? If so, the we can skip over it
              2. Is it in the rootTag list? If so, then we're gated by it
              3. Otherwise its non-specified and we simply presume we can close it.
        */

      CTagList* theCloseTags=gHTMLElements[aCurrentTag].GetAutoCloseEndTags();
      CTagList* theRootTags=gHTMLElements[aCurrentTag].GetRootTags();
      if(theCloseTags){  

        while(aChildIndex<--theTopIndex) {
          eHTMLTags theNextTag=aTagStack.mTags[theTopIndex];
          if(PR_FALSE==theCloseTags->Contains(theNextTag)) {
            if(PR_FALSE==theRootTags->Contains(theNextTag)) {
              break; //we encountered a tag in root list so fail (because we're gated).
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
        PRInt32 theRootIndex=theRootTags->GetTopmostIndexOf(aTagStack);          
        return (theRootIndex<aChildIndex) ? aCurrentTag : eHTMLTag_unknown;
      }
    } //if
  } //if
  return eHTMLTag_unknown;
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

  //begin with some simple (and obvious) cases...
  switch((eHTMLTags)aChild) {

    case eHTMLTag_userdefined:
    case eHTMLTag_comment:
      result=PR_TRUE; 
      break;

    case eHTMLTag_a:
      result=!HasOpenContainer(aChild);
      break;

    case eHTMLTag_html:
    case eHTMLTag_body:
      result=PR_TRUE;
      break;
      
    case eHTMLTag_newline:    
    case eHTMLTag_whitespace:

      switch(aParent) {
        case eHTMLTag_html:     case eHTMLTag_head:   
        case eHTMLTag_title:    case eHTMLTag_map:    
        case eHTMLTag_tr:       case eHTMLTag_table:  
        case eHTMLTag_thead:    case eHTMLTag_tfoot:  
        case eHTMLTag_tbody:    case eHTMLTag_col:    
        case eHTMLTag_colgroup: case eHTMLTag_unknown:
          result=PR_TRUE;
        default:
          break;
      } //switch
      break;

      //It turns out that a <Hn> can be closed by any other <H?>
      //This code makes them all seem compatible.
    case eHTMLTag_h1:           case eHTMLTag_h2:
    case eHTMLTag_h3:           case eHTMLTag_h4:
    case eHTMLTag_h5:           case eHTMLTag_h6:
      if(FindTagInSet(aParent,gHeadingTags,sizeof(gHeadingTags)/sizeof(eHTMLTag_unknown))) {
        result=PR_FALSE;
        break;
      }
      //Otherwise, IT's OK TO FALL THROUGH HERE...


    default: 
      {
        PRInt32 theTagPos=GetTopmostIndexOf(aChild);
        eHTMLTags theTarget=FindAutoCloseTargetForEndTag(aChild,mBodyContext->mTags,theTagPos);
        result=PRBool(eHTMLTag_unknown==theTarget);
      }
      break;
  } //switch 
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
  PRBool result=nsHTMLElement::IsContainer((eHTMLTags)aTag);
  return result;
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
  return ::GetTopmostIndexOf(aTag,mBodyContext->mTags);
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
        for(theTagPos=0;theTagPos<theStyleStack->mCount;theTagPos++){
          if((0==theCount) || (!FindTagInSet(theStyleStack->mTags[theTagPos],theStyles,theCount))) {
            theStyles[theCount++]=theStyleStack->mTags[theTagPos];
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

  nsresult result=mSink->OpenHTML(aNode); 
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
  nsresult result=mSink->CloseHTML(aNode); 
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
    nsresult result=mSink->OpenHead(aNode); 
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
  if(0==--mHasOpenHead){
    nsresult result=mSink->CloseHead(aNode); 
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

      CHTMLToken    token(gEmpty,eHTMLTag_html);
      nsCParserNode htmlNode(&token,mLineNumber);
      result=OpenHTML(htmlNode);  //open the html container...
    }
  }

  if(NS_OK==result) {
    result=mSink->OpenBody(aNode); 
    mBodyContext->Push((eHTMLTags)aNode.GetNodeType());
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
  nsresult result=mSink->CloseBody(aNode); 
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
  nsresult result=mSink->OpenForm(aNode);
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
  nsresult result=NS_OK;
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
nsresult CNavDTD::OpenMap(const nsIParserNode& aNode){
  if(mHasOpenMap)
    CloseMap(aNode);
  nsresult result=mSink->OpenMap(aNode);
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
  nsresult result=NS_OK;
  if(mHasOpenMap) {
    mHasOpenMap=PR_FALSE;
    result=mSink->CloseMap(aNode); 
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
  nsresult result=mSink->OpenFrameset(aNode); 
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
  nsresult result=mSink->CloseFrameset(aNode); 
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

  switch(nodeType) {

    case eHTMLTag_html:
      result=OpenHTML(aNode); break;

    case eHTMLTag_body:
      mHasOpenBody=PR_TRUE;
      result=OpenBody(aNode); break;

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
      result=OpenFrameset(aNode); break;

    case eHTMLTag_script:
      {
        nsCParserNode& theCNode=*(nsCParserNode*)&aNode;
        result=HandleScriptToken(theCNode);
      }
      break;

    case eHTMLTag_head:
     // result=OpenHead(aNode); //open the head...
      break;

    default:
      result=mSink->OpenContainer(aNode); 
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
  NS_PRECONDITION(mBodyContext->GetCount() > 0, kInvalidTagStackPos);
  nsresult   result=NS_OK;
  eHTMLTags nodeType=(eHTMLTags)aNode.GetNodeType();
  
  //XXX Hack! We know this is wrong, but it works
  //for the general case until we get it right.
  switch(nodeType) {

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

    case eHTMLTag_map:
      result=CloseMap(aNode);
      break;

    case eHTMLTag_form:
      result=CloseForm(aNode); break;

    case eHTMLTag_frameset:
      result=CloseFrameset(aNode); break;

    case eHTMLTag_title:
    default:
      result=mSink->CloseContainer(aNode); 
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
nsresult
CNavDTD::CloseContainersTo(PRInt32 anIndex,eHTMLTags aTag,
                           PRBool aUpdateStyles){
  NS_PRECONDITION(mBodyContext->GetCount() > 0, kInvalidTagStackPos);
  nsresult result=NS_OK;

  static CEndToken aToken(gEmpty);
  nsCParserNode theNode(&aToken,mLineNumber);

  if((anIndex<mBodyContext->GetCount()) && (anIndex>=0)) {
    while(mBodyContext->GetCount()>anIndex) {
      eHTMLTags theTag=mBodyContext->Last();
      aToken.SetTypeID(theTag);
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
  if(IsCompatibleTag(aTag,theTopTag)) {
    //if you're here, it's because we're trying to close one style tag,
    //but a different one is actually open. Because this is NAV4x
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

  CEndToken aToken(gEmpty);
  eHTMLTags theTag=mBodyContext->Last();
  aToken.SetTypeID(theTag);
  nsCParserNode theNode(&aToken,mLineNumber);
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
  nsresult result=mSink->AddLeaf(aNode); 
  return result;
}

/**
 * Call this method ONLY when you want to write a leaf
 * into the head container.
 * 
 * @update  vidur 11/14/98
 * @param   aNode -- next node to be added to model
 * @return  error code; 0 means OK
 */
nsresult CNavDTD::AddHeadLeaf(const nsIParserNode& aNode){
  nsresult result=NS_OK;

  static eHTMLTags gNoXTags[]={eHTMLTag_noframes,eHTMLTag_nolayer,eHTMLTag_noscript};
  
  //this code has been added to prevent <meta> tags from being processed inside
  //the document if the <meta> tag itself was found in a <noframe>, <nolayer>, or <noscript> tag.
  if(eHTMLTag_meta==aNode.GetNodeType())
    if(HasOpenContainer(gNoXTags,sizeof(gNoXTags)/sizeof(eHTMLTag_unknown))) {
      return result;
    }

  result=OpenHead(aNode);
  if(NS_OK==result) {
    result=AddLeaf(aNode);
    // XXX If the return value tells us to block, go
    // ahead and close the tag out anyway, since its
    // contents will be consumed.
    CloseHead(aNode);
  }
  return result;
}

  
static nsTagStack kPropagationStack;

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
  static CStartToken theToken(gEmpty);
  if(PR_TRUE==bResult){
    while(kPropagationStack.mCount>0) {
      eHTMLTags theTag=kPropagationStack.Pop();
      theToken.SetTypeID(theTag);  //open the container...
      HandleStartToken(&theToken);
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

  if(FindTagInSet(aTag,gStyleTags,sizeof(gStyleTags)/sizeof(eHTMLTag_unknown))) {
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
      if(FindTagInSet(aTag,gStyleTags,sizeof(gStyleTags)/sizeof(eHTMLTag_unknown))) {
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
  nsresult result = NS_OK;
  if(mSink) {
    result = mSink->WillResume();
  }
  return result;
}

/**
 * This method gets called when the parsing process is interrupted
 * due to lack of data (waiting for netlib).
 * @update  gess5/18/98
 * @return  error code
 */
nsresult CNavDTD::WillInterruptParse(void){
  nsresult result = NS_OK;
  if(mSink) {
    result = mSink->WillInterrupt();
  }
  return result;
}


