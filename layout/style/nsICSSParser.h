/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
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
class nsICSSRule;
class nsISupportsArray;

#define NS_ICSS_PARSER_IID    \
{ 0xcc9c0610, 0x968c, 0x11d1, \
  {0x93, 0x23, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32} }

// Rule processing function
typedef void (*PR_CALLBACK RuleAppendFunc) (nsICSSRule* aRule, void* aData);

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

  // Parse HTML style attribute or its equivalent in other markup
  // languages.  aBaseURL is the base url to use for relative links in
  // the declaration.
  NS_IMETHOD ParseStyleAttribute(const nsAReadableString& aAttributeValue,
                                 nsIURI*                  aBaseURL,
                                 nsIStyleRule**           aResult) = 0;

  NS_IMETHOD ParseAndAppendDeclaration(const nsAReadableString& aBuffer,
                                       nsIURI*                  aBaseURL,
                                       nsICSSDeclaration*       aDeclaration,
                                       PRBool                   aParseOnlyOneDecl,
                                       PRInt32*                 aHint) = 0;

  NS_IMETHOD ParseRule(nsAReadableString& aRule,
                       nsIURI*            aBaseURL,
                       nsISupportsArray** aResult) = 0;
  
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
