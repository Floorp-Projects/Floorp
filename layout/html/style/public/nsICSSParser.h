/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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
#ifndef nsCSS1Parser_h___
#define nsCSS1Parser_h___

#include "nslayout.h"
#include "nsISupports.h"
class nsIStyleRule;
class nsIStyleSheet;
class nsIUnicharInputStream;
class nsIURL;
class nsString;

#define NS_ICSS_PARSER_IID    \
{ 0xcc9c0610, 0x968c, 0x11d1, \
  {0x93, 0x23, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32} }

// Interface to the css parser.
class nsICSSParser : public nsISupports {
public:
  // Return a mask of the various css standards that this parser
  // supports.
  NS_IMETHOD GetInfoMask(PRUint32& aResult) = 0;

  // Set a style sheet for the parser to fill in. The style sheet must
  // implement the nsICSSStyleSheet interface
  NS_IMETHOD SetStyleSheet(nsIStyleSheet* aSheet) = 0;

  NS_IMETHOD Parse(nsIUnicharInputStream* aInput,
                   nsIURL*                aInputURL,
                   nsIStyleSheet*&        aResult) = 0;

  // Parse declarations assuming that the outer curly braces have
  // already been accounted for. aBaseURL is the base url to use for
  // relative links in the declaration.
  NS_IMETHOD ParseDeclarations(const nsString& aDeclaration,
                               nsIURL*         aBaseURL,
                               nsIStyleRule*&  aResult) = 0;
};

// Values or'd in the GetInfoMask; other bits are reserved
#define NS_CSS_GETINFO_CSS1         ((PRUint32) 0x00000001L)
#define NS_CSS_GETINFO_CSSP         ((PRUint32) 0x00000002L)
#define NS_CSS_GETINFO_CSS2         ((PRUint32) 0x00000004L)
#define NS_CSS_GETINFO_CSS_FROSTING ((PRUint32) 0x00000008L)

extern NS_HTML nsresult
  NS_NewCSSParser(nsICSSParser** aInstancePtrResult);

#endif /* nsCSS1Parser_h___ */
