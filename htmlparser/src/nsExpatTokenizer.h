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
 * This Original Code has been modified by IBM Corporation. Modifications made by IBM 
 * described herein are Copyright (c) International Business Machines Corporation, 2000.
 * Modifications to Mozilla code or documentation identified per MPL Section 3.3
 *
 * Date             Modified by     Description of modification
 * 04/20/2000       IBM Corp.      OS/2 VisualAge build.
 */


/**
 * MODULE NOTES:
 * @update  gess 4/1/98
 * 
 */

#ifndef __nsExpatTokenizer
#define __nsExpatTokenizer

#include "nsISupports.h"
#include "nsHTMLTokenizer.h"
#include "prtypes.h"

// Enable unicode characters in expat.
#define UNICODE
#include "xmlparse.h"

#define NS_EXPATTOKENIZER_IID      \
  {0x483836aa, 0xcabe, 0x11d2, { 0xab, 0xcb, 0x0, 0x10, 0x4b, 0x98, 0x3f, 0xd4 }}


// {575C063A-AE9C-11d3-B9FD-001083023C0E}
#define NS_EXPATTOKENIZER_CID  \
{ 0x575c063a, 0xae9c, 0x11d3, \
  {0xb9, 0xfd, 0x0, 0x10, 0x83, 0x2, 0x3c, 0xe}}


typedef struct _XMLParserState XMLParserState;

/***************************************************************
  Notes: 
 ***************************************************************/

#if defined(XP_PC)
#pragma warning( disable : 4275 )
#endif

CLASS_EXPORT_HTMLPARS nsExpatTokenizer : public nsHTMLTokenizer {
public:
          nsExpatTokenizer(nsString* aURL = nsnull);      
  virtual ~nsExpatTokenizer();

  virtual const   nsIID& GetCID();
  static  const   nsIID& GetIID();

          NS_DECL_ISUPPORTS

  /* nsITokenizer methods */
  virtual nsresult WillTokenize(PRBool aIsFinalChunk,nsTokenAllocator* aTokenAllocator);
  virtual nsresult ConsumeToken(nsScanner& aScanner,PRBool& aFlushTokens);  
  virtual nsresult DidTokenize(PRBool aIsFinalChunk);

  virtual void    FrontloadMisplacedContent(nsDeque& aDeque);

protected:

  /**
   * Parse an XML buffer using expat
   * @update	nra 2/29/99
   * @return  NS_ERROR_FAILURE if expat encounters an error, else NS_OK
   */
  nsresult ParseXMLBuffer(const char *aBuffer, PRUint32 aLength, PRBool aIsFinal=PR_FALSE);

  /**
   * Sets up the callbacks and user data for the expat parser      
   * @update  nra 2/24/99
   * @param   none
   * @return  none
   */
  void SetupExpatParser(void);

  // Propagate XML errors to the content sink
  nsresult PushXMLErrorTokens(const char *aBuffer, PRUint32 aLength, PRBool aIsFinal);
  nsresult AddErrorMessageTokens(nsParserError* aError);

	void GetLine(const char* aSourceBuffer, PRUint32 aLength, 
    PRUint32 aByteIndex, nsString& aLine);

  // Load up an external stream to get external entity information
  static nsresult OpenInputStream(const nsString& aURLStr, 
                                  const nsString& aBaseURL, 
                                  nsIInputStream** in, 
                                  nsString* aAbsURL);

  static nsresult LoadStream(nsIInputStream* in, 
                             PRUnichar* &uniBuf, 
                             PRUint32 &retLen);

  /* The callback handlers that get called from the expat parser */
  static void PR_CALLBACK HandleStartElement(void *userData, const XML_Char *name, const XML_Char **atts);
  static void PR_CALLBACK HandleEndElement(void *userData, const XML_Char *name);
  static void PR_CALLBACK HandleCharacterData(void *userData, const XML_Char *s, int len);
  static void PR_CALLBACK HandleComment(void *userData, const XML_Char *name);
  static void PR_CALLBACK HandleProcessingInstruction(void *userData, 
    const XML_Char *target, 
    const XML_Char *data);
  static void PR_CALLBACK HandleDefault(void *userData, const XML_Char *s, int len);
  static void PR_CALLBACK HandleStartCdataSection(void *userData);
  static void PR_CALLBACK HandleEndCdataSection(void *userData);
  static void PR_CALLBACK HandleUnparsedEntityDecl(void *userData, 
    const XML_Char *entityName, 
    const XML_Char *base, 
    const XML_Char *systemId, 
    const XML_Char *publicId,
    const XML_Char *notationName);
  static void PR_CALLBACK HandleNotationDecl(void *userData,
    const XML_Char *notationName,
    const XML_Char *base,
    const XML_Char *systemId,
    const XML_Char *publicId);
  static int PR_CALLBACK HandleExternalEntityRef(XML_Parser parser,
    const XML_Char *openEntityNames,
    const XML_Char *base,
    const XML_Char *systemId,
    const XML_Char *publicId);
  static int PR_CALLBACK HandleUnknownEncoding(void *encodingHandlerData,
    const XML_Char *name,
    XML_Encoding *info);
  static void PR_CALLBACK HandleStartDoctypeDecl(void *userData,
    const XML_Char *doctypeName);
  static void PR_CALLBACK HandleEndDoctypeDecl(void *userData);

  XML_Parser mExpatParser;
	PRUint32 mBytesParsed;
  nsString mLastLine;
  XMLParserState* mState;
};

extern NS_HTMLPARS nsresult NS_New_Expat_Tokenizer(nsITokenizer** aInstancePtrResult);

#endif
