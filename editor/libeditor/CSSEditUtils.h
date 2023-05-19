/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CSSEditUtils_h
#define CSSEditUtils_h

#include "ChangeStyleTransaction.h"  // for ChangeStyleTransaction
#include "EditorForwards.h"
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

using nsProcessValueFunc = void (*)(const nsAString* aInputString,
                                    nsAString& aOutputString,
                                    const char* aDefaultValueString,
                                    const char* aPrependString,
                                    const char* aAppendString);

class CSSEditUtils final {
 public:
  // To prevent the class being instantiated
  CSSEditUtils() = delete;

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
   * Get the default browser background color if we need it for
   * GetCSSBackgroundColorState().
   *
   * @param aColor         [OUT] The default color as it is defined in prefs.
   */
  static void GetDefaultBackgroundColor(nsAString& aColor);

  /**
   * Returns the list of values for the CSS equivalences to
   * the passed HTML style for the passed node.
   *
   * @param aElement       [IN] A DOM node.
   * @param aStyle         [IN] The style to get the values.
   * @param aValueString   [OUT] The list of CSS values.
   */
  MOZ_CAN_RUN_SCRIPT static nsresult GetComputedCSSEquivalentTo(
      dom::Element& aElement, const EditorElementStyle& aStyle,
      nsAString& aOutValue);

  /**
   * Does the node aNode (or his parent if it is not an element node) carries
   * the CSS equivalent styles to the HTML style for this node ?
   *
   * @param aHTMLEditor    [IN] An HTMLEditor instance
   * @param aContent       [IN] A DOM node.
   * @param aStyle         [IN] The style to check.
   * @param aInOutValue    [IN/OUT] Input value is used for initial value of the
   *                                result, out value is the list of CSS values.
   * @return               A boolean being true if the css properties are
   *                       not same as initial value.
   */
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT static Result<bool, nsresult>
  IsComputedCSSEquivalentTo(const HTMLEditor& aHTMLEditor, nsIContent& aContent,
                            const EditorInlineStyle& aStyle,
                            nsAString& aInOutValue);
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT_BOUNDARY static Result<bool, nsresult>
  IsSpecifiedCSSEquivalentTo(const HTMLEditor& aHTMLEditor,
                             nsIContent& aContent,
                             const EditorInlineStyle& aStyle,
                             nsAString& aInOutValue);

  /**
   * This is a kind of Is*CSSEquivalentTo.
   * Is*CSSEquivalentTo returns whether the properties aren't same as initial
   * value.  But this method returns whether the properties aren't set.
   * If node is <span style="font-weight: normal"/>,
   *  - Is(Computed|Specified)CSSEquivalentTo returns false.
   *  - Have(Computed|Specified)CSSEquivalentStyles returns true.
   *
   * @param aHTMLEditor    [IN] An HTMLEditor instance
   * @param aContent       [IN] A DOM node.
   * @param aStyle         [IN] The style to check.
   * @return               A boolean being true if the css properties are
   *                       not set.
   */
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT static Result<bool, nsresult>
  HaveComputedCSSEquivalentStyles(const HTMLEditor& aHTMLEditor,
                                  nsIContent& aContent,
                                  const EditorInlineStyle& aStyle);
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT_BOUNDARY static Result<bool, nsresult>
  HaveSpecifiedCSSEquivalentStyles(const HTMLEditor& aHTMLEditor,
                                   nsIContent& aContent,
                                   const EditorInlineStyle& aStyle);

  /**
   * Adds to the node the CSS inline styles equivalent to the HTML style
   * and return the number of CSS properties set by the call.
   *
   * @param aHTMLEditor    [IN} An HTMLEditor instance
   * @param aNode          [IN] A DOM node.
   * @param aStyleToSet    [IN] The style to set.  Can be EditorInlineStyle.
   * @param aValue         [IN] The attribute or style value of aStyleToSet.
   *
   * @return               The number of CSS properties set by the call.
   */
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT static Result<size_t, nsresult>
  SetCSSEquivalentToStyle(WithTransaction aWithTransaction,
                          HTMLEditor& aHTMLEditor,
                          nsStyledElement& aStyledElement,
                          const EditorElementStyle& aStyleToSet,
                          const nsAString* aValue);

  /**
   * Removes from the node the CSS inline styles equivalent to the HTML style.
   *
   * @param aHTMLEditor    [IN} An HTMLEditor instance
   * @param aStyledElement [IN] A DOM Element (must not be null).
   * @param aStyleToRemove [IN] The style to remove.  Can be EditorInlineStyle.
   * @param aAttribute     [IN] An atom to an attribute name or nullptr if
   *                            irrelevant.
   * @param aValue         [IN] The attribute value.
   */
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT static nsresult RemoveCSSEquivalentToStyle(
      WithTransaction aWithTransaction, HTMLEditor& aHTMLEditor,
      nsStyledElement& aStyledElement, const EditorElementStyle& aStyleToRemove,
      const nsAString* aValue);

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

  struct CSSDeclaration {
    nsStaticAtom& mProperty;
    nsString const mValue;
  };

  /**
   * Retrieves the CSS declarations for aEquivTable.
   *
   * @param aEquivTable         The equivalence table.
   * @param aValue              Optional.  If specified, may return only
   *                            matching declarations to this value (depends on
   *                            the style, check how is aInputString of
   *                            nsProcessValueFunc for the details).
   * @param aHandlingFor        What's the purpose of calling this.
   * @param aOutCSSDeclarations [OUT] The array of CSS declarations.
   */
  enum class HandlingFor { GettingStyle, SettingStyle, RemovingStyle };
  static void GetCSSDeclarations(const CSSEquivTable* aEquivTable,
                                 const nsAString* aValue,
                                 HandlingFor aHandlingFor,
                                 nsTArray<CSSDeclaration>& aOutCSSDeclarations);

  /**
   * Retrieves the CSS declarations equivalent to the given aStyle/aValue on
   * aElement.
   *
   * @param aElement            The DOM node.
   * @param aStyle              The style to get equivelent CSS properties and
   *                            values.
   * @param aValue              Optional.  If specified, may return only
   *                            matching declarations to this value (depends on
   *                            the style, check how is aInputString of
   *                            nsProcessValueFunc for the details).
   * @param aHandlingFor        What's the purpose of calling this.
   * @param aOutCSSDeclarations [OUT] The array of CSS declarations.
   */
  static void GetCSSDeclarations(dom::Element& aElement,
                                 const EditorElementStyle& aStyle,
                                 const nsAString* aValue,
                                 HandlingFor aHandlingFor,
                                 nsTArray<CSSDeclaration>& aOutCSSDeclarations);

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
  MOZ_CAN_RUN_SCRIPT static nsresult GetCSSEquivalentTo(
      dom::Element& aElement, const EditorElementStyle& aStyle,
      nsAString& aOutValue, StyleType aStyleType);
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT static Result<bool, nsresult>
  IsCSSEquivalentTo(const HTMLEditor& aHTMLEditor, nsIContent& aContent,
                    const EditorInlineStyle& aStyle, nsAString& aInOutValue,
                    StyleType aStyleType);
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT static Result<bool, nsresult>
  HaveCSSEquivalentStyles(const HTMLEditor& aHTMLEditor, nsIContent& aContent,
                          const EditorInlineStyle& aStyle,
                          StyleType aStyleType);

  [[nodiscard]] MOZ_CAN_RUN_SCRIPT static nsresult RemoveCSSPropertyInternal(
      HTMLEditor& aHTMLEditor, nsStyledElement& aStyledElement,
      nsAtom& aProperty, const nsAString& aPropertyValue,
      bool aSuppressTxn = false);
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT static nsresult SetCSSPropertyInternal(
      HTMLEditor& aHTMLEditor, nsStyledElement& aStyledElement,
      nsAtom& aProperty, const nsAString& aValue, bool aSuppressTxn = false);

  /**
   * Answers true if the given aStyle on aElement or an element whose name is
   * aTagName has a CSS equivalence in this implementation.
   *
   * @param aTagName       [IN] Tag name of an element.
   * @param aElement       [IN] An element.
   * @param aStyle         [IN] The style you want to check.
   * @return               A boolean saying if the tag/attribute has a CSS
   *                       equiv.
   */
  [[nodiscard]] static bool IsCSSEditableStyle(
      const nsAtom& aTagName, const EditorElementStyle& aStyle);
  [[nodiscard]] static bool IsCSSEditableStyle(
      const dom::Element& aElement, const EditorElementStyle& aStyle);

  friend class EditorElementStyle;  // for IsCSSEditableStyle
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

#endif  // #ifndef CSSEditUtils_h
