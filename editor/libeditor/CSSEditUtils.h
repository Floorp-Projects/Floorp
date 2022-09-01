/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_CSSEditUtils_h
#define mozilla_CSSEditUtils_h

#include "mozilla/ChangeStyleTransaction.h"  // for ChangeStyleTransaction
#include "mozilla/EditorForwards.h"
#include "nsCOMPtr.h"  // for already_AddRefed
#include "nsStringFwd.h"
#include "nsTArray.h"  // for nsTArray
#include "nscore.h"    // for nsAString, nsresult, nullptr

class nsComputedDOMStyle;
class nsAtom;
class nsIContent;
class nsICSSDeclaration;
class nsINode;
class nsStaticAtom;
class nsStyledElement;

namespace mozilla {
namespace dom {
class Element;
}  // namespace dom

typedef void (*nsProcessValueFunc)(const nsAString* aInputString,
                                   nsAString& aOutputString,
                                   const char* aDefaultValueString,
                                   const char* aPrependString,
                                   const char* aAppendString);

class CSSEditUtils final {
  // To prevent the class being instantiated
  CSSEditUtils() = delete;

 public:
  enum nsCSSEditableProperty {
    eCSSEditableProperty_NONE = 0,
    eCSSEditableProperty_background_color,
    eCSSEditableProperty_background_image,
    eCSSEditableProperty_border,
    eCSSEditableProperty_caption_side,
    eCSSEditableProperty_color,
    eCSSEditableProperty_float,
    eCSSEditableProperty_font_family,
    eCSSEditableProperty_font_size,
    eCSSEditableProperty_font_style,
    eCSSEditableProperty_font_weight,
    eCSSEditableProperty_height,
    eCSSEditableProperty_list_style_type,
    eCSSEditableProperty_margin_left,
    eCSSEditableProperty_margin_right,
    eCSSEditableProperty_text_align,
    eCSSEditableProperty_text_decoration,
    eCSSEditableProperty_vertical_align,
    eCSSEditableProperty_whitespace,
    eCSSEditableProperty_width
  };

  // Nb: keep these fields in an order that minimizes padding.
  struct CSSEquivTable {
    nsCSSEditableProperty cssProperty;
    bool gettable;
    bool caseSensitiveValue;
    nsProcessValueFunc processValueFunctor;
    const char* defaultValue;
    const char* prependValue;
    const char* appendValue;
  };

  /**
   * Answers true if the given combination element_name/attribute_name
   * has a CSS equivalence in this implementation.
   *
   * @param aNode          [IN] A DOM node.
   * @param aProperty      [IN] An atom containing a HTML tag name.
   * @param aAttribute     [IN] An atom containing a HTML
   *                            attribute carried by the element above.
   * @return               A boolean saying if the tag/attribute has a CSS
   *                       equiv.
   */
  static bool IsCSSEditableProperty(nsINode* aNode, nsAtom* aProperty,
                                    nsAtom* aAttribute);

  /**
   * Adds/remove a CSS declaration to the STYLE attribute carried by a given
   * element.
   *
   * @param aHTMLEditor    [IN] An HTMLEditor instance
   * @param aStyledElement [IN] A DOM styled element.
   * @param aProperty      [IN] An atom containing the CSS property to set.
   * @param aValue         [IN] A string containing the value of the CSS
   *                            property.
   */
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT static nsresult
  SetCSSPropertyWithTransaction(HTMLEditor& aHTMLEditor,
                                nsStyledElement& aStyledElement,
                                nsAtom& aProperty, const nsAString& aValue) {
    return SetCSSPropertyInternal(aHTMLEditor, aStyledElement, aProperty,
                                  aValue, false);
  }
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT static nsresult
  SetCSSPropertyPixelsWithTransaction(HTMLEditor& aHTMLEditor,
                                      nsStyledElement& aStyledElement,
                                      nsAtom& aProperty, int32_t aIntValue);
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT static nsresult
  SetCSSPropertyPixelsWithoutTransaction(nsStyledElement& aStyledElement,
                                         const nsAtom& aProperty,
                                         int32_t aIntValue);
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT static nsresult
  RemoveCSSPropertyWithTransaction(HTMLEditor& aHTMLEditor,
                                   nsStyledElement& aStyledElement,
                                   nsAtom& aProperty,
                                   const nsAString& aPropertyValue) {
    return RemoveCSSPropertyInternal(aHTMLEditor, aStyledElement, aProperty,
                                     aPropertyValue, false);
  }

  /**
   * Gets the specified/computed style value of a CSS property for a given
   * node (or its element ancestor if it is not an element).
   *
   * @param aContent       [IN] A DOM node.
   * @param aProperty      [IN] An atom containing the CSS property to get.
   * @param aPropertyValue [OUT] The retrieved value of the property.
   */
  static nsresult GetSpecifiedProperty(nsIContent& aContent,
                                       nsAtom& aCSSProperty, nsAString& aValue);
  MOZ_CAN_RUN_SCRIPT static nsresult GetComputedProperty(nsIContent& aContent,
                                                         nsAtom& aCSSProperty,
                                                         nsAString& aValue);

  /**
   * Removes a CSS property from the specified declarations in STYLE attribute
   * and removes the node if it is an useless span.
   *
   * @param aHTMLEditor    [IN] An HTMLEditor instance
   * @param aStyledElement  [IN] The styled element we want to remove a style
   *                             from.
   * @param aProperty       [IN] The CSS property atom to remove.
   * @param aPropertyValue  [IN] The value of the property we have to remove
   *                             if the property accepts more than one value.
   * @return                A candidate point to put caret.
   */
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT static Result<EditorDOMPoint, nsresult>
  RemoveCSSInlineStyleWithTransaction(HTMLEditor& aHTMLEditor,
                                      nsStyledElement& aStyledElement,
                                      nsAtom* aProperty,
                                      const nsAString& aPropertyValue);

  /**
   * Answers true is the property can be removed by setting a "none" CSS value
   * on a node.
   *
   * @param aProperty     [IN] An atom containing a CSS property.
   * @param aAttribute    [IN] Pointer to an attribute name or null if this
   *                           information is irrelevant.
   * @return              A boolean saying if the property can be remove by
   *                      setting a "none" value.
   */
  static bool IsCSSInvertible(nsAtom& aProperty, nsAtom* aAttribute);

  /**
   * Get the default browser background color if we need it for
   * GetCSSBackgroundColorState().
   *
   * @param aColor         [OUT] The default color as it is defined in prefs.
   */
  static void GetDefaultBackgroundColor(nsAString& aColor);

  /**
   * Get the default length unit used for CSS Indent/Outdent.
   *
   * @param aLengthUnit    [OUT] The default length unit as it is defined in
   *                             prefs.
   */
  static void GetDefaultLengthUnit(nsAString& aLengthUnit);

  /**
   * Returns the list of values for the CSS equivalences to
   * the passed HTML style for the passed node.
   *
   * @param aContent       [IN] A DOM node.
   * @param aHTMLProperty  [IN] An atom containing an HTML property.
   * @param aAttribute     [IN] An atom of attribute name or nullptr if
   *                            irrelevant.
   * @param aValueString   [OUT] The list of CSS values.
   */
  MOZ_CAN_RUN_SCRIPT static nsresult
  GetComputedCSSEquivalentToHTMLInlineStyleSet(nsIContent& aContent,
                                               nsAtom* aHTMLProperty,
                                               nsAtom* aAttribute,
                                               nsAString& aValue) {
    return GetCSSEquivalentToHTMLInlineStyleSetInternal(
        aContent, aHTMLProperty, aAttribute, aValue, StyleType::Computed);
  }

  /**
   * Does the node aNode (or his parent if it is not an element node) carries
   * the CSS equivalent styles to the HTML style for this node ?
   *
   * @param aHTMLEditor    [IN] An HTMLEditor instance
   * @param aContent       [IN] A DOM node.
   * @param aHTMLProperty  [IN] An atom containing an HTML property.
   * @param aAttribute     [IN] A pointer/atom to an attribute name or nullptr
   *                            if irrelevant.
   * @param aValueString   [IN/OUT] The attribute value (in) the list of CSS
   *                                values (out).
   * @return               A boolean being true if the css properties are
   *                       not same as initial value.
   */
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT static Result<bool, nsresult>
  IsComputedCSSEquivalentToHTMLInlineStyleSet(const HTMLEditor& aHTMLEditor,
                                              nsIContent& aContent,
                                              nsAtom* aHTMLProperty,
                                              nsAtom* aAttribute,
                                              nsAString& aValue) {
    MOZ_ASSERT(aHTMLProperty || aAttribute);
    return IsCSSEquivalentToHTMLInlineStyleSetInternal(
        aHTMLEditor, aContent, aHTMLProperty, aAttribute, aValue,
        StyleType::Computed);
  }
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT_BOUNDARY static Result<bool, nsresult>
  IsSpecifiedCSSEquivalentToHTMLInlineStyleSet(const HTMLEditor& aHTMLEditor,
                                               nsIContent& aContent,
                                               nsAtom* aHTMLProperty,
                                               nsAtom* aAttribute,
                                               nsAString& aValue) {
    MOZ_ASSERT(aHTMLProperty || aAttribute);
    return IsCSSEquivalentToHTMLInlineStyleSetInternal(
        aHTMLEditor, aContent, aHTMLProperty, aAttribute, aValue,
        StyleType::Specified);
  }

  /**
   * This is a kind of IsCSSEquivalentToHTMLInlineStyleSet.
   * IsCSSEquivalentToHTMLInlineStyleSet returns whether the properties
   * aren't same as initial value.  But this method returns whether the
   * properties aren't set.
   * If node is <span style="font-weight: normal"/>,
   *  - Is(Computed|Specified)CSSEquivalentToHTMLInlineStyleSet returns false.
   *  - Have(Computed|Specified)CSSEquivalentStyles returns true.
   *
   * @param aHTMLEditor    [IN] An HTMLEditor instance
   * @param aContent       [IN] A DOM node.
   * @param aHTMLProperty  [IN] An atom containing an HTML property.
   * @param aAttribute     [IN] An atom to an attribute name or nullptr
   *                            if irrelevant.
   * @return               A boolean being true if the css properties are
   *                       not set.
   */
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT static Result<bool, nsresult>
  HaveComputedCSSEquivalentStyles(const HTMLEditor& aHTMLEditor,
                                  nsIContent& aContent, nsAtom* aHTMLProperty,
                                  nsAtom* aAttribute) {
    MOZ_ASSERT(aHTMLProperty || aAttribute);
    return HaveCSSEquivalentStylesInternal(aHTMLEditor, aContent, aHTMLProperty,
                                           aAttribute, StyleType::Computed);
  }
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT_BOUNDARY static Result<bool, nsresult>
  HaveSpecifiedCSSEquivalentStyles(const HTMLEditor& aHTMLEditor,
                                   nsIContent& aContent, nsAtom* aHTMLProperty,
                                   nsAtom* aAttribute) {
    MOZ_ASSERT(aHTMLProperty || aAttribute);
    return HaveCSSEquivalentStylesInternal(aHTMLEditor, aContent, aHTMLProperty,
                                           aAttribute, StyleType::Specified);
  }

  /**
   * Adds to the node the CSS inline styles equivalent to the HTML style
   * and return the number of CSS properties set by the call.
   *
   * @param aHTMLEditor    [IN} An HTMLEditor instance
   * @param aNode          [IN] A DOM node.
   * @param aHTMLProperty  [IN] An atom containing an HTML property.
   * @param aAttribute     [IN] An atom to an attribute name or nullptr
   *                            if irrelevant.
   * @param aValue         [IN] The attribute value.
   *
   * @return               The number of CSS properties set by the call.
   */
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT static Result<int32_t, nsresult>
  SetCSSEquivalentToHTMLStyleWithTransaction(HTMLEditor& aHTMLEditor,
                                             nsStyledElement& aStyledElement,
                                             nsAtom* aProperty,
                                             nsAtom* aAttribute,
                                             const nsAString* aValue) {
    return SetCSSEquivalentToHTMLStyleInternal(
        aHTMLEditor, aStyledElement, aProperty, aAttribute, aValue, false);
  }
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT static Result<int32_t, nsresult>
  SetCSSEquivalentToHTMLStyleWithoutTransaction(HTMLEditor& aHTMLEditor,
                                                nsStyledElement& aStyledElement,
                                                nsAtom* aProperty,
                                                nsAtom* aAttribute,
                                                const nsAString* aValue) {
    return SetCSSEquivalentToHTMLStyleInternal(
        aHTMLEditor, aStyledElement, aProperty, aAttribute, aValue, true);
  }

  /**
   * Removes from the node the CSS inline styles equivalent to the HTML style.
   *
   * @param aHTMLEditor    [IN} An HTMLEditor instance
   * @param aStyledElement [IN] A DOM Element (must not be null).
   * @param aHTMLProperty  [IN] An atom containing an HTML property.
   * @param aAttribute     [IN] An atom to an attribute name or nullptr if
   *                            irrelevant.
   * @param aValue         [IN] The attribute value.
   */
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT static nsresult
  RemoveCSSEquivalentToHTMLStyleWithTransaction(HTMLEditor& aHTMLEditor,
                                                nsStyledElement& aStyledElement,
                                                nsAtom* aHTMLProperty,
                                                nsAtom* aAttribute,
                                                const nsAString* aValue) {
    return RemoveCSSEquivalentToHTMLStyleInternal(
        aHTMLEditor, aStyledElement, aHTMLProperty, aAttribute, aValue, false);
  }
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT static nsresult
  RemoveCSSEquivalentToHTMLStyleWithoutTransaction(
      HTMLEditor& aHTMLEditor, nsStyledElement& aStyledElement,
      nsAtom* aHTMLProperty, nsAtom* aAttribute, const nsAString* aValue) {
    return RemoveCSSEquivalentToHTMLStyleInternal(
        aHTMLEditor, aStyledElement, aHTMLProperty, aAttribute, aValue, true);
  }

  /**
   * Parses a "xxxx.xxxxxuuu" string where x is a digit and u an alpha char.
   *
   * @param aString        [IN] Input string to parse.
   * @param aValue         [OUT] Numeric part.
   * @param aUnit          [OUT] Unit part.
   */
  static void ParseLength(const nsAString& aString, float* aValue,
                          nsAtom** aUnit);

  /**
   * DoStyledElementsHaveSameStyle compares two elements and checks if they have
   * the same specified CSS declarations in the STYLE attribute. The answer is
   * always false if at least one of them carries an ID or a class.
   *
   * @param aStyledElement      [IN] A styled element.
   * @param aOtherStyledElement [IN] The other styled element.
   * @return                    true if the two elements are considered to
   *                            have same styles.
   */
  static bool DoStyledElementsHaveSameStyle(
      nsStyledElement& aStyledElement, nsStyledElement& aOtherStyledElement);

 public:
  /**
   * Gets the computed style for a given element.  Can return null.
   */
  static already_AddRefed<nsComputedDOMStyle> GetComputedStyle(
      dom::Element* aElement);

 private:
  enum class StyleType { Specified, Computed };

  /**
   * Retrieves the CSS property atom from an enum.
   *
   * @param aProperty           The enum value for the property.
   * @return                    The corresponding atom.
   */
  static nsStaticAtom* GetCSSPropertyAtom(nsCSSEditableProperty aProperty);

  /**
   * Retrieves the CSS declarations equivalent to a HTML style value for
   * a given equivalence table.
   *
   * @param aOutArrayOfCSSProperty  [OUT] The array of css properties.
   * @param aOutArrayOfCSSValue     [OUT] The array of values for the CSS
   *                                      properties above.
   * @param aEquivTable             The equivalence table.
   * @param aValue                  The HTML style value.
   * @param aGetOrRemoveRequest     A boolean value being true if the call to
   *                                the current method is made for
   *                                Get*CSSEquivalentToHTMLInlineStyleSet()
   *                                or
   *                                RemoveCSSEquivalentToHTMLInlineStyleSet().
   */
  static void BuildCSSDeclarations(
      nsTArray<nsStaticAtom*>& aOutArrayOfCSSProperty,
      nsTArray<nsString>& aOutArrayOfCSSValue, const CSSEquivTable* aEquivTable,
      const nsAString* aValue, bool aGetOrRemoveRequest);

  /**
   * Retrieves the CSS declarations equivalent to the given HTML
   * property/attribute/value for a given node.
   *
   * @param aElement                The DOM node.
   * @param aHTMLProperty           An atom containing an HTML property.
   * @param aAttribute              An atom to an attribute name or nullptr
   *                                if irrelevant
   * @param aValue                  The attribute value.
   * @param aOutArrayOfCSSProperty  [OUT] The array of CSS properties.
   * @param aOutArrayOfCSSValue     [OUT] The array of values for the CSS
   *                                      properties above.
   * @param aGetOrRemoveRequest     A boolean value being true if the call to
   *                                the current method is made for
   *                                Get*CSSEquivalentToHTMLInlineStyleSet() or
   *                                RemoveCSSEquivalentToHTMLInlineStyleSet().
   */
  static void GenerateCSSDeclarationsFromHTMLStyle(
      dom::Element& aElement, nsAtom* aHTMLProperty, nsAtom* aAttribute,
      const nsAString* aValue, nsTArray<nsStaticAtom*>& aOutArrayOfCSSProperty,
      nsTArray<nsString>& aOutArrayOfCSSValue, bool aGetOrRemoveRequest);

  /**
   * Back-end for GetSpecifiedProperty and GetComputedProperty.
   *
   * @param aNode               [IN] A DOM node.
   * @param aProperty           [IN] A CSS property.
   * @param aValue              [OUT] The retrieved value for this property.
   */
  MOZ_CAN_RUN_SCRIPT static nsresult GetComputedCSSInlinePropertyBase(
      nsIContent& aContent, nsAtom& aCSSProperty, nsAString& aValue);
  static nsresult GetSpecifiedCSSInlinePropertyBase(nsIContent& aContent,
                                                    nsAtom& aCSSProperty,
                                                    nsAString& aValue);

  /**
   * Those methods are wrapped with corresponding methods which do not have
   * "Internal" in their names.  Don't use these methods directly even if
   * you want to use one of them in this class.
   * Note that these methods may run scrip only when StyleType is Computed.
   */
  MOZ_CAN_RUN_SCRIPT static nsresult
  GetCSSEquivalentToHTMLInlineStyleSetInternal(nsIContent& aContent,
                                               nsAtom* aHTMLProperty,
                                               nsAtom* aAttribute,
                                               nsAString& aValue,
                                               StyleType aStyleType);
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT static Result<bool, nsresult>
  IsCSSEquivalentToHTMLInlineStyleSetInternal(const HTMLEditor& aHTMLEditor,
                                              nsIContent& aContent,
                                              nsAtom* aHTMLProperty,
                                              nsAtom* aAttribute,
                                              nsAString& aValue,
                                              StyleType aStyleType);
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT static Result<bool, nsresult>
  HaveCSSEquivalentStylesInternal(const HTMLEditor& aHTMLEditor,
                                  nsIContent& aContent, nsAtom* aHTMLProperty,
                                  nsAtom* aAttribute, StyleType aStyleType);

  [[nodiscard]] MOZ_CAN_RUN_SCRIPT static nsresult RemoveCSSPropertyInternal(
      HTMLEditor& aHTMLEditor, nsStyledElement& aStyledElement,
      nsAtom& aProperty, const nsAString& aPropertyValue,
      bool aSuppressTxn = false);
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT static nsresult
  RemoveCSSEquivalentToHTMLStyleInternal(HTMLEditor& aHTMLEditor,
                                         nsStyledElement& aStyledElement,
                                         nsAtom* aHTMLProperty,
                                         nsAtom* aAttribute,
                                         const nsAString* aValue,
                                         bool aSuppressTransaction);
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT static nsresult SetCSSPropertyInternal(
      HTMLEditor& aHTMLEditor, nsStyledElement& aStyledElement,
      nsAtom& aProperty, const nsAString& aValue, bool aSuppressTxn = false);
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT static Result<int32_t, nsresult>
  SetCSSEquivalentToHTMLStyleInternal(HTMLEditor& aHTMLEditor,
                                      nsStyledElement& aStyledElement,
                                      nsAtom* aProperty, nsAtom* aAttribute,
                                      const nsAString* aValue,
                                      bool aSuppressTransaction);
};

#define NS_EDITOR_INDENT_INCREMENT_IN 0.4134f
#define NS_EDITOR_INDENT_INCREMENT_CM 1.05f
#define NS_EDITOR_INDENT_INCREMENT_MM 10.5f
#define NS_EDITOR_INDENT_INCREMENT_PT 29.76f
#define NS_EDITOR_INDENT_INCREMENT_PC 2.48f
#define NS_EDITOR_INDENT_INCREMENT_EM 3
#define NS_EDITOR_INDENT_INCREMENT_EX 6
#define NS_EDITOR_INDENT_INCREMENT_PX 40
#define NS_EDITOR_INDENT_INCREMENT_PERCENT 4

}  // namespace mozilla

#endif  // #ifndef mozilla_CSSEditUtils_h
