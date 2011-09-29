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

#ifndef nsCSSParser_h___
#define nsCSSParser_h___

#include "nsAString.h"
#include "nsCSSProperty.h"
#include "nsColor.h"
#include "nsCOMArray.h"
#include "nsCOMPtr.h"

class nsCSSStyleSheet;
class nsIPrincipal;
class nsIURI;
struct nsCSSSelectorList;
class nsMediaList;
class nsCSSKeyframeRule;

namespace mozilla {
namespace css {
class Rule;
class Declaration;
class Loader;
class StyleRule;
}
}

// Interface to the css parser.

class NS_STACK_CLASS nsCSSParser {
public:
  nsCSSParser(mozilla::css::Loader* aLoader = nsnull,
              nsCSSStyleSheet* aSheet = nsnull);
  ~nsCSSParser();

  static void Shutdown();

private:
  // not to be implemented
  nsCSSParser(nsCSSParser const&);
  nsCSSParser& operator=(nsCSSParser const&);

public:
  // Set a style sheet for the parser to fill in. The style sheet must
  // implement the nsCSSStyleSheet interface.  Null can be passed in to clear
  // out an existing stylesheet reference.
  nsresult SetStyleSheet(nsCSSStyleSheet* aSheet);

  // Set whether or not to emulate Nav quirks
  nsresult SetQuirkMode(bool aQuirkMode);

  // Set whether or not we are in an SVG element
  nsresult SetSVGMode(bool aSVGMode);

  // Set loader to use for child sheets
  nsresult SetChildLoader(mozilla::css::Loader* aChildLoader);

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
   *                          mozilla::css::Loader::LoadSheetSync
   */
  nsresult ParseSheet(const nsAString& aInput,
                      nsIURI*          aSheetURL,
                      nsIURI*          aBaseURI,
                      nsIPrincipal*    aSheetPrincipal,
                      PRUint32         aLineNumber,
                      bool             aAllowUnsafeRules);

  // Parse HTML style attribute or its equivalent in other markup
  // languages.  aBaseURL is the base url to use for relative links in
  // the declaration.
  nsresult ParseStyleAttribute(const nsAString&  aAttributeValue,
                               nsIURI*           aDocURL,
                               nsIURI*           aBaseURL,
                               nsIPrincipal*     aNodePrincipal,
                               mozilla::css::StyleRule** aResult);

  // Parse the body of a declaration block.  Very similar to
  // ParseStyleAttribute, but used under different circumstances.
  // The contents of aDeclaration will be erased and replaced with the
  // results of parsing; aChanged will be set true if the aDeclaration
  // argument was modified.
  nsresult ParseDeclarations(const nsAString&  aBuffer,
                             nsIURI*           aSheetURL,
                             nsIURI*           aBaseURL,
                             nsIPrincipal*     aSheetPrincipal,
                             mozilla::css::Declaration* aDeclaration,
                             bool*           aChanged);

  nsresult ParseRule(const nsAString&        aRule,
                     nsIURI*                 aSheetURL,
                     nsIURI*                 aBaseURL,
                     nsIPrincipal*           aSheetPrincipal,
                     nsCOMArray<mozilla::css::Rule>& aResult);

  nsresult ParseProperty(const nsCSSProperty aPropID,
                         const nsAString&    aPropValue,
                         nsIURI*             aSheetURL,
                         nsIURI*             aBaseURL,
                         nsIPrincipal*       aSheetPrincipal,
                         mozilla::css::Declaration* aDeclaration,
                         bool*             aChanged,
                         bool                aIsImportant);

  /**
   * Parse aBuffer into a media list |aMediaList|, which must be
   * non-null, replacing its current contents.  If aHTMLMode is true,
   * parse according to HTML rules, with commas as the most important
   * delimiter.  Otherwise, parse according to CSS rules, with
   * parentheses and strings more important than commas.  |aURL| and
   * |aLineNumber| are used for error reporting.
   */
  nsresult ParseMediaList(const nsSubstring& aBuffer,
                          nsIURI*            aURL,
                          PRUint32           aLineNumber,
                          nsMediaList*       aMediaList,
                          bool               aHTMLMode);

  /**
   * Parse aBuffer into a nscolor |aColor|.  The alpha component of the
   * resulting aColor may vary due to rgba()/hsla().  Will return
   * NS_ERROR_FAILURE if aBuffer is not a valid CSS color specification.
   *
   * Will also currently return NS_ERROR_FAILURE if it is not
   * self-contained (i.e.  doesn't reference any external style state,
   * such as "initial" or "inherit").
   */
  nsresult ParseColorString(const nsSubstring& aBuffer,
                            nsIURI*            aURL,
                            PRUint32           aLineNumber,
                            nscolor*           aColor);

  /**
   * Parse aBuffer into a selector list.  On success, caller must
   * delete *aSelectorList when done with it.
   */
  nsresult ParseSelectorString(const nsSubstring&  aSelectorString,
                               nsIURI*             aURL,
                               PRUint32            aLineNumber,
                               nsCSSSelectorList** aSelectorList);

  /*
   * Parse a keyframe rule (which goes inside an @keyframes rule).
   * Return it if the parse was successful.
   */
  already_AddRefed<nsCSSKeyframeRule>
  ParseKeyframeRule(const nsSubstring& aBuffer,
                    nsIURI*            aURL,
                    PRUint32           aLineNumber);

  /*
   * Parse a selector list for a keyframe rule.  Return whether
   * the parse succeeded.
   */
  bool ParseKeyframeSelectorString(const nsSubstring& aSelectorString,
                                   nsIURI*            aURL,
                                   PRUint32           aLineNumber,
                                   nsTArray<float>&   aSelectorList);

protected:
  // This is a CSSParserImpl*, but if we expose that type name in this
  // header, we can't put the type definition (in nsCSSParser.cpp) in
  // the anonymous namespace.
  void* mImpl;
};

#endif /* nsCSSParser_h___ */
