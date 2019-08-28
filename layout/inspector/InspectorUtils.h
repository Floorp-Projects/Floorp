/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef mozilla_dom_InspectorUtils_h
#define mozilla_dom_InspectorUtils_h

#include "mozilla/dom/InspectorUtilsBinding.h"

class nsAtom;
class nsINode;
class ComputedStyle;

namespace mozilla {
class StyleSheet;
namespace css {
class Rule;
}  // namespace css
namespace dom {
class CharacterData;
class Element;
class InspectorFontFace;
}  // namespace dom
}  // namespace mozilla

namespace mozilla {
namespace dom {

/**
 * A collection of utility methods for use by devtools.
 */
class InspectorUtils {
 public:
  static void GetAllStyleSheets(GlobalObject& aGlobal, Document& aDocument,
                                bool aDocumentOnly,
                                nsTArray<RefPtr<StyleSheet>>& aResult);
  static void GetCSSStyleRules(GlobalObject& aGlobal, Element& aElement,
                               const nsAString& aPseudo,
                               nsTArray<RefPtr<BindingStyleRule>>& aResult);

  /**
   * Get the line number of a rule.
   *
   * @param aRule The rule.
   * @return The rule's line number.  Line numbers are 1-based.
   */
  static uint32_t GetRuleLine(GlobalObject& aGlobal, css::Rule& aRule);

  /**
   * Get the column number of a rule.
   *
   * @param aRule The rule.
   * @return The rule's column number.  Column numbers are 1-based.
   */
  static uint32_t GetRuleColumn(GlobalObject& aGlobal, css::Rule& aRule);

  /**
   * Like getRuleLine, but if the rule is in a <style> element,
   * returns a line number relative to the start of the element.
   *
   * @param aRule the rule to examine
   * @return the line number of the rule, possibly relative to the
   *         <style> element
   */
  static uint32_t GetRelativeRuleLine(GlobalObject& aGlobal, css::Rule& aRule);

  static bool HasRulesModifiedByCSSOM(GlobalObject& aGlobal,
                                      StyleSheet& aSheet);

  // Utilities for working with selectors.  We don't have a JS OM representation
  // of a single selector or a selector list yet, but given a rule we can index
  // into the selector list.
  //
  // These methods would probably make more sense being [ChromeOnly] APIs on
  // CSSStyleRule itself (bug 1428245).
  static uint32_t GetSelectorCount(GlobalObject& aGlobal,
                                   BindingStyleRule& aRule);

  // For all three functions below, aSelectorIndex is 0-based
  static void GetSelectorText(GlobalObject& aGlobal, BindingStyleRule& aRule,
                              uint32_t aSelectorIndex, nsString& aText,
                              ErrorResult& aRv);
  static uint64_t GetSpecificity(GlobalObject& aGlobal, BindingStyleRule& aRule,
                                 uint32_t aSelectorIndex, ErrorResult& aRv);
  // Note: This does not handle scoped selectors correctly, because it has no
  // idea what the right scope is.
  static bool SelectorMatchesElement(GlobalObject& aGlobal, Element& aElement,
                                     BindingStyleRule& aRule,
                                     uint32_t aSelectorIndex,
                                     const nsAString& aPseudo,
                                     ErrorResult& aRv);

  // Utilities for working with CSS properties
  //
  // Returns true if the string names a property that is inherited by default.
  static bool IsInheritedProperty(GlobalObject& aGlobal,
                                  const nsAString& aPropertyName);

  // Get a list of all our supported property names.  Optionally
  // property aliases included.
  static void GetCSSPropertyNames(GlobalObject& aGlobal,
                                  const PropertyNamesOptions& aOptions,
                                  nsTArray<nsString>& aResult);

  // Get a list of all properties controlled by preference, as well as
  // their corresponding preference names.
  static void GetCSSPropertyPrefs(GlobalObject& aGlobal,
                                  nsTArray<PropertyPref>& aResult);

  // Get a list of all valid keywords and colors for aProperty.
  static void GetCSSValuesForProperty(GlobalObject& aGlobal,
                                      const nsAString& aPropertyName,
                                      nsTArray<nsString>& aResult,
                                      ErrorResult& aRv);

  // Utilities for working with CSS colors
  static void RgbToColorName(GlobalObject& aGlobal, uint8_t aR, uint8_t aG,
                             uint8_t aB, nsAString& aResult, ErrorResult& aRv);

  // Convert a given CSS color string to rgba. Returns null on failure or an
  // InspectorRGBATuple on success.
  //
  // NOTE: Converting a color to RGBA may be lossy when converting from some
  // formats e.g. CMYK.
  static void ColorToRGBA(GlobalObject& aGlobal, const nsAString& aColorString,
                          Nullable<InspectorRGBATuple>& aResult);

  // Check whether a given color is a valid CSS color.
  static bool IsValidCSSColor(GlobalObject& aGlobal,
                              const nsAString& aColorString);

  // Utilities for obtaining information about a CSS property.

  // Get a list of the longhands corresponding to the given CSS property.  If
  // the property is a longhand already, just returns the property itself.
  // Throws on unsupported property names.
  static void GetSubpropertiesForCSSProperty(GlobalObject& aGlobal,
                                             const nsAString& aProperty,
                                             nsTArray<nsString>& aResult,
                                             ErrorResult& aRv);

  // Check whether a given CSS property is a shorthand.  Throws on unsupported
  // property names.
  static bool CssPropertyIsShorthand(GlobalObject& aGlobal,
                                     const nsAString& aProperty,
                                     ErrorResult& aRv);

  // Check whether values of the given type are valid values for the property.
  // For shorthands, checks whether there's a corresponding longhand property
  // that accepts values of this type.  Throws on unsupported properties or
  // unknown types.
  static bool CssPropertySupportsType(GlobalObject& aGlobal,
                                      const nsAString& aProperty,
                                      InspectorPropertyType, ErrorResult& aRv);

  static bool IsIgnorableWhitespace(GlobalObject& aGlobalObject,
                                    CharacterData& aDataNode) {
    return IsIgnorableWhitespace(aDataNode);
  }
  static bool IsIgnorableWhitespace(CharacterData& aDataNode);

  // Returns the "parent" of a node.  The parent of a document node is the
  // frame/iframe containing that document.  aShowingAnonymousContent says
  // whether we are showing anonymous content.
  static nsINode* GetParentForNode(nsINode& aNode,
                                   bool aShowingAnonymousContent);
  static nsINode* GetParentForNode(GlobalObject& aGlobalObject, nsINode& aNode,
                                   bool aShowingAnonymousContent) {
    return GetParentForNode(aNode, aShowingAnonymousContent);
  }

  static already_AddRefed<nsINodeList> GetChildrenForNode(
      GlobalObject& aGlobalObject, nsINode& aNode,
      bool aShowingAnonymousContent) {
    return GetChildrenForNode(aNode, aShowingAnonymousContent);
  }
  static already_AddRefed<nsINodeList> GetChildrenForNode(
      nsINode& aNode, bool aShowingAnonymousContent);

  static void GetBindingURLs(GlobalObject& aGlobal, Element& aElement,
                             nsTArray<nsString>& aResult);

  /**
   * Setting and removing content state on an element. Both these functions
   * call EventStateManager::SetContentState internally; the difference is
   * that for the remove case we simply pass in nullptr for the element.
   * Use them accordingly.
   *
   * When removing the active state, you may optionally also clear the active
   * document as well by setting aClearActiveDocument
   *
   * @return Returns true if the state was set successfully. See more details
   * in EventStateManager.h SetContentState.
   */
  static bool SetContentState(GlobalObject& aGlobal, Element& aElement,
                              uint64_t aState, ErrorResult& aRv);
  static bool RemoveContentState(GlobalObject& aGlobal, Element& aElement,
                                 uint64_t aState, bool aClearActiveDocument,
                                 ErrorResult& aRv);
  static uint64_t GetContentState(GlobalObject& aGlobal, Element& aElement);

  static void GetUsedFontFaces(GlobalObject& aGlobal, nsRange& aRange,
                               uint32_t aMaxRanges,  // max number of ranges to
                                                     // record for each face
                               bool aSkipCollapsedWhitespace,
                               nsTArray<nsAutoPtr<InspectorFontFace>>& aResult,
                               ErrorResult& aRv);

  /**
   * Get the names of all the supported pseudo-elements.
   * Pseudo-elements which are only accepted in UA style sheets are
   * not included.
   */
  static void GetCSSPseudoElementNames(GlobalObject& aGlobal,
                                       nsTArray<nsString>& aResult);

  // pseudo-class style locking methods. aPseudoClass must be a valid
  // pseudo-class selector string, e.g. ":hover". ":any-link" and
  // non-event-state pseudo-classes are ignored. aEnabled sets whether the
  // psuedo-class should be locked to on or off.
  static void AddPseudoClassLock(GlobalObject& aGlobal, Element& aElement,
                                 const nsAString& aPseudoClass, bool aEnabled);
  static void RemovePseudoClassLock(GlobalObject& aGlobal, Element& aElement,
                                    const nsAString& aPseudoClass);
  static bool HasPseudoClassLock(GlobalObject& aGlobal, Element& aElement,
                                 const nsAString& aPseudoClass);
  static void ClearPseudoClassLocks(GlobalObject& aGlobal, Element& aElement);

  /**
   * Parse CSS and update the style sheet in place.
   *
   * @param DOMCSSStyleSheet aSheet
   * @param DOMString aInput
   *        The new source string for the style sheet.
   */
  static void ParseStyleSheet(GlobalObject& aGlobal, StyleSheet& aSheet,
                              const nsAString& aInput, ErrorResult& aRv);

  /**
   * Check if the provided name can be custom element name.
   */
  static bool IsCustomElementName(GlobalObject&, const nsAString& aName,
                                  const nsAString& aNamespaceURI);

 private:
  static already_AddRefed<ComputedStyle> GetCleanComputedStyleForElement(
      Element* aElement, nsAtom* aPseudo);
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_InspectorUtils_h
