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
static NS_DEFINE_IID(kIExpatTokenizerIID, NS_IEXPATTOKENIZER_IID);
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
    nsIExpatTokenizer *temp = this;
    *aInstancePtr = (void *) temp;                                        
  }
  else if(aIID.Equals(kHTMLTokenizerIID)) {  //do nsHTMLTokenizer base class...
    *aInstancePtr = (nsHTMLTokenizer*)(this);                                        
  }
  else if (aIID.Equals(kIExpatTokenizerIID)) { //do IExpatTokenizer base class...
    *aInstancePtr = (nsIExpatTokenizer*)(this);
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
 *  Default constructor
 *   
 *  @update  gess 4/9/98
 *  @param   
 *  @return  
 */
nsExpatTokenizer::nsExpatTokenizer() : nsHTMLTokenizer() {
  NS_INIT_REFCNT();
  
  // Create a new expat XML parser
  parser = XML_ParserCreate(NULL);
}

/**
 *  Destructor
 *   
 *  @update  gess 4/9/98
 *  @param   
 *  @return  
 */
nsExpatTokenizer::~nsExpatTokenizer(){
  if (parser) {
    XML_ParserFree(parser);
  }
}


/*******************************************************************
  Here begins the real working methods for the tokenizer.
 *******************************************************************/

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
  const char *expatBuffer = NULL;
  nsresult result = NS_OK;

  // XXX Rick should add a method to the scanner that gives me the
  // entire contents of the scanner's buffer without calling
  // GetChar() repeatedly.
  result = aScanner.ReadUntil(buffer, '\0', PR_FALSE);
  if (aScanner.Eof() == result || NS_OK == result) {
    expatBuffer = buffer.ToNewCString();
  
    if (expatBuffer) {
      if (parser) {
        if (!XML_Parse(parser, expatBuffer, strlen(expatBuffer), PR_FALSE)) {
          // XXX Add code here to implement error propagation to the
          // content sink.
          NS_NOTYETIMPLEMENTED("Error: nsExpatTokenizer::ConsumeToken(): \
            Error propogation from expat not yet implemented.");
        }
      }
      else {
        result = NS_ERROR_FAILURE;
      }
    }
  }

  return result;
}

/* XXX These other methods should presumably never get called.
   So, should I put an ASSERT here?  For now, I just pass the
   method call up to the parent class, nsHTMLTokenizer. */

nsITokenRecycler* nsExpatTokenizer::GetTokenRecycler(void)
{
  return nsHTMLTokenizer::GetTokenRecycler();
}

CToken* nsExpatTokenizer::PushTokenFront(CToken* theToken)
{
  return nsHTMLTokenizer::PushTokenFront(theToken);
}

CToken* nsExpatTokenizer::PushToken(CToken* theToken)
{
  return nsHTMLTokenizer::PushToken(theToken);
}

CToken* nsExpatTokenizer::PeekToken(void)
{
  return nsHTMLTokenizer::PeekToken();
}

CToken* nsExpatTokenizer::PopToken(void)
{
  return nsHTMLTokenizer::PopToken();
}

PRInt32 nsExpatTokenizer::GetCount(void)
{
  return nsHTMLTokenizer::GetCount();
}

CToken* nsExpatTokenizer::GetTokenAt(PRInt32 anIndex)
{
  return nsHTMLTokenizer::GetTokenAt(anIndex);
}

/************************************************/
/* Methods to set callbacks on the expat parser */
/************************************************/

void nsExpatTokenizer::SetElementHandler(XML_StartElementHandler start, XML_EndElementHandler end)
{
  PR_ASSERT(parser != NULL);
  XML_SetElementHandler(parser, start, end);
}

void nsExpatTokenizer::SetCharacterDataHandler(XML_CharacterDataHandler handler)
{
  PR_ASSERT(parser != NULL);
  XML_SetCharacterDataHandler(parser, handler);
}

void nsExpatTokenizer::SetProcessingInstructionHandler(XML_ProcessingInstructionHandler handler)
{
  PR_ASSERT(parser != NULL);
  XML_SetProcessingInstructionHandler(parser, handler);
}

void nsExpatTokenizer::SetDefaultHandler(XML_DefaultHandler handler)
{
  PR_ASSERT(parser != NULL);
  XML_SetDefaultHandler(parser, handler);
}

void nsExpatTokenizer::SetUnparsedEntityDeclHandler(XML_UnparsedEntityDeclHandler handler)
{
  PR_ASSERT(parser != NULL);
  XML_SetUnparsedEntityDeclHandler(parser, handler);
}

void nsExpatTokenizer::SetNotationDeclHandler(XML_NotationDeclHandler handler)
{
  PR_ASSERT(parser != NULL);
  XML_SetNotationDeclHandler(parser, handler);
}

void nsExpatTokenizer::SetExternalEntityRefHandler(XML_ExternalEntityRefHandler handler)
{
  PR_ASSERT(parser != NULL);
  XML_SetExternalEntityRefHandler(parser, handler);
}

void nsExpatTokenizer::SetUnknownEncodingHandler(XML_UnknownEncodingHandler handler, void *encodingHandlerData)
{
  PR_ASSERT(parser != NULL);
  XML_SetUnknownEncodingHandler(parser, handler, encodingHandlerData);
}