/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* parsing of CSS stylesheets, based on a token stream from the CSS scanner */

#ifndef nsCSSParser_h___
#define nsCSSParser_h___

#include "mozilla/Attributes.h"

#include "nsAString.h"
#include "nsCSSProperty.h"
#include "nsColor.h"
#include "nsCOMArray.h"
#include "nsCOMPtr.h"
#include "nsTArray.h"

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
  nsCSSParser(nsCSSParser const&) MOZ_DELETE;
  nsCSSParser& operator=(nsCSSParser const&) MOZ_DELETE;

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
                                   InfallibleTArray<float>& aSelectorList);

protected:
  // This is a CSSParserImpl*, but if we expose that type name in this
  // header, we can't put the type definition (in nsCSSParser.cpp) in
  // the anonymous namespace.
  void* mImpl;
};

#endif /* nsCSSParser_h___ */
