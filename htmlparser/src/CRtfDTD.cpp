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
 
/** 
 * MODULE NOTES:
 * @update  gess 1/31/00
 * 
 *         
 */


#include "nsIDTDDebug.h"
#include "CRtfDTD.h"
#include "nsCRT.h"
#include "nsParser.h"
#include "nsScanner.h"
#include "nsIParser.h"
#include "nsTokenHandler.h"
#include "nsITokenizer.h"
#include "nsIHTMLContentSink.h" 
#include "nsHTMLEntities.h"
#include "nsDTDUtils.h"

#include "prenv.h"  //this is here for debug reasons...
#include "prtypes.h"  //this is here for debug reasons...
#include "prio.h"
#include "plstr.h"
#include "nsDebug.h" 

#include "prmem.h"


static NS_DEFINE_IID(kIHTMLContentSinkIID, NS_IHTML_CONTENT_SINK_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);                 
static NS_DEFINE_IID(kIDTDIID,      NS_IDTD_IID);
static NS_DEFINE_IID(kClassIID,     NS_RTF_DTD_IID); 
static NS_DEFINE_IID(kITokenizerIID,NS_ITOKENIZER_IID);


static const char* kRTFDocHeader= "{\\rtf0";


class nsRTFTokenizer: public nsITokenizer {
public:
          nsRTFTokenizer();
  virtual ~nsRTFTokenizer();

          NS_DECL_ISUPPORTS

  virtual nsresult          WillTokenize(PRBool aIsFinalChunk,nsTokenAllocator* aTokenAllocator);
  virtual nsresult          ConsumeToken(nsScanner& aScanner,PRBool& aFlushTokens);
  virtual nsresult          DidTokenize(PRBool aIsFinalChunk);
  virtual nsTokenAllocator* GetTokenAllocator(void);

  virtual CToken*           PushTokenFront(CToken* theToken);
  virtual CToken*           PushToken(CToken* theToken);
	virtual CToken*           PopToken(void);
	virtual CToken*           PeekToken(void);
	virtual CToken*           GetTokenAt(PRInt32 anIndex);
	virtual PRInt32           GetCount(void);

  virtual void              PrependTokens(nsDeque& aDeque);
protected:

  nsDeque             mTokenDeque;
  nsTokenAllocator*   mTokenAllocator;
};


/**
 * 
 * @update  gess 1/31/00
 * @param 
 * @return
 */
class CRTFGroup {
public:
  CRTFGroup() {     
    nsCRT::zero(mContainers,sizeof(mContainers));
    mColor=0;
  }
  
  PRBool  mContainers[eRTFCtrl_last];
  PRInt32 mColor;
};



/**
 *  This method is defined in nsRTFTokenizer.h. It is used to 
 *  cause the COM-like construction of an RTFTokenizer.
 *  
 * @update  gess 1/31/00
 * @param   nsIParser** ptr to newly instantiated parser
 * @return  NS_xxx error result
 */

nsresult NS_NewRTFTokenizer(nsITokenizer** aInstancePtrResult) {
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsRTFTokenizer* it = new nsRTFTokenizer();
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kClassIID, (void **) aInstancePtrResult);
}



struct RTFEntry {
  char      mName[10];
  eRTFTags  mTagID;
};

#if 1
static RTFEntry gRTFTable[] = {

  {"$",eRTFCtrl_unknown},
  {"'",eRTFCtrl_quote},
  {"*",eRTFCtrl_star},   
  {"0x0a",eRTFCtrl_linefeed},
  {"0x0d",eRTFCtrl_return},
  {"\\",eRTFCtrl_begincontrol},
  {"ansi",eRTFCtrl_ansi}, 
  {"b",eRTFCtrl_bold}, 
  {"bin",eRTFCtrl_bin},
  {"blue",eRTFCtrl_blue},
  {"cols",eRTFCtrl_cols},
  {"comment",eRTFCtrl_comment},
  {"f",eRTFCtrl_font},
  {"fonttbl",eRTFCtrl_fonttable},
  {"green",eRTFCtrl_green},
  {"i",eRTFCtrl_italic},
  {"margb",eRTFCtrl_bottommargin},
  {"margl",eRTFCtrl_leftmargin},
  {"margr",eRTFCtrl_rightmargin},
  {"margt",eRTFCtrl_topmargin},
  {"par",eRTFCtrl_par},
  {"pard",eRTFCtrl_pard},
  {"plain",eRTFCtrl_plain},
  {"qc",eRTFCtrl_justified},
  {"qj",eRTFCtrl_fulljustified},
  {"ql",eRTFCtrl_leftjustified},
  {"qr",eRTFCtrl_rightjustified},
  {"rdblquote",eRTFCtrl_rdoublequote},
  {"red",eRTFCtrl_red},
  {"rtf",eRTFCtrl_rtf},
  {"tab",eRTFCtrl_tab},
  {"title",eRTFCtrl_title},
  {"u",eRTFCtrl_underline},
  {"{",eRTFCtrl_startgroup},
  {"}",eRTFCtrl_endgroup},
  {"~",eRTFCtrl_last} //make sure this stays the last token...
};

/**
 * 
 * @update  gess 1/31/00
 * @param 
 * @return
 */
#if 0
static const char* GetTagName(eRTFTags aTag) {
  PRInt32  cnt=sizeof(gRTFTable)/sizeof(RTFEntry);
  PRInt32  low=0; 
  PRInt32  high=cnt-1;
  PRInt32  middle=kNotFound;
  
  while(low<=high) {
    middle=(PRInt32)(low+high)/2;
    if(aTag==gRTFTable[middle].mTagID)   
      return gRTFTable[middle].mName;
    if(aTag<gRTFTable[middle].mTagID)
      high=middle-1; 
    else low=middle+1; 
  }
  return "";
}
#endif

/**
 * 
 * @update  gess 1/31/00
 * @param 
 * @return
 */
static eRTFTags GetTagID(const nsString& aTag) {
  PRInt32  cnt=sizeof(gRTFTable)/sizeof(RTFEntry);
  PRInt32  low=0; 
  PRInt32  high=cnt-1;
  PRInt32  middle=kNotFound;
  
  while(low<=high) {
    middle=(PRInt32)(low+high)/2;

    PRInt32 cmp=aTag.CompareWithConversion(gRTFTable[middle].mName);
    if(0==cmp)
      return gRTFTable[middle].mTagID;
    if(-1==cmp)
      high=middle-1; 
    else low=middle+1; 
  }
  return eRTFCtrl_unknown;
}

#endif

/**
 *  This method gets called as part of our COM-like interfaces.
 *  Its purpose is to create an interface to parser object
 *  of some type.
 *  
 *  @update  gess 1/31/00
 *  @param    nsIID  id of object to discover
 *  @param    aInstancePtr ptr to newly discovered interface
 *  @return   NS_xxx result code
 */
nsresult CRtfDTD::QueryInterface(const nsIID& aIID, void** aInstancePtr)  
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
    *aInstancePtr = (CRtfDTD*)(this);                                        
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
 * @update  gess 1/31/00
 *  @param   nsIParser** ptr to newly instantiated parser
 *  @return  NS_xxx error result
 */
NS_HTMLPARS nsresult NS_NewRTF_DTD(nsIDTD** aInstancePtrResult)
{
  CRtfDTD* it = new CRtfDTD();

  if (it == 0) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return it->QueryInterface(kClassIID, (void **) aInstancePtrResult);
}


NS_IMPL_ADDREF(CRtfDTD)
NS_IMPL_RELEASE(CRtfDTD)


/**
 *  Default constructor
 *  
 *  @update  gess 1/31/00
 *  @param   
 *  @return  
 */
CRtfDTD::CRtfDTD() : nsIDTD(), mGroups(0) {
  NS_INIT_REFCNT();
  mParser=0;
  mFilename=0;
  mTokenizer=0;
  mHasHeader=PR_FALSE;
  mSink=0;
}

/**
 *  Default destructor
 *  
 * @update  gess 1/31/00
 * @param   
 * @return  
 */
CRtfDTD::~CRtfDTD(){
}

/**
 * 
 * @update  gess 1/31/00
 * @param 
 * @return
 */
const nsIID& CRtfDTD::GetMostDerivedIID(void) const{
  return kClassIID;
}

/**
 * Call this method if you want the DTD to construct fresh instance
 * of itself.
 * @update  gess 1/31/00
 * @param 
 * @return
 */
nsresult CRtfDTD::CreateNewInstance(nsIDTD** aInstancePtrResult){
  return NS_NewRTF_DTD(aInstancePtrResult);
}

/**
 * This method is called to determine if the given DTD can parse
 * a document in a given source-type. 
 * NOTE: Parsing always assumes that the end result will involve
 *       storing the result in the main content model.
 * @update  gess 1/31/00
 * @param   
 * @return  TRUE if this DTD can satisfy the request; FALSE otherwise.
 */
eAutoDetectResult CRtfDTD::CanParse(CParserContext& aParserContext,nsString& aBuffer, PRInt32 aVersion) {
  eAutoDetectResult result=eUnknownDetect;

  if(eViewSource!=aParserContext.mParserCommand) {
    if(aParserContext.mMimeType.EqualsWithConversion(kRTFTextContentType)) {
      result=ePrimaryDetect;
    }
    else if(kNotFound!=aBuffer.Find(kRTFDocHeader)) {
      result=ePrimaryDetect;
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
nsresult CRtfDTD::WillBuildModel(  const CParserContext& aParserContext,nsIContentSink* aSink){

  nsresult result=NS_OK;
  mGroupCount=0;


  if((!aParserContext.mPrevContext) && (aSink)) {

    if(aSink && (!mSink)) {
      result=aSink->QueryInterface(kIHTMLContentSinkIID, (void **)&mSink);
    }

    if(result==NS_OK) {
      result = aSink->WillBuildModel();

    }
  }


  return result;
}

/**
  * The parser uses a code sandwich to wrap the parsing process. Before
  * the process begins, WillBuildModel() is called. Afterwards the parser
  * calls DidBuildModel(). 
  * @update  gess 1/31/00
  * @param	aFilename is the name of the file being parsed.
  * @return	error code (almost always 0)
  */
NS_IMETHODIMP CRtfDTD::BuildModel(nsIParser* aParser,nsITokenizer* aTokenizer,nsITokenObserver* anObserver,nsIContentSink* aSink) {
  nsresult result=NS_OK;

  if(aTokenizer) {
    nsITokenizer*  oldTokenizer=mTokenizer;
    mTokenizer=aTokenizer;
    mParser=(nsParser*)aParser;

    if(mTokenizer) {

      if(aSink) {   

        do {

#if 0
          int n=aTokenizer->GetCount();
          if(n>50) n=50;
          for(int i=0;i<n;i++){
            CToken* theToken=aTokenizer->GetTokenAt(i);
            printf("\nToken[%i],%p",i,theToken);
          }
          printf("\n");
#endif

          CToken* theToken=mTokenizer->PopToken();
          if(theToken) { 
            result=HandleToken(theToken,aParser);
          }
          else break;
        } while(NS_SUCCEEDED(result) && mGroups.GetSize());

        mTokenizer=oldTokenizer;
      }
    }
  }
  else result=NS_ERROR_HTMLPARSER_BADTOKENIZER;
  return result;
}

/**
 * 
 * @update  gess 1/31/00
 * @param 
 * @return
 */
NS_IMETHODIMP CRtfDTD::DidBuildModel(nsresult anErrorCode,PRInt32 aLevel,nsIParser* aParser,nsIContentSink* aSink){
  nsresult result=NS_OK;

  CloseContainer(eHTMLTag_pre);
  CloseContainer(eHTMLTag_body);
  CloseContainer(eHTMLTag_html);

  if(mSink) mSink->DidBuildModel(0);

  return result;
}

/**
 * Retrieve the preferred tokenizer for use by this DTD.
 * @update  gess 1/31/00
 * @param   none
 * @return  ptr to tokenizer
 */
nsresult CRtfDTD::GetTokenizer(nsITokenizer*& aTokenizer) {
  nsresult result=NS_OK;
  if(!mTokenizer) {
    result=NS_NewRTFTokenizer(&mTokenizer);
  }
  aTokenizer=mTokenizer;
  return result;
}

/**
 * 
 * @update  gess 1/31/00
 * @param 
 * @return
 */
nsresult CRtfDTD::WillResumeParse(void){
  return NS_OK;
}

/**
 * 
 * @update  gess 1/31/00
 * @param 
 * @return
 */
nsresult CRtfDTD::WillInterruptParse(void){
  return NS_OK;
}

/**
 * Called by the parser to initiate dtd verification of the
 * internal context stack.
 * @update  gess 1/31/00
 * @param 
 * @return
 */
PRBool CRtfDTD::Verify(nsString& aURLRef,nsIParser* aParser){
  PRBool result=PR_TRUE;
  mParser=(nsParser*)aParser;
  return result;
}

/**
 * Called by the parser to enable/disable dtd verification of the
 * internal context stack.
 * @update  gess 1/31/00
 * @param 
 * @return
 */
void CRtfDTD::SetVerification(PRBool aEnabled){
}

/**
 *  This method gets called to determine whether a given 
 *  tag is itself a container
 *  
 * @update  gess 1/31/00
 * @param   aTag -- tag to test for containership
 * @return  PR_TRUE if given tag can contain other tags
 */
PRBool CRtfDTD::IsContainer(PRInt32 aTag) const{
  PRBool result=PR_FALSE;
  return result;
}


/**
 *  This method is called to determine whether or not a tag
 *  of one type can contain a tag of another type.
 *  
 * @update  gess 1/31/00
 * @param   aParent -- int tag of parent container
 * @param   aChild -- int tag of child container
 * @return  PR_TRUE if parent can contain child
 */
PRBool CRtfDTD::CanContain(PRInt32 aParent,PRInt32 aChild) const{
  PRBool result=PR_FALSE;
  return result;
}

/**
 * Give rest of world access to our tag enums, so that CanContain(), etc,
 * become useful.
 */
NS_IMETHODIMP CRtfDTD::StringTagToIntTag(nsString &aTag, PRInt32* aIntTag) const {
  *aIntTag = nsHTMLTags::LookupTag(aTag);
  return NS_OK;
}

NS_IMETHODIMP CRtfDTD::IntTagToStringTag(PRInt32 aIntTag, nsString& aTag) const {
  aTag.AssignWithConversion(nsHTMLTags::GetStringValue((nsHTMLTag)aIntTag).GetBuffer());
  return NS_OK;
}

NS_IMETHODIMP CRtfDTD::ConvertEntityToUnicode(const nsString& aEntity, PRInt32* aUnicode) const {
  *aUnicode = nsHTMLEntities::EntityToUnicode(aEntity);
  return NS_OK;
}


/**
 *  
 * @update  gess 1/31/00
 * @param   aToken -- token object to be put into content model
 * @return  0 if all is well; non-zero is an error
 */
nsresult CRtfDTD::PushGroup(){
  nsresult result=NS_OK;

  CRTFGroup* theGroup=new CRTFGroup();
  if(theGroup)
    mGroups.Push(theGroup);
  else result=NS_ERROR_OUT_OF_MEMORY;
  return result;
}

/**
 *  
 * @update  gess 1/31/00
 * @param   aToken -- token object to be put into content model
 * @return  0 if all is well; non-zero is an error
 */
nsresult CRtfDTD::PopGroup(){
  nsresult result=NS_OK;

  CRTFGroup* theGroup=(CRTFGroup*)mGroups.Pop();
  if(theGroup)
    delete theGroup;
  return result;
}

/**
 *  
 * @update  gess 1/31/00
 * @param   aTag -- text containing the tag to open
 * @return  0 if all is well; non-zero is an error
 */
nsresult CRtfDTD::AddLeafContainer(eHTMLTags aTag,const char* aTagName){
  nsresult result=NS_OK;

  if(mSink) {
    nsAutoString theStr; theStr.AssignWithConversion(aTagName);
    CStartToken theToken(theStr,aTag);
    nsCParserNode theNode(&theToken);

    switch(aTag) {
      case eHTMLTag_br:
      default:
        result=mSink->AddLeaf(theNode);
        break;
    }
  }
  return result;
}

/**
 *  
 * @update  gess 1/31/00
 * @param   aTag -- text containing the tag to open
 * @return  0 if all is well; non-zero is an error
 */
nsresult CRtfDTD::OpenContainer(eHTMLTags aTag,const char* aTagName){
  nsresult result=NS_OK;

  if(mSink) {
    nsAutoString theStr; theStr.AssignWithConversion(aTagName);
    CStartToken theToken(theStr,aTag);
    nsCParserNode theNode(&theToken);

    switch(aTag) {
      case eHTMLTag_html:
        mSink->OpenHTML(theNode);
        break;
      case eHTMLTag_body:
        mSink->OpenBody(theNode);
        break;
      default:
        mSink->OpenContainer(theNode);
        break;
    }
  }
  return result;
}

/**
 *  
 * @update  gess 1/31/00
 * @param   aTag -- text containing the tag to close
 * @return  0 if all is well; non-zero is an error
 */
nsresult CRtfDTD::CloseContainer(eHTMLTags aTag){
  nsresult result=NS_OK;

  if(mSink) {
    CEndToken theToken(aTag);
    nsCParserNode theNode(&theToken);

    switch(aTag) {
      case eHTMLTag_html:
        mSink->CloseHTML(theNode);
        break;
      case eHTMLTag_body:
        mSink->CloseBody(theNode);
        break;
      case eHTMLTag_p:
        break;
      default:
        mSink->CloseContainer(theNode);
        break;
    }
  }

  return result;
}


/**
 *  
 * @update  gess 1/31/00
 * @param   aToken -- token object to be put into content model
 * @return  0 if all is well; non-zero is an error
 */
nsresult CRtfDTD::EmitStyleContainer(CToken* aToken,eRTFTags aTag,PRBool aState){
  nsresult result=NS_OK;

  const char* theTag=0;
  eHTMLTags   theID=eHTMLTag_unknown;

  switch(aTag) {
    case eRTFCtrl_bold:
      theTag="b"; 
      theID=eHTMLTag_b;
      break;
    case eRTFCtrl_italic:
      theTag="i"; 
      theID=eHTMLTag_i;
      break;
    case eRTFCtrl_underline:
      theTag="u"; 
      theID=eHTMLTag_u;
      break;
    case eRTFCtrl_plain:
      result=EmitStyleContainer(aToken,eRTFCtrl_bold,PR_FALSE);
      result=EmitStyleContainer(aToken,eRTFCtrl_italic,PR_FALSE);
      result=EmitStyleContainer(aToken,eRTFCtrl_underline,PR_FALSE);
      break;
    default:
      break;
  } //switch

  CRTFGroup* theGroup=(CRTFGroup*)mGroups.ObjectAt(mGroups.GetSize()-1);

  if(aState) {
    if(!theGroup->mContainers[aTag]) {
      result=OpenContainer(theID,theTag);
      theGroup->mContainers[aTag]=PR_TRUE;
    }
  }
  else {
    if(theGroup->mContainers[aTag]) {
      result=CloseContainer(theID);
      theGroup->mContainers[aTag]=PR_FALSE;
    }
  }

  return result;
}

/**
 *  
 * @update  gess 1/31/00
 * @param   aToken -- token object to be put into content model
 * @return  0 if all is well; non-zero is an error
 */
nsresult CRtfDTD::HandleControlWord(CToken* aToken){
  nsresult result=NS_OK;

  eRTFTags theTag=(eRTFTags)aToken->GetTypeID();
  
  switch(theTag) {
    case eRTFCtrl_startgroup:
      PushGroup();
      break;
    case eRTFCtrl_endgroup:
      PopGroup();
      break;
    case eRTFCtrl_rtf:
      mHasHeader=PR_TRUE;
      OpenContainer(eHTMLTag_html,"html");
      OpenContainer(eHTMLTag_body,"body");
      OpenContainer(eHTMLTag_pre,"pre");
      break;
    default:
      if(mGroups.GetSize() && mHasHeader) {
        
        CRTFControlWord* theToken=(CRTFControlWord*)aToken;

        switch(theTag) {
          case eRTFCtrl_ansi:
            break;
          case eRTFCtrl_tab:
            {
              CTextToken theToken2( NS_ConvertToString("   ") );
              result=HandleContent(&theToken2);
            }
            break;
          case eRTFCtrl_bold:
          case eRTFCtrl_italic:
          case eRTFCtrl_underline:
          case eRTFCtrl_plain:
            result=EmitStyleContainer(theToken,theTag,theToken->mArgument.CharAt(0)!='0');
            break;
          case eRTFCtrl_par:
            AddLeafContainer(eHTMLTag_br,"br");
            break;
          default:
            break; //just drop it on the floor.
        }
      }
      break;
  }
//  PushGroup();

  return result;
}

/**
 *  
 * @update  gess 1/31/00
 * @param   aToken -- token object to be put into content model
 * @return  0 if all is well; non-zero is an error
 */
nsresult CRtfDTD::HandleContent(CToken* aToken){
  nsresult result=NS_OK;

  if(mSink) {
    CTextToken theToken(aToken->GetStringValueXXX());
    nsCParserNode theNode(&theToken);
    mSink->AddLeaf(theNode);
  }

  return result;
}

/**
 *  
 * @update  gess 1/31/00
 * @param   aToken -- token object to be put into content model
 * @return  0 if all is well; non-zero is an error
 */
nsresult CRtfDTD::HandleToken(CToken* aToken,nsIParser* aParser) {
  nsresult result=NS_OK;

  mParser=(nsParser*)aParser;
  if(aToken) {
    eRTFTokenTypes theType=eRTFTokenTypes(aToken->GetTokenType());
    
    switch(theType) {

      case eRTFToken_controlword:
        result=HandleControlWord(aToken); break;

      case eRTFToken_content:
      case eToken_newline:
      case eHTMLTag_text:
        result=HandleContent(aToken); break;

      default:
        break;
      //everything else is just text or attributes of controls...
    }

  }//if

  return result;
}


/**
 * 
 * @update  gess 1/31/00
 * @param 
 * @return
 */
nsTokenAllocator* CRtfDTD::GetTokenAllocator(void){
  return 0;
}

/**
 * Use this id you want to stop the building content model
 * --------------[ Sets DTD to STOP mode ]----------------
 * It's recommended to use this method in accordance with
 * the parser's terminate() method.
 *
 * @update	harishd 07/22/99
 * @update  gess 1/31/00
 * @param 
 * @return
 */
nsresult  CRtfDTD::Terminate(nsIParser* aParser)
{
  return NS_ERROR_HTMLPARSER_STOPPARSING;
}

/***************************************************************
  Heres's the RTFControlWord subclass...
 ***************************************************************/


CRTFControlWord::CRTFControlWord(eRTFTags aTagID) : CToken(aTagID) {
}

/**
 *  
 * @update  gess 1/31/00
 *  @param   
 *  @return  0 if all is well; non-zero is an error
 */ 
PRInt32 CRTFControlWord::GetTokenType() {
  return eRTFToken_controlword;
}


/**
 *  
 * @update  gess 1/31/00
 * @param   
 * @return  nsresult
 */
nsresult CRTFControlWord::Consume(PRUnichar aChar,nsScanner& aScanner,PRInt32 aMode) {
  const char* gAlphaChars="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
  const char* gDigits="-0123456789";

  //First, decide if it's a control word or a control symbol (1 char)
  PRUnichar theChar=0;
  nsresult  result=aScanner.Peek(theChar);
  
  if(NS_SUCCEEDED(result)) {
    if(('a'<=theChar) && (theChar<='z')) {
      nsAutoString temp; temp.AssignWithConversion(gAlphaChars);
      result=aScanner.ReadWhile(mTextValue,temp,PR_FALSE);
      if(NS_OK==result) {
        //ok, now look for an option parameter...

        mTypeID=GetTagID(mTextValue);
        result=aScanner.Peek(theChar);

        switch(theChar) {
          case '0': case '1': case '2': case '3': case '4': 
          case '5': case '6': case '7': case '8': case '9': 
          case kMinus:
            {
              nsAutoString theDigits; theDigits.AssignWithConversion(gDigits);
              result=aScanner.ReadWhile(mArgument,theDigits,PR_FALSE);
            }
            break;

          case kSpace:
          default:
            break;
        }
      }
      if(NS_OK==result)
        result=aScanner.SkipWhitespace();
    }
    else {
      //it's a control symbol
    }
  }
  return result;
}


/***************************************************************
  Heres's the RTFContent subclass...
 ***************************************************************/


/**
 *  
 * @update  gess 1/31/00
 * @param   
 * @return  nsresult
 */
CRTFContent::CRTFContent(PRUnichar* aKey) : CToken(nsAutoString(aKey)) {
}


/**
 *  
 * @update  gess 1/31/00
 * @param   
 * @return  nsresult
 */
PRInt32 CRTFContent::GetTokenType() {
  return eRTFToken_content;
}



/**
 * We're supposed to read text until we encounter one
 * of the RTF control characters: \.{,}.
 * @update  gess 1/31/00
 * @param 
 * @return
 */

nsresult CRTFContent::Consume(PRUnichar aChar,nsScanner& aScanner,PRInt32 aMode) {
  static const char* textTerminators="\\{}\r\n";
  mTextValue.Append(aChar);
  PRInt32 result=aScanner.ReadUntil(mTextValue,textTerminators,PR_FALSE);
  if(NS_SUCCEEDED(result)) {
    mTypeID=eHTMLTag_text;
  }
  return result;
}

/***************************************************************
  Heres's the nsRTFTokenizer class...
 ***************************************************************/


/**
 *  This method gets called as part of our COM-like interfaces.
 *  Its purpose is to create an interface to parser object
 *  of some type.
 *  
 * @update   gess 4/8/98
 * @param    nsIID  id of object to discover
 * @param    aInstancePtr ptr to newly discovered interface
 * @return   NS_xxx result code
 */
nsresult nsRTFTokenizer::QueryInterface(const nsIID& aIID, void** aInstancePtr)  
{                                                                        
  if (NULL == aInstancePtr) {                                            
    return NS_ERROR_NULL_POINTER;                                        
  }                                                                      

  if(aIID.Equals(kISupportsIID))    {  //do IUnknown...
    *aInstancePtr = (nsISupports*)(this);                                        
  }
  else if(aIID.Equals(kITokenizerIID)) {  //do IParser base class...
    *aInstancePtr = (nsITokenizer*)(this);                                        
  }
  else if(aIID.Equals(kClassIID)) {  //do this class...
    *aInstancePtr = (nsRTFTokenizer*)(this);                                        
  }                 
  else {
    *aInstancePtr=0;
    return NS_NOINTERFACE;
  }
  NS_ADDREF_THIS();
  return NS_OK;                                                        
}

nsRTFTokenizer::nsRTFTokenizer() : mTokenDeque(0) {
  NS_INIT_REFCNT();
}
 
/**
 *  
 * @update  gess 1/31/00
 * @param   
 * @return  nsresult
 */
nsRTFTokenizer::~nsRTFTokenizer() {
}

/**
 *  
 * @update  gess 1/31/00
 * @param   
 * @return  nsresult
 */
nsresult nsRTFTokenizer::WillTokenize(PRBool aIsFinalChunk,nsTokenAllocator* aTokenAllocator) {
  nsresult result=NS_OK;
  mTokenAllocator=aTokenAllocator;
  return result;
}


/**
 *  
 * @update  gess 1/31/00
 * @param   
 * @return  nsresult
 */
nsTokenAllocator* nsRTFTokenizer::GetTokenAllocator(void) {
    //let's move to this once we eliminate the leaking of tokens...
  return mTokenAllocator;
}

/**
 *  
 * @update  gess 1/31/00
 * @param   
 * @return  nsresult
 */
nsresult nsRTFTokenizer::ConsumeToken(nsScanner& aScanner,PRBool& aFlushTokens) {
  nsresult result=NS_OK;

  PRUnichar theChar=0;
  result=aScanner.GetChar(theChar);
  if(NS_SUCCEEDED(result)) {
    switch(theChar) {
      case '}':
      case '{':
        {
          eRTFTags theTag= ('{'==theChar) ? eRTFCtrl_startgroup : eRTFCtrl_endgroup;
          CRTFControlWord* theToken=(CRTFControlWord*)mTokenAllocator->CreateRTFTokenOfType(eRTFToken_controlword,theTag);

          if(theToken) {
            mTokenDeque.Push(theToken);
          }
          else result=NS_ERROR_OUT_OF_MEMORY;
        }
        break;
      case '\\':
        {
          CRTFControlWord* theWord =(CRTFControlWord*)mTokenAllocator->CreateRTFTokenOfType(eRTFToken_controlword,eRTFCtrl_unknown);
          if(theWord) {
            result=theWord->Consume(theChar,aScanner,0);
            if(NS_SUCCEEDED(result)) {
              mTokenDeque.Push(theWord);
            }
          }
        }
        break;
      case '\n':
      case '\r':
        {
          CNewlineToken* theContent=(CNewlineToken*)mTokenAllocator->CreateTokenOfType(eToken_newline,eHTMLTag_newline);
          if(theContent) {
            result=theContent->Consume(theChar,aScanner,0);
            if(NS_SUCCEEDED(result)) {
              mTokenDeque.Push(theContent);
            }
          }
        }
        break;
      default:
        CRTFContent* theContent=(CRTFContent*)mTokenAllocator->CreateRTFTokenOfType(eRTFToken_content,(eRTFTags)0);
        if(theContent) {
          result=theContent->Consume(theChar,aScanner,0);
         if(NS_SUCCEEDED(result)) {
           mTokenDeque.Push(theContent);
         }
        }
        break;
    }
  }
  return result;
}


/**
 *  
 * @update  gess 1/31/00
 * @param   
 * @return  nsresult
 */
nsresult nsRTFTokenizer::DidTokenize(PRBool aIsFinalChunk) {
  nsresult result=NS_OK;
  return result;
}


/**
 * This method provides access to the topmost token in the tokenDeque.
 * The token is not really removed from the list.
 * @update  gess 1/31/00
 * @return  ptr to token
 */
CToken* nsRTFTokenizer::PeekToken() {
  return (CToken*)mTokenDeque.PeekFront();
}


/**
 * This method provides access to the topmost token in the tokenDeque.
 * The token is really removed from the list; if the list is empty we return 0.
 * @update  gess 1/31/00
 * @return  ptr to token or NULL
 */
CToken* nsRTFTokenizer::PopToken() {
  CToken* result=nsnull;
  result=(CToken*)mTokenDeque.PopFront();
  return result;
}


/**
 * 
 * @update  gess 1/31/00
 * @param 
 * @return
 */
CToken* nsRTFTokenizer::PushTokenFront(CToken* theToken) {
  mTokenDeque.PushFront(theToken);
	return theToken;
}

/**
 * 
 * @update  gess 1/31/00
 * @param 
 * @return
 */
CToken* nsRTFTokenizer::PushToken(CToken* theToken) {
  mTokenDeque.Push(theToken);
	return theToken;
}

/**
 * 
 * @update  gess 1/31/00
 * @param 
 * @return
 */
PRInt32 nsRTFTokenizer::GetCount(void) {
  return mTokenDeque.GetSize();
}

/**
 * 
 * @update  gess 1/31/00
 * @param 
 * @return
 */
CToken* nsRTFTokenizer::GetTokenAt(PRInt32 anIndex){
  return (CToken*)mTokenDeque.ObjectAt(anIndex);
}


/**
 *  
 * @update  gess 1/31/00
 *  @param   
 *  @return  nsresult
 */
void nsRTFTokenizer::PrependTokens(nsDeque& aDeque) {
}


NS_IMPL_ADDREF(nsRTFTokenizer)
NS_IMPL_RELEASE(nsRTFTokenizer)


