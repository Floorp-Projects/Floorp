/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/* parsing of CSS stylesheets, based on a token stream from the CSS scanner */

#ifndef nsICSSParser_h___
#define nsICSSParser_h___

#include "nsISupports.h"
#include "nsAString.h"
#include "nsCSSProperty.h"
#include "nsColor.h"
#include "nsCOMArray.h"

class nsICSSStyleRule;
class nsICSSStyleSheet;
class nsIUnicharInputStream;
class nsIURI;
class nsCSSDeclaration;
class nsICSSLoader;
class nsICSSRule;
class nsMediaList;
class nsIPrincipal;
struct nsCSSSelectorList;

#define NS_ICSS_PARSER_IID    \
{ 0xad4a3778, 0xdae0, 0x4640, \
 { 0xb2, 0x5a, 0x24, 0xff, 0x09, 0xc3, 0x70, 0xef } }

// Rule processing function
typedef void (* RuleAppendFunc) (nsICSSRule* aRule, void* aData);

// Interface to the css parser.
class nsICSSParser : public nsISupports {
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_ICSS_PARSER_IID)

  // Set a style sheet for the parser to fill in. The style sheet must
  // implement the nsICSSStyleSheet interface.  Null can be passed in to clear
  // out an existing stylesheet reference.
  NS_IMETHOD SetStyleSheet(nsICSSStyleSheet* aSheet) = 0;

  // Set whether or not tags & classes are case sensitive or uppercased
  NS_IMETHOD SetCaseSensitive(PRBool aCaseSensitive) = 0;

  // Set whether or not to emulate Nav quirks
  NS_IMETHOD SetQuirkMode(PRBool aQuirkMode) = 0;

#ifdef  MOZ_SVG
  // Set whether or not we are in an SVG element
  NS_IMETHOD SetSVGMode(PRBool aSVGMode) = 0;
#endif

  // Set loader to use for child sheets
  NS_IMETHOD SetChildLoader(nsICSSLoader* aChildLoader) = 0;

  /**
   * Parse aInput into the stylesheet that was previously set by calling
   * SetStyleSheet.  Calling this method without calling SetStyleSheet first is
   * an error.
   *
   * @param aInput the data to parse
   * @param aSheetURL the URI to use as the sheet URI (for error reporting).
   *                  This must match the URI of the sheet passed to
   *                  SetStyleSheet.
   * @param aBaseURI the URI to use for relative URI resolution
   * @param aSheetPrincipal the principal of the stylesheet.  This must match
   *                        the principal of the sheet passed to SetStyleSheet.
   * @param aLineNumber the line number of the first line of the sheet.   
   * @param aAllowUnsafeRules see aEnableUnsafeRules in
   *                          nsICSSLoader::LoadSheetSync
   */
  NS_IMETHOD Parse(nsIUnicharInputStream* aInput,
                   nsIURI*                aSheetURL,
                   nsIURI*                aBaseURI,
                   nsIPrincipal*          aSheetPrincipal,
                   PRUint32               aLineNumber,
                   PRBool                 aAllowUnsafeRules) = 0;

  // Parse HTML style attribute or its equivalent in other markup
  // languages.  aBaseURL is the base url to use for relative links in
  // the declaration.
  NS_IMETHOD ParseStyleAttribute(const nsAString&         aAttributeValue,
                                 nsIURI*                  aDocURL,
                                 nsIURI*                  aBaseURL,
                                 nsIPrincipal*            aNodePrincipal,
                                 nsICSSStyleRule**        aResult) = 0;

  NS_IMETHOD ParseAndAppendDeclaration(const nsAString&         aBuffer,
                                       nsIURI*                  aSheetURL,
                                       nsIURI*                  aBaseURL,
                                       nsIPrincipal*            aSheetPrincipal,
                                       nsCSSDeclaration*        aDeclaration,
                                       PRBool                   aParseOnlyOneDecl,
                                       PRBool*                  aChanged,
                                       PRBool                   aClearOldDecl) = 0;

  NS_IMETHOD ParseRule(const nsAString&        aRule,
                       nsIURI*                 aSheetURL,
                       nsIURI*                 aBaseURL,
                       nsIPrincipal*           aSheetPrincipal,
                       nsCOMArray<nsICSSRule>& aResult) = 0;

  NS_IMETHOD ParseProperty(const nsCSSProperty aPropID,
                           const nsAString& aPropValue,
                           nsIURI* aSheetURL,
                           nsIURI* aBaseURL,
                           nsIPrincipal* aSheetPrincipal,
                           nsCSSDeclaration* aDeclaration,
                           PRBool* aChanged) = 0;

  /**
   * Parse aBuffer into a media list |aMediaList|, which must be
   * non-null, replacing its current contents.  If aHTMLMode is true,
   * parse according to HTML rules, with commas as the most important
   * delimiter.  Otherwise, parse according to CSS rules, with
   * parentheses and strings more important than commas.
   */
  NS_IMETHOD ParseMediaList(const nsSubstring& aBuffer,
                            nsIURI* aURL, // for error reporting
                            PRUint32 aLineNumber, // for error reporting
                            nsMediaList* aMediaList,
                            PRBool aHTMLMode) = 0;

  /**
   * Parse aBuffer into a nscolor |aColor|.  The alpha component of the
   * resulting aColor may vary due to rgba()/hsla().  Will return
   * NS_ERROR_FAILURE if aBuffer is not a valid CSS color specification.
   *
   * Will also currently return NS_ERROR_FAILURE if it is not
   * self-contained (i.e.  doesn't reference any external style state,
   * such as "initial" or "inherit").
   */
  NS_IMETHOD ParseColorString(const nsSubstring& aBuffer,
                              nsIURI* aURL, // for error reporting
                              PRUint32 aLineNumber, // for error reporting
                              nscolor* aColor) = 0;

  /**
   * Parse aBuffer into a selector list.  On success, caller must
   * delete *aSelectorList when done with it.
   */
  NS_IMETHOD ParseSelectorString(const nsSubstring& aSelectorString,
                                 nsIURI* aURL, // for error reporting
                                 PRUint32 aLineNumber, // for error reporting
                                 nsCSSSelectorList **aSelectorList) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsICSSParser, NS_ICSS_PARSER_IID)

nsresult
NS_NewCSSParser(nsICSSParser** aInstancePtrResult);

#endif /* nsICSSParser_h___ */
