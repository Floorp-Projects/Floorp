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


#include "nsIDTDDebug.h"
#include "nsViewSourceHTML.h"
#include "nsCRT.h"
#include "nsParser.h"
#include "nsScanner.h"
#include "nsIParser.h"
#include "nsTokenHandler.h"
#include "nsDTDUtils.h"
#include "nsIContentSink.h"
#include "nsIHTMLContentSink.h"
#include "nsHTMLTokenizer.h"

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
static NS_DEFINE_IID(kClassIID,     NS_VIEWSOURCE_HTML_IID); 



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
nsresult CViewSourceHTML::QueryInterface(const nsIID& aIID, void** aInstancePtr)  
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
    *aInstancePtr = (CViewSourceHTML*)(this);                                        
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
NS_HTMLPARS nsresult NS_NewViewSourceHTML(nsIDTD** aInstancePtrResult)
{
  CViewSourceHTML* it = new CViewSourceHTML();

  if (it == 0) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return it->QueryInterface(kClassIID, (void **) aInstancePtrResult);
}


NS_IMPL_ADDREF(CViewSourceHTML)
NS_IMPL_RELEASE(CViewSourceHTML)



static
void SetFont(const char* aFace,const char* aSize,PRBool aEnable,nsIContentSink& aSink) {
  if(aEnable){
    CStartToken theFont(eHTMLTag_font);  
    nsCParserNode theFontNode(&theFont,0);
    CAttributeToken theFontAttr("face",aFace);
    theFontNode.AddAttribute(&theFontAttr);
    CAttributeToken theSizeAttr("size",aSize);
    theFontNode.AddAttribute(&theSizeAttr);
    aSink.OpenContainer(theFontNode);
  }
  else {
    CEndToken theFont(eHTMLTag_font);  
    nsCParserNode theFontNode(&theFont,0);
    aSink.CloseContainer(theFontNode);
  }
}

static
void SetColor(const char* aColor,PRBool aEnable,nsIContentSink& aSink) {
  if(aEnable){
    CStartToken theFont(eHTMLTag_font);  
    nsCParserNode theFontNode(&theFont,0);
    CAttributeToken theFontAttr("color",aColor);
    theFontNode.AddAttribute(&theFontAttr);
    aSink.OpenContainer(theFontNode);
  }
  else {
    CEndToken theFont(eHTMLTag_font);  
    nsCParserNode theFontNode(&theFont,0);
    aSink.CloseContainer(theFontNode);
  }
}

static
void SetStyle(eHTMLTags theTag,PRBool aEnable,nsIContentSink& aSink) {
  if(aEnable){
    CStartToken theToken(theTag);  
    nsCParserNode theNode(&theToken,0);
    aSink.OpenContainer(theNode);
  }
  else {
    CEndToken theToken(theTag);  
    nsCParserNode theNode(&theToken,0);
    aSink.CloseContainer(theNode);
  }
}


/**
 *  Default constructor
 *  
 *  @update  gess 4/9/98
 *  @param   
 *  @return  
 */
CViewSourceHTML::CViewSourceHTML() : nsIDTD() {
  NS_INIT_REFCNT();
  mParser=0;
  mSink=0;
  mFilename;
  mLineNumber=0;
  mTokenizer=0;
  mIsHTML=PR_FALSE;
  mHasOpenHead=0;
}

/**
 *  Default destructor
 *  
 *  @update  gess 4/9/98
 *  @param   
 *  @return  
 */
CViewSourceHTML::~CViewSourceHTML(){
  mParser=0; //just to prove we destructed...
}

/**
 * 
 * @update	gess1/8/99
 * @param 
 * @return
 */
const nsIID& CViewSourceHTML::GetMostDerivedIID(void) const{
  return kClassIID;
}

/**
 * Call this method if you want the DTD to construct a fresh 
 * instance of itself. 
 * @update	gess7/23/98
 * @param 
 * @return
 */
nsresult CViewSourceHTML::CreateNewInstance(nsIDTD** aInstancePtrResult){
  return NS_NewViewSourceHTML(aInstancePtrResult);
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
eAutoDetectResult CViewSourceHTML::CanParse(nsString& aContentType, nsString& aCommand, nsString& aBuffer, PRInt32 aVersion) {
  eAutoDetectResult result=eUnknownDetect;

  if(PR_TRUE==aContentType.Equals(kPlainTextContentType)) {
    result=eValidDetect;
  }
  else if(aCommand.Equals(kViewSourceCommand)) {
    if(aContentType.Equals(kXMLTextContentType) ||
       aContentType.Equals(kRDFTextContentType) ||
       aContentType.Equals(kHTMLTextContentType) ||
       aContentType.Equals(kPlainTextContentType) ||
       aContentType.Equals(kXULTextContentType)) {
      result=eValidDetect;
    }
  }
  return result;
}


/**
 * 
 * @update	gess5/18/98
 * @param 
 * @return
 */
NS_IMETHODIMP CViewSourceHTML::WillBuildModel(nsString& aFilename,PRBool aNotifySink,nsString& aSourceType,nsIContentSink* aSink){
  nsresult result=NS_OK;
  mFilename=aFilename;

  mSink=(nsIHTMLContentSink*)aSink;
  if((aNotifySink) && (mSink)) {
    mLineNumber=0;
    result = mSink->WillBuildModel();

    mIsHTML=aSourceType.Equals(kHTMLTextContentType);
    mIsPlaintext=aSourceType.Equals(kPlainTextContentType);

    //now let's automatically open the html...
    CStartToken theHTMLToken(eHTMLTag_html);
    nsCParserNode theHTMLNode(&theHTMLToken,0);
    mSink->OpenHTML(theHTMLNode);

      //now let's automatically open the body...
    CStartToken theBodyToken(eHTMLTag_body);
    nsCParserNode theBodyNode(&theBodyToken,0);
    mSink->OpenBody(theBodyNode);

     //now let's automatically open the body...
    if(mIsPlaintext) {
      CStartToken thePREToken(eHTMLTag_pre);
      nsCParserNode thePRENode(&thePREToken,0);
      mSink->OpenContainer(thePRENode);
    }
    else SetFont("courier","-1",PR_TRUE,*mSink);

  }
  return result;
}

/**
  * The parser uses a code sandwich to wrap the parsing process. Before
  * the process begins, WillBuildModel() is called. Afterwards the parser
  * calls DidBuildModel(). 
  * @update	gess5/18/98
  * @param	aFilename is the name of the file being parsed.
  * @return	error code (almost always 0)
  */
NS_IMETHODIMP CViewSourceHTML::BuildModel(nsIParser* aParser,nsITokenizer* aTokenizer,nsITokenObserver* anObserver,nsIContentSink* aSink) {
  nsresult result=NS_OK;

  if(aTokenizer) {
    nsITokenizer*  oldTokenizer=mTokenizer;
    mTokenizer=aTokenizer;
    nsITokenRecycler* theRecycler=mTokenizer->GetTokenRecycler();

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
 * @update	gess5/18/98
 * @param 
 * @return
 */
NS_IMETHODIMP CViewSourceHTML::DidBuildModel(nsresult anErrorCode,PRBool aNotifySink,nsIParser* aParser,nsIContentSink* aSink){
  nsresult result= NS_OK;

  //ADD CODE HERE TO CLOSE OPEN CONTAINERS...

  if(aParser){
    mSink=(nsIHTMLContentSink*)aParser->GetContentSink();
    if((aNotifySink) && (mSink)) {
        //now let's automatically close the pre...

      if(!mIsPlaintext){
        SetStyle(eHTMLTag_font,PR_FALSE,*mSink);
      }
  
      //now let's automatically close the body...
      CEndToken theBodyToken(eHTMLTag_body);
      nsCParserNode theBodyNode(&theBodyToken,0);
      mSink->CloseBody(theBodyNode);

       //now let's automatically close the html...
      CEndToken theHTMLToken(eHTMLTag_html);
      nsCParserNode theHTMLNode(&theBodyToken,0);
      mSink->CloseHTML(theBodyNode);
      result = mSink->DidBuildModel(1);
    }
  }
  return result;
}

/**
 * 
 * @update	gess8/4/98
 * @param 
 * @return
 */
nsITokenRecycler* CViewSourceHTML::GetTokenRecycler(void){
  nsITokenizer* theTokenizer=GetTokenizer();
  return theTokenizer->GetTokenRecycler();
}

/**
 * Retrieve the preferred tokenizer for use by this DTD.
 * @update	gess12/28/98
 * @param   none
 * @return  ptr to tokenizer
 */
nsITokenizer* CViewSourceHTML::GetTokenizer(void) {
  if(!mTokenizer)
    mTokenizer=new nsHTMLTokenizer();
  return mTokenizer;
}

/**
 * 
 * @update	gess5/18/98
 * @param 
 * @return
 */
NS_IMETHODIMP CViewSourceHTML::WillResumeParse(void){
  nsresult result = NS_OK;
  if(mSink) {
    result = mSink->WillResume();
  }
  return result;
}

/**
 * 
 * @update	gess5/18/98
 * @param 
 * @return
 */
NS_IMETHODIMP CViewSourceHTML::WillInterruptParse(void){
  nsresult result = NS_OK;
  if(mSink) {
    result = mSink->WillInterrupt();
  }
  return result;
}

/**
 * Called by the parser to initiate dtd verification of the
 * internal context stack.
 * @update	gess 7/23/98
 * @param 
 * @return
 */
PRBool CViewSourceHTML::Verify(nsString& aURLRef,nsIParser* aParser) {
  PRBool result=PR_TRUE;
  mParser=(nsParser*)aParser;
  return result;
}

/**
 * Called by the parser to enable/disable dtd verification of the
 * internal context stack.
 * @update	gess 7/23/98
 * @param 
 * @return
 */
void CViewSourceHTML::SetVerification(PRBool aEnabled){
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
PRBool CViewSourceHTML::CanContain(PRInt32 aParent,PRInt32 aChild) const{
  PRBool result=PR_TRUE;
  return result;
}

/**
 *  This method gets called to determine whether a given 
 *  tag is itself a container
 *  
 *  @update  gess 3/25/98
 *  @param   aTag -- tag to test for containership
 *  @return  PR_TRUE if given tag can contain other tags
 */
PRBool CViewSourceHTML::IsContainer(PRInt32 aTag) const{
  PRBool result=PR_TRUE;
  return result;
}


/**
 *  This method gets called when a tag needs to be sent out
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  result status
 */
static
nsresult WriteNewline(nsIContentSink& aSink) {
  nsresult result=NS_OK;
  CStartToken theBRToken(eHTMLTag_br);
  nsCParserNode theBRNode(&theBRToken,0);
  result=aSink.AddLeaf(theBRNode); 
  return NS_OK;
}


/**
 *  This method gets called when a tag needs to be sent out
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  result status
 */
static
nsresult WriteNBSP(PRInt32 aCount, nsIContentSink& aSink) {
  nsresult result=NS_OK;

  CEntityToken theEntity("nbsp");
  nsCParserNode theEntityNode(&theEntity,0);
  int theIndex;
  for(theIndex=0;theIndex<aCount;theIndex++)
    result=aSink.AddLeaf(theEntityNode); 
  return NS_OK;
}

/**
 *  This method gets called when a tag needs to be sent out
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  result status
 */
nsresult CViewSourceHTML::WriteText(const nsString& aTextString,nsIContentSink& aSink,PRBool aPreserveSpace) {
  nsresult result=NS_OK;

  CTextToken  theTextToken;
  nsCParserNode theTextNode((CToken*)&theTextToken,0);
  nsString&   temp=theTextToken.GetStringValueXXX();

  PRInt32 theEnd=aTextString.Length();
  PRInt32 theOffset=0;

  while(theOffset<theEnd){
    switch(aTextString[theOffset]){
      case kCR: break;
      case kLF:
        {
          if(temp.Length())
            result=aSink.AddLeaf(theTextNode); //just dump the whole string...
          WriteNewline(aSink);
        }
        temp="";
        break;
      case kSpace:
        if((PR_TRUE==aPreserveSpace) && (kSpace==aTextString[theOffset+1])) {
          if(temp.Length())
            result=aSink.AddLeaf(theTextNode); //just dump the whole string...
          WriteNBSP(1,aSink);
          temp="";
          break;
        }
        //fall through...
      default:
          //scan ahead looking for valid chars...
        temp+=aTextString[theOffset];
        break;
    }//switch...
    theOffset++;
  }
  if(temp.Length())
    result=aSink.AddLeaf(theTextNode); //just dump the whole string...

  return result;
}

/**
 *  This method gets called when a tag needs to be sent out
 *  
 *  @update  gess 3/25/98
 *  @param   
 *  @return  result status
 */
static
PRBool WriteTag(nsCParserNode& aNode,nsIContentSink& aSink,PRBool anEndToken,PRBool aIsHTML,PRBool aIsPlaintext) {
  static nsString     theString;
  static nsAutoString theLTEntity("lt");
  static nsAutoString theGTEntity("gt");
  static const char*  theColors[][2]={{"purple","purple"},{"purple","red"}};

  PRBool result=PR_TRUE;

  CEntityToken theStartEntityToken(theLTEntity);  
  nsCParserNode theStartNode(&theStartEntityToken,aNode.GetSourceLineNumber());
  aSink.AddLeaf(theStartNode);

  if(!aIsPlaintext) {
    SetStyle(eHTMLTag_b,PR_TRUE,aSink);
    SetColor(theColors[aIsHTML][eHTMLTag_userdefined==aNode.GetNodeType()],PR_TRUE,aSink);
  }

  if(anEndToken)
    theString="/";
  else theString="";
  theString.Append(aNode.GetText());
  {
    CTextToken theToken(theString);  
    nsCParserNode theNode(&theToken,aNode.GetSourceLineNumber());
    aSink.AddLeaf(theNode);
  }

  if(!aIsPlaintext){
    SetStyle(eHTMLTag_font,PR_FALSE,aSink);
    SetStyle(eHTMLTag_b,PR_FALSE,aSink);
  }

  PRInt32 theCount=aNode.GetAttributeCount();
  if(0<theCount){
    PRInt32 theIndex=0;
    for(theIndex=0;theIndex<theCount;theIndex++){
      
       //begin by writing the key...
      {
        if(!aIsPlaintext){
          SetStyle(eHTMLTag_b,PR_TRUE,aSink);
        }
        theString=" ";
        theString.Append(aNode.GetKeyAt(theIndex));
        CTextToken theToken(theString);  
        nsCParserNode theNode(&theToken,aNode.GetSourceLineNumber());
        aSink.AddLeaf(theNode);
        if(!aIsPlaintext){
          SetStyle(eHTMLTag_b,PR_FALSE,aSink);
        }
      }

       //begin by writing the value...
      {
        if(!aIsPlaintext){
          SetColor("blue",PR_TRUE,aSink);
        }
        theString=aNode.GetValueAt(theIndex);
        if(0<theString.Length()){
          theString.Insert('=',0);
          CTextToken theToken(theString);  
          nsCParserNode theNode(&theToken,aNode.GetSourceLineNumber());
          aSink.AddLeaf(theNode);        
        }
        if(!aIsPlaintext){
          SetStyle(eHTMLTag_font,PR_FALSE,aSink);
        }
      }
    }
  }

  CEntityToken theEndEntityToken(theGTEntity);  
  nsCParserNode theEndNode(&theEndEntityToken,aNode.GetSourceLineNumber());
  aSink.AddLeaf(theEndNode);

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
nsresult CViewSourceHTML::OpenHead(const nsIParserNode& aNode){
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
nsresult CViewSourceHTML::CloseHead(const nsIParserNode& aNode){
  if(mHasOpenHead) {
    if(0==--mHasOpenHead){
      nsresult result=(mSink) ? mSink->CloseHead(aNode) : NS_OK; 
    }
  }
  return NS_OK;
}
/**
 *  
 *  @update  gess 3/25/98
 *  @param   aToken -- token object to be put into content model
 *  @return  0 if all is well; non-zero is an error
 */
NS_IMETHODIMP CViewSourceHTML::HandleToken(CToken* aToken,nsIParser* aParser) {
  nsresult        result=NS_OK;
  CHTMLToken*     theToken= (CHTMLToken*)(aToken);
  eHTMLTokenTypes theType= (eHTMLTokenTypes)theToken->GetTokenType();
  eHTMLTags  theTag = (eHTMLTags)aToken->GetTypeID();

  PRBool          theEndTag=PR_TRUE;

  mParser=(nsParser*)aParser;
  mSink=(nsIHTMLContentSink*)aParser->GetContentSink();
  nsCParserNode theNode(theToken,mLineNumber);
 
  switch(theType) {

    case eToken_newline:
      mLineNumber++; //now fall through
      WriteNewline(*mSink);
      break;

    case eToken_whitespace:
      {
        PRInt32 theLength=aToken->GetStringValueXXX().Length();
        WriteNBSP(theLength,*mSink);
      }
      break;

    case eToken_entity:
      {
        if(!mIsPlaintext){
          SetColor("maroon",PR_TRUE,*mSink);
        }
        nsAutoString theStr("&");
        nsString& theEntity=aToken->GetStringValueXXX();
        if(!theEntity.EqualsIgnoreCase("XI",2)) {
          PRUnichar theChar=theEntity.CharAt(0);
          if((nsString::IsDigit(theChar)) || ('X'==theChar) || ('x'==theChar)){
            theStr.Append("#");
          }
        }
        theStr.Append(aToken->GetStringValueXXX());
        theStr.Append(";");
        WriteText(theStr,*mSink,PR_FALSE);
        if(!mIsPlaintext){
          SetStyle(eHTMLTag_font,PR_FALSE,*mSink);
        }
      }
      break;

    case eToken_comment:
      {
        if(!mIsPlaintext){
          SetColor("green",PR_TRUE,*mSink);
          SetStyle(eHTMLTag_i,PR_TRUE,*mSink);
        }
        nsString& theText=aToken->GetStringValueXXX();
        WriteText(theText,*mSink,PR_TRUE);
        if(!mIsPlaintext){
          SetStyle(eHTMLTag_i,PR_FALSE,*mSink);
          SetStyle(eHTMLTag_font,PR_FALSE,*mSink);
        }
      }
      break;

    case eToken_style:
    case eToken_skippedcontent:
      {
        CAttributeToken* theToken=(CAttributeToken*)aToken;
        nsString& theText=theToken->GetKey();
        WriteText(theText,*mSink,PR_FALSE);
      }
      break;

    case eToken_text:
      {
        nsString& theText=aToken->GetStringValueXXX();
        WriteText(theText,*mSink,PR_FALSE);
      }
      break;

    case eToken_instruction:
      {
        if(!mIsPlaintext){
          SetColor("orange",PR_TRUE,*mSink);
          SetStyle(eHTMLTag_i,PR_TRUE,*mSink);
        }
        CTextToken theTextToken(aToken->GetStringValueXXX());
        nsCParserNode theTextNode(&theTextToken,mLineNumber);
        result=mSink->AddLeaf(theTextNode); 
        if(!mIsPlaintext){
          SetStyle(eHTMLTag_i,PR_FALSE,*mSink);
          SetStyle(eHTMLTag_font,PR_FALSE,*mSink);
        }
      }
      break;
    
    case eToken_start:
      {

        PRInt16 attrCount=aToken->GetAttributeCount();
        theEndTag=PR_FALSE;
        if(0<attrCount){ //go collect the attributes...
          int attr=0;
          for(attr=0;attr<attrCount;attr++){
            CToken* theToken=mTokenizer->PeekToken();
            if(theToken)  {
              eHTMLTokenTypes theType=eHTMLTokenTypes(theToken->GetTokenType());
              if(eToken_attribute==theType){
                mTokenizer->PopToken(); //pop it for real...
                theNode.AddAttribute(theToken);
              } 
            }
            else return kEOF;
          }
        }
      }
      
      WriteTag(theNode,*mSink,theEndTag,mIsHTML,mIsPlaintext);
      
      // We make sure to display the title on the view source window.
      
      if(eHTMLTag_title == theTag){
        nsCParserNode attrNode(theToken,mLineNumber,GetTokenRecycler());
        CToken* theNextToken = mTokenizer->PopToken();
        if(theNextToken) {
          theType=eHTMLTokenTypes(theNextToken->GetTokenType());
          if(eToken_skippedcontent==theType) {
            attrNode.SetSkippedContent(theNextToken);
          } 
        }
        result= OpenHead(attrNode);
        if(NS_OK==result) {
          if(mSink) {
            mSink->SetTitle(attrNode.GetSkippedContent());
          }
          if(NS_OK==result)
            result=CloseHead(attrNode);
        }
        nsString& theText=((CAttributeToken*)theNextToken)->GetKey();
        WriteText(theText,*mSink,PR_FALSE);
      }
      break;
    case eToken_end:
      WriteTag(theNode,*mSink,theEndTag,mIsHTML,mIsPlaintext);
      break;
    default:
      result=NS_OK;
  }//switch
  return result;
}

/**
 *  This method causes all tokens to be dispatched to the given tag handler.
 *
 *  @update  gess 3/25/98
 *  @param   aHandler -- object to receive subsequent tokens...
 *  @return	 error code (usually 0)
 */
nsresult CViewSourceHTML::CaptureTokenPump(nsITagHandler* aHandler) {
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
nsresult CViewSourceHTML::ReleaseTokenPump(nsITagHandler* aHandler){
  nsresult result=NS_OK;
  return result;
}
