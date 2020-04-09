/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_CSSEditUtils_h
#define mozilla_CSSEditUtils_h

#include "mozilla/ChangeStyleTransaction.h"  // for ChangeStyleTransaction
#include "nsCOMPtr.h"                        // for already_AddRefed
#include "nsStringFwd.h"
#include "nsTArray.h"  // for nsTArray
#include "nscore.h"    // for nsAString, nsresult, nullptr

class nsComputedDOMStyle;
class nsAtom;
class nsIContent;
class nsICSSDeclaration;
class nsINode;
class nsStaticAtom;

namespace mozilla {

class HTMLEditor;
namespace dom {
class Element;
}  // namespace dom

typedef void (*nsProcessValueFunc)(const nsAString* aInputString,
                                   nsAString& aOutputString,
                                   const char* aDefaultValueString,
                                   const char* aPrependString,
                                   const char* aAppendString);

class CSSEditUtils final {
 public:
  explicit CSSEditUtils(HTMLEditor* aEditor);

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

  struct CSSEquivTable {
    nsCSSEditableProperty cssProperty;
    nsProcessValueFunc processValueFunctor;
    const char* defaultValue;
    const char* prependValue;
    const char* appendValue;
    bool gettable;
    bool caseSensitiveValue;
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
   * Adds/remove a CSS declaration to the STYLE atrribute carried by a given
   * element.
   *
   * @param aElement       [IN] A DOM element.
   * @param aProperty      [IN] An atom containing the CSS property to set.
   * @param aValue         [IN] A string containing the value of the CSS
   *                            property.
   * @param aSuppressTransaction [IN] A boolean indicating, when true,
   *                                  that no transaction should be recorded.
   */
  MOZ_CAN_RUN_SCRIPT nsresult SetCSSProperty(dom::Element& aElement,
                                             nsAtom& aProperty,
                                             const nsAString& aValue,
                                             bool aSuppressTxn = false);
  MOZ_CAN_RUN_SCRIPT nsresult SetCSSPropertyPixels(dom::Element& aElement,
                                                   nsAtom& aProperty,
                                                   int32_t aIntValue);
  MOZ_CAN_RUN_SCRIPT nsresult RemoveCSSProperty(dom::Element& aElement,
                                                nsAtom& aProperty,
                                                const nsAString& aPropertyValue,
                                                bool aSuppressTxn = false);

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
   * @param aNode           [IN] The specific node we want to remove a style
   *                             from.
   * @param aProperty       [IN] The CSS property atom to remove.
   * @param aPropertyValue  [IN] The value of the property we have to remove
   *                             if the property accepts more than one value.
   */
  MOZ_CAN_RUN_SCRIPT nsresult RemoveCSSInlineStyle(
      nsINode& aNode, nsAtom* aProperty, const nsAString& aPropertyValue);

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
   * @param aContent       [IN] A DOM node.
   * @param aHTMLProperty  [IN] An atom containing an HTML property.
   * @param aAttribute     [IN] A pointer/atom to an attribute name or nullptr
   *                            if irrelevant.
   * @param aValueString   [IN/OUT] The attribute value (in) the list of CSS
   *                                values (out).
   * @return               A boolean being true if the css properties are
   *                       not same as initial value.
   */
  MOZ_CAN_RUN_SCRIPT static bool IsComputedCSSEquivalentToHTMLInlineStyleSet(
      nsIContent& aContent, nsAtom* aHTMLProperty, nsAtom* aAttribute,
      nsAString& aValue) {
    MOZ_ASSERT(aHTMLProperty || aAttribute);
    return IsCSSEquivalentToHTMLInlineStyleSetInternal(
        aContent, aHTMLProperty, aAttribute, aValue, StyleType::Computed);
  }
  MOZ_CAN_RUN_SCRIPT_BOUNDARY static bool
  IsSpecifiedCSSEquivalentToHTMLInlineStyleSet(nsIContent& aContent,
                                               nsAtom* aHTMLProperty,
                                               nsAtom* aAttribute,
                                               nsAString& aValue) {
    MOZ_ASSERT(aHTMLProperty || aAttribute);
    return IsCSSEquivalentToHTMLInlineStyleSetInternal(
        aContent, aHTMLProperty, aAttribute, aValue, StyleType::Specified);
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
   * @param aContent       [IN] A DOM node.
   * @param aHTMLProperty  [IN] An atom containing an HTML property.
   * @param aAttribute     [IN] An atom to an attribute name or nullptr
   *                            if irrelevant.
   * @return               A boolean being true if the css properties are
   *                       not set.
   */
  MOZ_CAN_RUN_SCRIPT static bool HaveComputedCSSEquivalentStyles(
      nsIContent& aContent, nsAtom* aHTMLProperty, nsAtom* aAttribute) {
    MOZ_ASSERT(aHTMLProperty || aAttribute);
    return HaveCSSEquivalentStylesInternal(aContent, aHTMLProperty, aAttribute,
                                           StyleType::Computed);
  }
  MOZ_CAN_RUN_SCRIPT_BOUNDARY static bool HaveSpecifiedCSSEquivalentStyles(
      nsIContent& aContent, nsAtom* aHTMLProperty, nsAtom* aAttribute) {
    MOZ_ASSERT(aHTMLProperty || aAttribute);
    return HaveCSSEquivalentStylesInternal(aContent, aHTMLProperty, aAttribute,
                                           StyleType::Specified);
  }

  /**
   * Adds to the node the CSS inline styles equivalent to the HTML style
   * and return the number of CSS properties set by the call.
   *
   * @param aNode          [IN] A DOM node.
   * @param aHTMLProperty  [IN] An atom containing an HTML property.
   * @param aAttribute     [IN] An atom to an attribute name or nullptr
   *                            if irrelevant.
   * @param aValue         [IN] The attribute value.
   * @param aSuppressTransaction [IN] A boolean indicating, when true,
   *                                  that no transaction should be recorded.
   *
   * @return               The number of CSS properties set by the call.
   */
  MOZ_CAN_RUN_SCRIPT int32_t SetCSSEquivalentToHTMLStyle(
      dom::Element* aElement, nsAtom* aProperty, nsAtom* aAttribute,
      const nsAString* aValue, bool aSuppressTransaction);

  /**
   * Removes from the node the CSS inline styles equivalent to the HTML style.
   *
   * @param aElement       [IN] A DOM Element (must not be null).
   * @param aHTMLProperty  [IN] An atom containing an HTML property.
   * @param aAttribute     [IN] An atom to an attribute name or nullptr if
   *                            irrelevant.
   * @param aValue         [IN] The attribute value.
   * @param aSuppressTransaction [IN] A boolean indicating, when true,
   *                                  that no transaction should be recorded.
   */
  MOZ_CAN_RUN_SCRIPT nsresult RemoveCSSEquivalentToHTMLStyle(
      dom::Element* aElement, nsAtom* aHTMLProperty, nsAtom* aAttribute,
      const nsAString* aValue, bool aSuppressTransaction);

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
   * Sets the mIsCSSPrefChecked private member; used as callback from observer
   *  when the CSS pref state is changed.
   *
   * @param aIsCSSPrefChecked [IN] The new boolean state for the pref.
   */
  void SetCSSEnabled(bool aIsCSSPrefChecked);

  /**
   * Retrieves the mIsCSSPrefChecked private member, true if the CSS pref is
   * checked, false if it is not.
   *
   * @return                 the boolean value of the CSS pref.
   */
  bool IsCSSPrefChecked() const;

  /**
   * ElementsSameStyle compares two elements and checks if they have the same
   * specified CSS declarations in the STYLE attribute.
   * The answer is always false if at least one of them carries an ID or a
   * class.
   *
   * @param aFirstNode           [IN] A DOM node.
   * @param aSecondNode          [IN] A DOM node.
   * @return                     true if the two elements are considered to
   *                             have same styles.
   */
  static bool ElementsSameStyle(dom::Element* aFirstNode,
                                dom::Element* aSecondNode);

  /**
   * Get the specified inline styles (style attribute) for an element.
   *
   * @param aElement        [IN] The element node.
   * @param aCssDecl        [OUT] The CSS declaration corresponding to the
   *                              style attribute.
   * @param aLength         [OUT] The number of declarations in aCssDecl.
   */
  static nsresult GetInlineStyles(dom::Element* aElement,
                                  nsICSSDeclaration** aCssDecl,
                                  uint32_t* aLength);

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
  MOZ_CAN_RUN_SCRIPT static bool IsCSSEquivalentToHTMLInlineStyleSetInternal(
      nsIContent& aContent, nsAtom* aHTMLProperty, nsAtom* aAttribute,
      nsAString& aValue, StyleType aStyleType);
  MOZ_CAN_RUN_SCRIPT static bool HaveCSSEquivalentStylesInternal(
      nsIContent& aContent, nsAtom* aHTMLProperty, nsAtom* aAttribute,
      StyleType aStyleType);

 private:
  HTMLEditor* mHTMLEditor;
  bool mIsCSSPrefChecked;
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
