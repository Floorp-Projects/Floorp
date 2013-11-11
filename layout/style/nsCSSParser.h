/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* parsing of CSS stylesheets, based on a token stream from the CSS scanner */

#ifndef nsCSSParser_h___
#define nsCSSParser_h___

#include "mozilla/Attributes.h"

#include "nsCSSProperty.h"
#include "nsCOMPtr.h"
#include "nsStringFwd.h"
#include "nsTArrayForwardDeclare.h"

class nsCSSStyleSheet;
class nsIPrincipal;
class nsIURI;
struct nsCSSSelectorList;
class nsMediaList;
class nsCSSKeyframeRule;
class nsCSSValue;

namespace mozilla {
namespace css {
class Rule;
class Declaration;
class Loader;
class StyleRule;
}
}

// Interface to the css parser.

class MOZ_STACK_CLASS nsCSSParser {
public:
  nsCSSParser(mozilla::css::Loader* aLoader = nullptr,
              nsCSSStyleSheet* aSheet = nullptr);
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
                      uint32_t         aLineNumber,
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
                     mozilla::css::Rule**    aResult);

  // Parse the value of a single CSS property, and add or replace that
  // property in aDeclaration.
  //
  // SVG "mapped attributes" (which correspond directly to CSS
  // properties) are parsed slightly differently from regular CSS; in
  // particular, units may be omitted from <length>.  The 'aIsSVGMode'
  // argument controls this quirk.  Note that this *only* applies to
  // mapped attributes, not inline styles or full style sheets in SVG.
  nsresult ParseProperty(const nsCSSProperty aPropID,
                         const nsAString&    aPropValue,
                         nsIURI*             aSheetURL,
                         nsIURI*             aBaseURL,
                         nsIPrincipal*       aSheetPrincipal,
                         mozilla::css::Declaration* aDeclaration,
                         bool*               aChanged,
                         bool                aIsImportant,
                         bool                aIsSVGMode = false);

  /**
   * Parse aBuffer into a media list |aMediaList|, which must be
   * non-null, replacing its current contents.  If aHTMLMode is true,
   * parse according to HTML rules, with commas as the most important
   * delimiter.  Otherwise, parse according to CSS rules, with
   * parentheses and strings more important than commas.  |aURL| and
   * |aLineNumber| are used for error reporting.
   */
  void ParseMediaList(const nsSubstring& aBuffer,
                      nsIURI*            aURL,
                      uint32_t           aLineNumber,
                      nsMediaList*       aMediaList,
                      bool               aHTMLMode);

  /**
   * Parse aBuffer into a nsCSSValue |aValue|. Will return false
   * if aBuffer is not a valid CSS color specification.
   * One can use nsRuleNode::ComputeColor to compute an nscolor from
   * the returned nsCSSValue.
   */
  bool ParseColorString(const nsSubstring& aBuffer,
                        nsIURI*            aURL,
                        uint32_t           aLineNumber,
                        nsCSSValue&        aValue);

  /**
   * Parse aBuffer into a selector list.  On success, caller must
   * delete *aSelectorList when done with it.
   */
  nsresult ParseSelectorString(const nsSubstring&  aSelectorString,
                               nsIURI*             aURL,
                               uint32_t            aLineNumber,
                               nsCSSSelectorList** aSelectorList);

  /*
   * Parse a keyframe rule (which goes inside an @keyframes rule).
   * Return it if the parse was successful.
   */
  already_AddRefed<nsCSSKeyframeRule>
  ParseKeyframeRule(const nsSubstring& aBuffer,
                    nsIURI*            aURL,
                    uint32_t           aLineNumber);

  /*
   * Parse a selector list for a keyframe rule.  Return whether
   * the parse succeeded.
   */
  bool ParseKeyframeSelectorString(const nsSubstring& aSelectorString,
                                   nsIURI*            aURL,
                                   uint32_t           aLineNumber,
                                   InfallibleTArray<float>& aSelectorList);

  /**
   * Parse a property and value and return whether the property/value pair
   * is supported.
   */
  bool EvaluateSupportsDeclaration(const nsAString& aProperty,
                                   const nsAString& aValue,
                                   nsIURI* aDocURL,
                                   nsIURI* aBaseURL,
                                   nsIPrincipal* aDocPrincipal);

  /**
   * Parse an @supports condition and returns the result of evaluating the
   * condition.
   */
  bool EvaluateSupportsCondition(const nsAString& aCondition,
                                 nsIURI* aDocURL,
                                 nsIURI* aBaseURL,
                                 nsIPrincipal* aDocPrincipal);

protected:
  // This is a CSSParserImpl*, but if we expose that type name in this
  // header, we can't put the type definition (in nsCSSParser.cpp) in
  // the anonymous namespace.
  void* mImpl;
};

#endif /* nsCSSParser_h___ */
