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
#include "nsExpatDTD.h"
#include "nsCRT.h"
#include "nsParser.h"
#include "nsScanner.h"
#include "nsIParser.h"
#include "nsTokenHandler.h"
#include "nsDTDUtils.h"
#include "nsIContentSink.h"
#include "nsIHTMLContentSink.h"
#include "nsExpatTokenizer.h"

#include "prenv.h"  //this is here for debug reasons...
#include "prtypes.h"  //this is here for debug reasons...
#include "prio.h"
#include "plstr.h"
#include "prlog.h"

#ifdef XP_PC
#include <direct.h> //this is here for debug reasons...
#endif
#include "prmem.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);                 
static NS_DEFINE_IID(kIDTDIID,      NS_IDTD_IID);
static NS_DEFINE_IID(kClassIID,     NS_EXPAT_DTD_IID); 

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
nsresult nsExpatDTD::QueryInterface(const nsIID& aIID, void** aInstancePtr)  
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
    *aInstancePtr = (nsExpatDTD*)(this);                                        
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
NS_HTMLPARS nsresult NS_New_Expat_DTD(nsIDTD** aInstancePtrResult) {
  nsExpatDTD* it = new nsExpatDTD();
  if (it == 0) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kClassIID, (void **) aInstancePtrResult);
}


NS_IMPL_ADDREF(nsExpatDTD)
NS_IMPL_RELEASE(nsExpatDTD)


/**
 *  Default constructor
 *  
 *  @update  gess 4/9/98
 *  @param   
 *  @return  
 */
nsExpatDTD::nsExpatDTD() : nsIDTD() {
  NS_INIT_REFCNT();
    
  mExpatParser=0;
  mParser=0;
  mSink=0;
  mFilename;
  mLineNumber=0;
  mTokenizer=0;
}

/**
 *  Default destructor
 *  
 *  @update  gess 4/9/98
 *  @param   
 *  @return  
 */
nsExpatDTD::~nsExpatDTD(){
  mParser=0; //just to prove we destructed...
  if(mTokenizer)
    delete (nsExpatTokenizer*)mTokenizer;
  if (mExpatParser)
    XML_ParserFree(mExpatParser);
}

/**
 * 
 * @update	gess1/8/99
 * @param 
 * @return
 */
const nsIID& nsExpatDTD::GetMostDerivedIID(void) const{
  return kClassIID;
}

/**
 * Call this method if you want the DTD to construct a fresh 
 * instance of itself. 
 * @update	gess7/23/98
 * @param 
 * @return
 */
nsresult nsExpatDTD::CreateNewInstance(nsIDTD** aInstancePtrResult){
  return NS_New_Expat_DTD(aInstancePtrResult);
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
eAutoDetectResult nsExpatDTD::CanParse(nsString& aContentType, nsString& aCommand, nsString& aBuffer, PRInt32 aVersion) {
  eAutoDetectResult result=eUnknownDetect;

  if(!aCommand.Equals(kViewSourceCommand)) {
    if(aContentType.Equals(kXMLTextContentType) ||
       aContentType.Equals(kRDFTextContentType) ||
       aContentType.Equals(kXULTextContentType)) {
      result=eValidDetect;
    }
    else {
      if(-1<aBuffer.Find("<?xml ")) {
        if(0==aContentType.Length()) {
          aContentType = kXMLTextContentType; //only reset it if it's empty
        }
        result=eValidDetect;
      }
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
NS_IMETHODIMP nsExpatDTD::WillBuildModel(nsString& aFilename,PRBool aNotifySink,nsString& aString,nsIContentSink* aSink){
  nsresult result=NS_OK;
  mFilename=aFilename;

  mSink=aSink;
  if((aNotifySink) && (mSink)) {
    mLineNumber=0;
    result = mSink->WillBuildModel();
  }

  return result;
}

/**
  * The parser uses a code sandwich to wrap the parsing process. Before
  * the process begins, WillBuildModel() is called. Afterwards the parser
  * calls DidBuildModel(). 
  * @update	gess  1/4/99
  * @param	aFilename is the name of the file being parsed.
  * @return	error code (almost always 0)
  */
NS_IMETHODIMP nsExpatDTD::BuildModel(nsIParser* aParser,nsITokenizer* aTokenizer,nsITokenObserver* anObserver,nsIContentSink* aSink) {
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
 * @update	gess5/18/98
 * @param 
 * @return
 */
NS_IMETHODIMP nsExpatDTD::DidBuildModel(nsresult anErrorCode,PRBool aNotifySink,nsIParser* aParser,nsIContentSink* aSink){
  nsresult result= NS_OK;

  //ADD CODE HERE TO CLOSE OPEN CONTAINERS...

  if(aParser){
    mSink=aParser->GetContentSink();
    if((aNotifySink) && (mSink)) {
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
nsITokenRecycler* nsExpatDTD::GetTokenRecycler(void){
  nsITokenizer* theTokenizer=GetTokenizer();
  return theTokenizer->GetTokenRecycler();
}


/**
 * Sets up the callbacks for the expat parser      
 * @update  nra 2/24/99
 * @param   none
 * @return  none
 */
void nsExpatDTD::SetupExpatCallbacks(void) {
  if (mExpatParser) {
    XML_SetElementHandler(mExpatParser, HandleStartElement, HandleEndElement);    
    XML_SetCharacterDataHandler(mExpatParser, HandleCharacterData);
    XML_SetProcessingInstructionHandler(mExpatParser, HandleProcessingInstruction);
    // XML_SetDefaultHandler(mExpatParser, NULL);
    // XML_SetUnparsedEntityDeclHandler(mExpatParser, NULL);
    XML_SetNotationDeclHandler(mExpatParser, HandleNotationDecl);
    // XML_SetExternalEntityRefHandler(mExpatParser, NULL);
    // XML_SetUnknownEncodingHandler(mExpatParser, NULL, NULL);    
  }
}

/**
 * Retrieve the preferred tokenizer for use by this DTD.
 * @update	gess12/28/98
 * @param   none
 * @return  ptr to tokenizer
 */
nsITokenizer* nsExpatDTD::GetTokenizer(void) {
  if(!mTokenizer) {
    mTokenizer=new nsExpatTokenizer();
    mExpatParser = XML_ParserCreate(NULL);
    if (mExpatParser) {
      SetupExpatCallbacks();
    }
  }
  return mTokenizer;
}


/**
 * 
 * @update	gess5/18/98
 * @param 
 * @return
 */
NS_IMETHODIMP nsExpatDTD::WillResumeParse(void){
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
NS_IMETHODIMP nsExpatDTD::WillInterruptParse(void){
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
PRBool nsExpatDTD::Verify(nsString& aURLRef,nsIParser* aParser) {
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
void nsExpatDTD::SetVerification(PRBool aEnabled){
}

/**
 *  
 *  
 *  @update  gess 4/01/99
 *  @param   aTokenizer 
 *  @return  
 */
void nsExpatDTD::EmitMisplacedContent(nsITokenizer* aTokenizer){
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
PRBool nsExpatDTD::CanContain(PRInt32 aParent,PRInt32 aChild) const{
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
PRBool nsExpatDTD::IsContainer(PRInt32 aTag) const{
  PRBool result=PR_TRUE;
  return result;
}

/**
 *  
 *  @update  vidur 11/12/98
 *  @param   aToken -- token object to be put into content model
 *  @return  0 if all is well; non-zero is an error
 */
NS_IMETHODIMP nsExpatDTD::HandleToken(CToken* aToken,nsIParser* aParser) {
  nsresult        result=NS_OK;
  CHTMLToken*     theToken= (CHTMLToken*)(aToken);
  eHTMLTokenTypes theType= (eHTMLTokenTypes)theToken->GetTokenType();

  mParser=(nsParser*)aParser;
  mSink=aParser->GetContentSink();

  nsCParserNode theNode(theToken,mLineNumber);
  switch(theType) {

    case eToken_newline:
      mLineNumber++; //now fall through
    case eToken_entity:
    case eToken_whitespace:
    case eToken_text:
    case eToken_cdatasection:
      result=mSink->AddLeaf(theNode); 
      break;

    case eToken_comment:
      result=mSink->AddComment(theNode); 
      break;
    
    case eToken_instruction:
      result=mSink->AddProcessingInstruction(theNode); 
      break;

    case eToken_start:
      {
        PRInt16 attrCount=aToken->GetAttributeCount();

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
        if(NS_OK==result){
          result=mSink->OpenContainer(theNode); 
          if(((CStartToken*)aToken)->IsEmpty()){
            result=mSink->CloseContainer(theNode); 
          }
        }
      }
      break;

    case eToken_end:
      result=mSink->CloseContainer(theNode); 
      break;

    case eToken_style:
    case eToken_skippedcontent:
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
nsresult nsExpatDTD::CaptureTokenPump(nsITagHandler* aHandler) {
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
nsresult nsExpatDTD::ReleaseTokenPump(nsITagHandler* aHandler){
  nsresult result=NS_OK;
  return result;
}

nsresult nsExpatDTD::ParseXMLBuffer(const char *buffer){
  nsresult result=NS_OK;
  if (mExpatParser) {
    if (!XML_Parse(mExpatParser, buffer, strlen(buffer), PR_FALSE)) {
      // XXX Add code here to implement error propagation to the
      // content sink.
      NS_NOTYETIMPLEMENTED("Error: nsExpatDTD::ParseXMLBuffer(): \
        Error propogation from expat not yet implemented.");
      result = NS_ERROR_FAILURE;
    }
  }
  else {
    result = NS_ERROR_FAILURE;
  }
  return result;
}

/***************************************/
/* Expat Callback Functions start here */
/***************************************/

void nsExpatDTD::HandleStartElement(void *userData, const XML_Char *name, const XML_Char **atts)
{
  NS_NOTYETIMPLEMENTED("Error: nsExpatDTD::HandleStartElement() not yet implemented.");
}

void nsExpatDTD::HandleEndElement(void *userData, const XML_Char *name)
{
  NS_NOTYETIMPLEMENTED("Error: nsExpatDTD::HandleEndElement() not yet implemented.");
}

void nsExpatDTD::HandleCharacterData(void *userData, const XML_Char *s, int len)
{
  NS_NOTYETIMPLEMENTED("Error: nsExpatDTD::HandleCharacterData() not yet implemented.");
}

void nsExpatDTD::HandleProcessingInstruction(void *userData, 
                                             const XML_Char *target, 
                                             const XML_Char *data)
{
  NS_NOTYETIMPLEMENTED("Error: nsExpatDTD::HandleProcessingInstruction() not yet implemented.");
}

void nsExpatDTD::HandleDefault(void *userData, const XML_Char *s, int len)
{
  NS_NOTYETIMPLEMENTED("Error: nsExpatDTD::HandleDefault() not yet implemented.");
}

void nsExpatDTD::HandleUnparsedEntityDecl(void *userData, 
                                          const XML_Char *entityName, 
                                          const XML_Char *base, 
                                          const XML_Char *systemId, 
                                          const XML_Char *publicId,
                                          const XML_Char *notationName)
{
  NS_NOTYETIMPLEMENTED("Error: nsExpatDTD::HandleUnparsedEntityDecl() not yet implemented.");
}

void nsExpatDTD::HandleNotationDecl(void *userData,
                                    const XML_Char *notationName,
                                    const XML_Char *base,
                                    const XML_Char *systemId,
                                    const XML_Char *publicId)
{
  NS_NOTYETIMPLEMENTED("Error: nsExpatDTD::HandleNotationDecl() not yet implemented.");
}

void nsExpatDTD::HandleExternalEntityRef(XML_Parser parser,
                                         const XML_Char *openEntityNames,
                                         const XML_Char *base,
                                         const XML_Char *systemId,
                                         const XML_Char *publicId)
{
  NS_NOTYETIMPLEMENTED("Error: nsExpatDTD::HandleExternalEntityRef() not yet implemented.");
}

void nsExpatDTD::HandleUnknownEncoding(void *encodingHandlerData,
                                       const XML_Char *name,
                                       XML_Encoding *info)
{
  NS_NOTYETIMPLEMENTED("Error: nsExpatDTD::HandleUnknownEncoding() not yet implemented.");
}
