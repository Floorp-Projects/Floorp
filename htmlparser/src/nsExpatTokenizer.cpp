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
 * @update  gess 4/1/98
 * 
 */

#include "nsExpatTokenizer.h"
#include "nsParser.h"
#include "nsScanner.h"
#include "nsDTDUtils.h"
#include "nsParser.h"

/************************************************************************
  And now for the main class -- nsExpatTokenizer...
 ************************************************************************/

static NS_DEFINE_IID(kISupportsIID,       NS_ISUPPORTS_IID);                 
static NS_DEFINE_IID(kITokenizerIID,      NS_ITOKENIZER_IID);
static NS_DEFINE_IID(kHTMLTokenizerIID,   NS_HTMLTOKENIZER_IID);
static NS_DEFINE_IID(kClassIID,           NS_EXPATTOKENIZER_IID);

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
nsresult nsExpatTokenizer::QueryInterface(const nsIID& aIID, void** aInstancePtr)  
{                                                                        
  if (NULL == aInstancePtr) {                                            
    return NS_ERROR_NULL_POINTER;                                        
  }                                                                      

  if(aIID.Equals(kISupportsIID))    {  //do IUnknown...
    *aInstancePtr = (nsExpatTokenizer*)(this);                                        
  }
  else if(aIID.Equals(kITokenizerIID)) {  //do ITokenizer base class...    
    *aInstancePtr = (nsITokenizer*)(this);
  }
  else if(aIID.Equals(kHTMLTokenizerIID)) {  //do nsHTMLTokenizer base class...
    *aInstancePtr = (nsHTMLTokenizer*)(this);                                        
  }
  else if(aIID.Equals(kClassIID)) {  //do this class...
    *aInstancePtr = (nsExpatTokenizer*)(this);                                        
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
NS_HTMLPARS nsresult NS_New_Expat_Tokenizer(nsIDTD** aInstancePtrResult) {
  nsExpatTokenizer* it = new nsExpatTokenizer();
  if (it == 0) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kClassIID, (void **) aInstancePtrResult);
}


NS_IMPL_ADDREF(nsExpatTokenizer)
NS_IMPL_RELEASE(nsExpatTokenizer)

/**
 * Sets up the callbacks for the expat parser      
 * @update  nra 2/24/99
 * @param   none
 * @return  none
 */
void nsExpatTokenizer::SetupExpatCallbacks(void) {
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
 *  Default constructor
 *   
 *  @update  gess 4/9/98
 *  @param   
 *  @return  
 */
nsExpatTokenizer::nsExpatTokenizer() : nsHTMLTokenizer() {
  NS_INIT_REFCNT();
  mExpatParser = XML_ParserCreate(NULL);
  if (mExpatParser) {
    SetupExpatCallbacks();
  }
}

/**
 *  Destructor
 *   
 *  @update  gess 4/9/98
 *  @param   
 *  @return  
 */
nsExpatTokenizer::~nsExpatTokenizer(){
  if (mExpatParser)
    XML_ParserFree(mExpatParser);  
}


/*******************************************************************
  Here begins the real working methods for the tokenizer.
 *******************************************************************/

nsresult nsExpatTokenizer::ParseXMLBuffer(const char *buffer){
  nsresult result=NS_OK;
  if (mExpatParser) {
    if (!XML_Parse(mExpatParser, buffer, strlen(buffer), PR_FALSE)) {
      // XXX Add code here to implement error propagation to the
      // content sink.
      NS_NOTYETIMPLEMENTED("Error: nsExpatTokenizer::ParseXMLBuffer(): \
        Error propogation from expat not yet implemented.");
      result = NS_ERROR_FAILURE;
    }
  }
  else {
    result = NS_ERROR_FAILURE;
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
nsresult nsExpatTokenizer::ConsumeToken(nsScanner& aScanner) {
  
  // return nsHTMLTokenizer::ConsumeToken(aScanner);

  // Ask the scanner to send us all the data it has
  // scanned and pass that data to expat.
  nsString buffer;
  char *expatBuffer = NULL;
  nsresult result = NS_OK;

  // XXX Rick should add a method to the scanner that gives me the
  // entire contents of the scanner's buffer without calling
  // GetChar() repeatedly.
  buffer = aScanner.GetBuffer();
  if (buffer) {
    expatBuffer = buffer.ToNewCString();
  
    if (expatBuffer) {      
      result = ParseXMLBuffer(expatBuffer);
      delete [] expatBuffer;
    }
  }

  return result;
}

/***************************************/
/* Expat Callback Functions start here */
/***************************************/

void nsExpatTokenizer::HandleStartElement(void *userData, const XML_Char *name, const XML_Char **atts)
{
  NS_NOTYETIMPLEMENTED("Error: nsExpatTokenizer::HandleStartElement() not yet implemented.");
}

void nsExpatTokenizer::HandleEndElement(void *userData, const XML_Char *name)
{
  NS_NOTYETIMPLEMENTED("Error: nsExpatTokenizer::HandleEndElement() not yet implemented.");
}

void nsExpatTokenizer::HandleCharacterData(void *userData, const XML_Char *s, int len)
{
  NS_NOTYETIMPLEMENTED("Error: nsExpatTokenizer::HandleCharacterData() not yet implemented.");
}

void nsExpatTokenizer::HandleProcessingInstruction(void *userData, 
                                             const XML_Char *target, 
                                             const XML_Char *data)
{
  NS_NOTYETIMPLEMENTED("Error: nsExpatTokenizer::HandleProcessingInstruction() not yet implemented.");
}

void nsExpatTokenizer::HandleDefault(void *userData, const XML_Char *s, int len)
{
  NS_NOTYETIMPLEMENTED("Error: nsExpatTokenizer::HandleDefault() not yet implemented.");
}

void nsExpatTokenizer::HandleUnparsedEntityDecl(void *userData, 
                                          const XML_Char *entityName, 
                                          const XML_Char *base, 
                                          const XML_Char *systemId, 
                                          const XML_Char *publicId,
                                          const XML_Char *notationName)
{
  NS_NOTYETIMPLEMENTED("Error: nsExpatTokenizer::HandleUnparsedEntityDecl() not yet implemented.");
}

void nsExpatTokenizer::HandleNotationDecl(void *userData,
                                    const XML_Char *notationName,
                                    const XML_Char *base,
                                    const XML_Char *systemId,
                                    const XML_Char *publicId)
{
  NS_NOTYETIMPLEMENTED("Error: nsExpatTokenizer::HandleNotationDecl() not yet implemented.");
}

void nsExpatTokenizer::HandleExternalEntityRef(XML_Parser parser,
                                         const XML_Char *openEntityNames,
                                         const XML_Char *base,
                                         const XML_Char *systemId,
                                         const XML_Char *publicId)
{
  NS_NOTYETIMPLEMENTED("Error: nsExpatTokenizer::HandleExternalEntityRef() not yet implemented.");
}

void nsExpatTokenizer::HandleUnknownEncoding(void *encodingHandlerData,
                                       const XML_Char *name,
                                       XML_Encoding *info)
{
  NS_NOTYETIMPLEMENTED("Error: nsExpatTokenizer::HandleUnknownEncoding() not yet implemented.");
}
