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

/**
 * MODULE NOTES:
 * @update  gess 4/8/98
 * 
 *         
 */

/**
 * TRANSIENT STYLE-HANDLING NOTES:
 * @update  gess 6/15/98
 * 
 * ...add comments here about transient style stack.
 *         
   */

#include "nsIDTDDebug.h"
#include "CRtfDTD.h"
#include "nsCRT.h"
#include "nsParser.h"
#include "nsScanner.h"
#include "nsParserTypes.h"
#include "nsTokenHandler.h"

#include "prenv.h"  //this is here for debug reasons...
#include "prtypes.h"  //this is here for debug reasons...
#include "prio.h"
#include "plstr.h"

#ifdef XP_PC
#include <direct.h> //this is here for debug reasons...
#endif
#include "prmem.h"


static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);                 
static NS_DEFINE_IID(kIDTDIID,      NS_IDTD_IID);
static NS_DEFINE_IID(kClassIID,     NS_RTF_DTD_IID); 


static const char* kNullURL = "Error: Null URL given";
static const char* kNullFilename= "Error: Null filename given";
static const char* kNullTokenizer = "Error: Unable to construct tokenizer";
static const char* kNullToken = "Error: Null token given";
static const char* kInvalidTagStackPos = "Error: invalid tag stack position";
static const char* kRTFTextContentType = "application/rtf";
static const char* kRTFDocHeader= "{\\rtf0";
static nsString     gAlphaChars("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ");
static nsAutoString gDigits("-0123456789");

static nsAutoString gEmpty;

struct RTFEntry {
  char      mName[10];
  eRTFTags  mTagID;
};


static RTFEntry gRTFTable[] = {

  {"$",eRTFCtrl_unknown},
  {"'",eRTFCtrl_quote},
  {"*",eRTFCtrl_star},   
  {"0x0a",eRTFCtrl_linefeed},
  {"0x0d",eRTFCtrl_return},
  {"\\",eRTFCtrl_begincontrol},
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
 * @update	gess4/25/98
 * @param 
 * @return
 */
const char* GetTagName(eRTFTags aTag) {
  const    char* result=0;
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
  ((nsISupports*) *aInstancePtr)->AddRef();
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
 *  @update  gess 4/9/98
 *  @param   
 *  @return  
 */
CRtfDTD::CRtfDTD() : nsIDTD() {
  NS_INIT_REFCNT();
  mParser=0;
  mFilename=0;
}

/**
 *  Default destructor
 *  
 *  @update  gess 4/9/98
 *  @param   
 *  @return  
 */
CRtfDTD::~CRtfDTD(){
}

/**
 * This method is called to determine if the given DTD can parse
 * a document in a given source-type. 
 * NOTE: Parsing always assumes that the end result will involve
 *       storing the result in the main content model.
 * @update	gess6/24/98
 * @param   
 * @return  TRUE if this DTD can satisfy the request; FALSE otherwise.
 */
PRBool CRtfDTD::CanParse(nsString& aContentType, PRInt32 aVersion){
  PRBool result=aContentType.Equals(kRTFTextContentType);
  return result;
}


/**
 * 
 * @update	gess7/7/98
 * @param 
 * @return
 */
eAutoDetectResult CRtfDTD::AutoDetectContentType(nsString& aBuffer,nsString& aType){
  eAutoDetectResult result=eUnknownDetect;
  if(kNotFound!=aBuffer.Find(kRTFDocHeader))
    if(PR_TRUE==aType.Equals(kRTFTextContentType))
      result=eValidDetect;
  return result;
}

/**
 * 
 * @update	gess5/18/98
 * @param 
 * @return
 */
PRInt32 CRtfDTD::WillBuildModel(nsString& aFilename){
  PRInt32 result=0;
  return result;
}

/**
 * 
 * @update	gess5/18/98
 * @param 
 * @return
 */
PRInt32 CRtfDTD::DidBuildModel(PRInt32 anErrorCode){
  PRInt32 result=0;

  return result;
}

/**
 * 
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return 
 */
void CRtfDTD::SetParser(nsIParser* aParser) {
  mParser=(nsParser*)aParser;
}

/**
 *  This method gets called in order to set the content
 *  sink for this parser to dump nodes to.
 *  
 *  @update  gess 3/25/98
 *  @param   nsIContentSink interface for node receiver
 *  @return  
 */
nsIContentSink* CRtfDTD::SetContentSink(nsIContentSink* aSink) {
  return 0;
}


/*******************************************************************
  These methods use to be hidden in the tokenizer-delegate. 
  That file merged with the DTD, since the separation wasn't really
  buying us anything.
 *******************************************************************/

/**
 *  This gets called when we've just read a '\' char. 
 *  It means we need to read an RTF control word.
 *  
 *  @update gess 3/25/98
 *  @param  aToken ptr-ref to new token we create
 *  @return error code -- preferably kNoError
 */
PRInt32 CRtfDTD::ConsumeControlWord(CToken*& aToken){
  PRInt32 result=kNoError;
  CRTFControlWord* cw=new CRTFControlWord("");
  if(cw){
    CScanner* theScanner=mParser->GetScanner();
    cw->Consume(*theScanner);
    aToken=cw;
  }
  return result;
}

/**
 *  This gets called when we've just read a '{' or '}' group char. 
 *  It means we have just started or ended an RTF group.
 *  
 *  @update gess 3/25/98
 *  @param  aToken ptr-ref to new token we create
 *  @return error code -- preferably kNoError
 */
static char* keys[] = {"}","{"};

PRInt32 CRtfDTD::ConsumeGroupTag(CToken*& aToken,PRBool aStartGroup){
  PRInt32 result=kNoError;
  CRTFGroup* cw=new CRTFGroup(keys[PR_TRUE==aStartGroup],aStartGroup);
  if(cw){
    CScanner* theScanner=mParser->GetScanner();
    cw->Consume(*theScanner);
    aToken=cw;
  }
  return result;
}

/**
 *  This gets called when we've just read plain text char.
 *  It means we need to read RTF content.
 *  
 *  @update gess 3/25/98
 *  @param  aToken ptr-ref to new token we create
 *  @return error code -- preferably kNoError
 */
PRInt32 CRtfDTD::ConsumeContent(PRUnichar aChar,CToken*& aToken){
  PRInt32 result=kNoError;
  PRUnichar buffer[2] = {0,0};
  buffer[0]=aChar;
  CRTFContent * cw=new CRTFContent(buffer);
  if(cw){
    CScanner* theScanner=mParser->GetScanner();
    cw->Consume(*theScanner);
    aToken=cw;
  }
  return result;
}

/**
 *  This method repeatedly called by the tokenizer. 
 *  Each time, we determine the kind of token were about to 
 *  read, and then we call the appropriate method to handle
 *  that token type.
 *  
 *  @update gess 3/25/98
 *  @param  aChar: last char read
 *  @param  aScanner: see nsScanner.h
 *  @param  anErrorCode: arg that will hold error condition
 *  @return new token or null 
 */
PRInt32 CRtfDTD::ConsumeToken(CToken*& aToken){
  
  CScanner* theScanner=mParser->GetScanner();

  PRUnichar aChar;
  PRInt32   result=theScanner->GetChar(aChar);

  switch(result) {
    case kEOF:
      break;

    case kInterrupted:
      theScanner->RewindToMark();
      break; 

    case kNoError:
    default:
      switch(aChar) {
        case kLeftBrace:
          result=ConsumeGroupTag(aToken,PR_TRUE); break;

        case kRightBrace:
          result=ConsumeGroupTag(aToken,PR_FALSE); break;

        case kBackSlash:
          result=ConsumeControlWord(aToken); break;

        case kCR:
        case kSpace:
        case kTab:
        case kLF:
          break;

        default:
          result=ConsumeContent(aChar,aToken); break;
      } //switch
      break; 
  } //switch
  if(kNoError==result)
    result=theScanner->Eof();
  return result;
}

/**
 * 
 * @update	gess5/18/98
 * @param 
 * @return
 */
void CRtfDTD::WillResumeParse(void){
  return;
}

/**
 * 
 * @update	gess5/18/98
 * @param 
 * @return
 */
void CRtfDTD::WillInterruptParse(void){
  return;
}

/**
 * 
 * @update	jevering6/23/98
 * @param 
 * @return
 */
void CRtfDTD::SetDTDDebug(nsIDTDDebug * aDTDDebug) {
}

/**
 *  This method is called to determine whether or not a tag
 *  of one type can contain a tag of another type.
 *  
 *  @update  gess 3/25/98
 *  @param   aParent -- int tag of parent container
 *  @param   aChild -- int tag of child container
 *  @return  PR_TRUE if parent can contain child
 */
PRBool CRtfDTD::CanContain(PRInt32 aParent,PRInt32 aChild){
  PRBool result=PR_FALSE;
  return result;
}

/**
 *  
 *  @update  gess 3/25/98
 *  @param   aToken -- token object to be put into content model
 *  @return  0 if all is well; non-zero is an error
 */
PRInt32 CRtfDTD::HandleGroup(CToken* aToken){
  PRInt32 result=kNoError;
  return result;
}

/**
 *  
 *  @update  gess 3/25/98
 *  @param   aToken -- token object to be put into content model
 *  @return  0 if all is well; non-zero is an error
 */
PRInt32 CRtfDTD::HandleControlWord(CToken* aToken){
  PRInt32 result=kNoError;
  return result;
}

/**
 *  
 *  @update  gess 3/25/98
 *  @param   aToken -- token object to be put into content model
 *  @return  0 if all is well; non-zero is an error
 */
PRInt32 CRtfDTD::HandleContent(CToken* aToken){
  PRInt32 result=kNoError;
  return result;
}

/**
 *  
 *  @update  gess 3/25/98
 *  @param   aToken -- token object to be put into content model
 *  @return  0 if all is well; non-zero is an error
 */
PRInt32 CRtfDTD::HandleToken(CToken* aToken) {
  PRInt32 result=0;

  if(aToken) {
    eRTFTokenTypes theType=eRTFTokenTypes(aToken->GetTokenType());
    
    switch(theType) {
      case eRTFToken_group:
        result=HandleGroup(aToken); break;

      case eRTFToken_controlword:
        result=HandleControlWord(aToken); break;

      case eRTFToken_content:
        result=HandleContent(aToken); break;

      default:
        break;
      //everything else is just text or attributes of controls...
    }

  }//if

  return result;
}



/***************************************************************
  Heres's the RTFControlWord subclass...
 ***************************************************************/

CRTFControlWord::CRTFControlWord(char* aKey) : CToken(aKey), mArgument("") {
}


PRInt32 CRTFControlWord::GetTokenType() {
  return eRTFToken_controlword;
}


PRInt32 CRTFControlWord::Consume(CScanner& aScanner){
  PRInt32 result=aScanner.ReadWhile(mTextValue,gAlphaChars,PR_FALSE);
  if(kNoError==result) {
    //ok, now look for an option parameter...
    PRUnichar ch;
    result=aScanner.Peek(ch);

    switch(ch) {
      case '0': case '1': case '2': case '3': case '4': 
      case '5': case '6': case '7': case '8': case '9': 
      case kMinus:
        result=aScanner.ReadWhile(mArgument,gDigits,PR_FALSE);
        break;

      case kSpace:
      default:
        break;
    }
  }
  if(kNoError==result)
    result=aScanner.SkipWhitespace();
  return result;
}


/***************************************************************
  Heres's the RTFGroup subclass...
 ***************************************************************/

CRTFGroup::CRTFGroup(char* aKey,PRBool aStartGroup) : CToken(aKey) {
  mStart=aStartGroup;
}


PRInt32 CRTFGroup::GetTokenType() {
  return eRTFToken_group;
}

void CRTFGroup::SetGroupStart(PRBool aFlag){
  mStart=aFlag;
}

PRBool CRTFGroup::IsGroupStart(){
  return mStart;
}

PRInt32 CRTFGroup::Consume(CScanner& aScanner){
  PRInt32 result=kNoError;
  if(PR_FALSE==mStart)
    result=aScanner.SkipWhitespace();
  return result;
}


/***************************************************************
  Heres's the RTFContent subclass...
 ***************************************************************/

CRTFContent::CRTFContent(PRUnichar* aKey) : CToken(aKey) {
}


PRInt32 CRTFContent::GetTokenType() {
  return eRTFToken_content;
}



/**
 * We're supposed to read text until we encounter one
 * of the RTF control characters: \.{,}.
 * @update	gess7/9/98
 * @param 
 * @return
 */
static nsString textTerminators("\\{}");
PRInt32 CRTFContent::Consume(CScanner& aScanner){
  PRInt32 result=aScanner.ReadUntil(mTextValue,textTerminators,PR_FALSE);
  return result;
}



