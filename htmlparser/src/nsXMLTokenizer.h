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
 * @update  gess 4/1/98
 * 
 */

#ifndef __NSXMLTOKENIZER
#define __NSXMLTOKENIZER

#include "nsISupports.h"
#include "nsHTMLTokenizer.h"
#include "nsIParser.h"
#include "nsIDTD.h"
#include "prtypes.h"
#include "nsDeque.h"
#include "nsScanner.h"
#include "nsHTMLTokens.h"

#define NS_XMLTOKENIZER_IID      \
  {0xcf22e1fa, 0x9ed2, 0x11d2, { 0xba, 0xa5, 0x0, 0x10, 0x4b, 0x98, 0x3f, 0xd4 }}



/***************************************************************
  Notes: 
 ***************************************************************/

#if defined(XP_PC)
#pragma warning( disable : 4275 )
#endif

CLASS_EXPORT_HTMLPARS nsXMLTokenizer : public nsHTMLTokenizer {
public:
            nsXMLTokenizer();
  virtual   ~nsXMLTokenizer();

            NS_DECL_ISUPPORTS

  virtual nsresult          ConsumeToken(nsScanner& aScanner,PRBool& aFlushTokens);
  virtual nsTokenAllocator*  GetTokenAllocator(void);

protected:

  virtual nsresult HandleSkippedContent(nsScanner& aScanner,CToken*& aToken);
  virtual nsresult ConsumeComment(PRUnichar aChar,CToken*& aToken,nsScanner& aScanner);
  virtual nsresult ConsumeSpecialMarkup(PRUnichar aChar,CToken*& aToken,nsScanner& aScanner);

};

extern NS_HTMLPARS nsresult NS_NewXMLTokenizer(nsITokenizer** aInstancePtrResult);

#endif


