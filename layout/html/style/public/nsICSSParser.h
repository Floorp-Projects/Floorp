/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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
#ifndef nsCSS1Parser_h___
#define nsCSS1Parser_h___

#include "nslayout.h"
#include "nsISupports.h"
#include "nsAWritableString.h"
class nsIStyleRule;
class nsICSSStyleSheet;
class nsIUnicharInputStream;
class nsIURI;
class nsICSSDeclaration;
class nsICSSLoader;

#define NS_ICSS_PARSER_IID    \
{ 0xcc9c0610, 0x968c, 0x11d1, \
  {0x93, 0x23, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32} }

// Interface to the css parser.
class nsICSSParser : public nsISupports {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_ICSS_PARSER_IID; return iid; }

  // Return a mask of the various css standards that this parser
  // supports.
  NS_IMETHOD GetInfoMask(PRUint32& aResult) = 0;

  // Set a style sheet for the parser to fill in. The style sheet must
  // implement the nsICSSStyleSheet interface
  NS_IMETHOD SetStyleSheet(nsICSSStyleSheet* aSheet) = 0;

  // Set whether or not tags & classes are case sensitive or uppercased
  NS_IMETHOD SetCaseSensitive(PRBool aCaseSensitive) = 0;

  // Set whether or not to emulate Nav quirks
  NS_IMETHOD SetQuirkMode(PRBool aQuirkMode) = 0;

  // Set loader to use for child sheets
  NS_IMETHOD SetChildLoader(nsICSSLoader* aChildLoader) = 0;

  NS_IMETHOD Parse(nsIUnicharInputStream* aInput,
                   nsIURI*                aInputURL,
                   nsICSSStyleSheet*&     aResult) = 0;

  // Parse declarations assuming that the outer curly braces have
  // already been accounted for. aBaseURL is the base url to use for
  // relative links in the declaration.
  NS_IMETHOD ParseDeclarations(const nsAReadableString& aDeclaration,
                               nsIURI*                  aBaseURL,
                               nsIStyleRule*&           aResult) = 0;

  NS_IMETHOD ParseAndAppendDeclaration(const nsAReadableString& aBuffer,
                                       nsIURI*                  aBaseURL,
                                       nsICSSDeclaration*       aDeclaration,
                                       PRBool                   aParseOnlyOneDecl,
                                       PRInt32*                 aHint) = 0;

  // Charset management method:
  //  Set the charset before calling any of the Parse emthods if you want the 
  //  charset to be anything other than the default

  // sets the out-param to the current charset, as set by SetCharset
  NS_IMETHOD GetCharset(/*out*/nsAWritableString &aCharsetDest) const = 0;

  // SetCharset expects the charset to be the preferred charset
  // and it just records the string exactly as passed in (no alias resolution)
  NS_IMETHOD SetCharset(/*in*/ const nsAReadableString &aCharsetSrc) = 0;
};

// Values or'd in the GetInfoMask; other bits are reserved
#define NS_CSS_GETINFO_CSS1         ((PRUint32) 0x00000001L)
#define NS_CSS_GETINFO_CSSP         ((PRUint32) 0x00000002L)
#define NS_CSS_GETINFO_CSS2         ((PRUint32) 0x00000004L)
#define NS_CSS_GETINFO_CSS_FROSTING ((PRUint32) 0x00000008L)

// Success code that can be returned from ParseAndAppendDeclaration()
#define NS_CSS_PARSER_DROP_DECLARATION \
  NS_ERROR_GENERATE_SUCCESS(NS_ERROR_MODULE_LAYOUT,1)

extern NS_HTML nsresult
  NS_NewCSSParser(nsICSSParser** aInstancePtrResult);

#endif /* nsCSS1Parser_h___ */
