/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_CSSEditUtils_h
#define mozilla_CSSEditUtils_h

#include "mozilla/ChangeStyleTransaction.h" // for ChangeStyleTransaction
#include "nsCOMPtr.h"               // for already_AddRefed
#include "nsStringFwd.h"
#include "nsTArray.h"               // for nsTArray
#include "nscore.h"                 // for nsAString, nsresult, nullptr

class nsComputedDOMStyle;
class nsAtom;
class nsIContent;
class nsIDOMCSSStyleDeclaration;
class nsIDOMElement;
class nsIDOMNode;
class nsINode;

namespace mozilla {

class HTMLEditor;
namespace dom {
class Element;
} // namespace dom

typedef void (*nsProcessValueFunc)(const nsAString* aInputString,
                                   nsAString& aOutputString,
                                   const char* aDefaultValueString,
                                   const char* aPrependString,
                                   const char* aAppendString);

class CSSEditUtils final
{
public:
  explicit CSSEditUtils(HTMLEditor* aEditor);
  ~CSSEditUtils();

  enum nsCSSEditableProperty
  {
    eCSSEditableProperty_NONE=0,
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

  enum StyleType { eSpecified, eComputed };


  struct CSSEquivTable
  {
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
  bool IsCSSEditableProperty(nsINode* aNode, nsAtom* aProperty,
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
  nsresult SetCSSProperty(dom::Element& aElement, nsAtom& aProperty,
                          const nsAString& aValue, bool aSuppressTxn = false);
  nsresult SetCSSPropertyPixels(dom::Element& aElement,
                                nsAtom& aProperty, int32_t aIntValue);
  nsresult RemoveCSSProperty(dom::Element& aElement,
                             nsAtom& aProperty,
                             const nsAString& aPropertyValue,
                             bool aSuppressTxn = false);

  /**
   * Directly adds/remove a CSS declaration to the STYLE atrribute carried by
   * a given element without going through the transaction manager.
   *
   * @param aElement       [IN] A DOM element.
   * @param aProperty      [IN] A string containing the CSS property to
   *                            set/remove.
   * @param aValue         [IN] A string containing the new value of the CSS
   *                            property.
   */
  nsresult SetCSSProperty(nsIDOMElement* aElement,
                          const nsAString& aProperty,
                          const nsAString& aValue);
  nsresult SetCSSPropertyPixels(nsIDOMElement* aElement,
                                const nsAString& aProperty,
                                int32_t aIntValue);

  /**
   * Gets the specified/computed style value of a CSS property for a given
   * node (or its element ancestor if it is not an element).
   *
   * @param aNode          [IN] A DOM node.
   * @param aProperty      [IN] An atom containing the CSS property to get.
   * @param aPropertyValue [OUT] The retrieved value of the property.
   */
  nsresult GetSpecifiedProperty(nsINode& aNode, nsAtom& aProperty,
                                nsAString& aValue);
  nsresult GetComputedProperty(nsINode& aNode, nsAtom& aProperty,
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
  nsresult RemoveCSSInlineStyle(nsINode& aNode, nsAtom* aProperty,
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
  bool IsCSSInvertible(nsAtom& aProperty, nsAtom* aAttribute);

  /**
   * Get the default browser background color if we need it for
   * GetCSSBackgroundColorState().
   *
   * @param aColor         [OUT] The default color as it is defined in prefs.
   */
  void GetDefaultBackgroundColor(nsAString& aColor);

  /**
   * Get the default length unit used for CSS Indent/Outdent.
   *
   * @param aLengthUnit    [OUT] The default length unit as it is defined in
   *                             prefs.
   */
  void GetDefaultLengthUnit(nsAString & aLengthUnit);

  /**
   * Returns the list of values for the CSS equivalences to
   * the passed HTML style for the passed node.
   *
   * @param aNode          [IN] A DOM node.
   * @param aHTMLProperty  [IN] An atom containing an HTML property.
   * @param aAttribute     [IN] An atom of attribute name or nullptr if
   *                            irrelevant.
   * @param aValueString   [OUT] The list of CSS values.
   * @param aStyleType     [IN] eSpecified or eComputed.
   */
  nsresult GetCSSEquivalentToHTMLInlineStyleSet(nsINode* aNode,
                                                nsAtom* aHTMLProperty,
                                                nsAtom* aAttribute,
                                                nsAString& aValueString,
                                                StyleType aStyleType);

  /**
   * Does the node aNode (or his parent if it is not an element node) carries
   * the CSS equivalent styles to the HTML style for this node ?
   *
   * @param aNode          [IN] A DOM node.
   * @param aHTMLProperty  [IN] An atom containing an HTML property.
   * @param aAttribute     [IN] A pointer/atom to an attribute name or nullptr
   *                            if irrelevant.
   * @param aValueString   [IN/OUT] The attribute value (in) the list of CSS
   *                                values (out).
   * @param aStyleType     [IN] eSpecified or eComputed.
   * @return               A boolean being true if the css properties are
   *                       not same as initial value.
   */
  bool IsCSSEquivalentToHTMLInlineStyleSet(nsINode* aContent,
                                           nsAtom* aProperty,
                                           nsAtom* aAttribute,
                                           nsAString& aValue,
                                           StyleType aStyleType);

  bool IsCSSEquivalentToHTMLInlineStyleSet(nsINode* aContent,
                                           nsAtom* aProperty,
                                           nsAtom* aAttribute,
                                           const nsAString& aValue,
                                           StyleType aStyleType);

  bool IsCSSEquivalentToHTMLInlineStyleSet(nsINode* aContent,
                                           nsAtom* aProperty,
                                           const nsAString* aAttribute,
                                           nsAString& aValue,
                                           StyleType aStyleType);

  bool IsCSSEquivalentToHTMLInlineStyleSet(nsIDOMNode* aNode,
                                           nsAtom* aProperty,
                                           const nsAString* aAttribute,
                                           nsAString& aValue,
                                           StyleType aStyleType);

  /**
   * This is a kind of IsCSSEquivalentToHTMLInlineStyleSet.
   * IsCSSEquivalentToHTMLInlineStyleSet returns whether the properties
   * aren't same as initial value.  But this method returns whether the
   * properties aren't set.
   * If node is <span style="font-weight: normal"/>,
   *  - IsCSSEquivalentToHTMLInlineStyleSet returns false.
   *  - HaveCSSEquivalentStyles returns true.
   *
   * @param aNode          [IN] A DOM node.
   * @param aHTMLProperty  [IN] An atom containing an HTML property.
   * @param aAttribute     [IN] An atom to an attribute name or nullptr
   *                            if irrelevant.
   * @param aStyleType     [IN] eSpecified or eComputed.
   * @return               A boolean being true if the css properties are
   *                       not set.
   */

  bool HaveCSSEquivalentStyles(nsINode& aNode,
                               nsAtom* aProperty,
                               nsAtom* aAttribute,
                               StyleType aStyleType);

  /**
   * Adds to the node the CSS inline styles equivalent to the HTML style
   * and return the number of CSS properties set by the call.
   *
   * @param aNode          [IN] A DOM node.
   * @param aHTMLProperty  [IN] An atom containing an HTML property.
   * @param aAttribute     [IN] A pointer/atom to an attribute name or nullptr
   *                            if irrelevant.
   * @param aValue         [IN] The attribute value.
   * @param aSuppressTransaction [IN] A boolean indicating, when true,
   *                                  that no transaction should be recorded.
   *
   * @return               The number of CSS properties set by the call.
   */
  int32_t SetCSSEquivalentToHTMLStyle(dom::Element* aElement,
                                      nsAtom* aProperty,
                                      nsAtom* aAttribute,
                                      const nsAString* aValue,
                                      bool aSuppressTransaction);
  int32_t SetCSSEquivalentToHTMLStyle(dom::Element* aElement,
                                      nsAtom* aProperty,
                                      const nsAString* aAttribute,
                                      const nsAString* aValue,
                                      bool aSuppressTransaction);
  int32_t SetCSSEquivalentToHTMLStyle(nsIDOMNode* aNode,
                                      nsAtom* aHTMLProperty,
                                      const nsAString* aAttribute,
                                      const nsAString* aValue,
                                      bool aSuppressTransaction);

  /**
   * Removes from the node the CSS inline styles equivalent to the HTML style.
   *
   * @param aNode          [IN] A DOM node.
   * @param aHTMLProperty  [IN] An atom containing an HTML property.
   * @param aAttribute     [IN] A pointer to an attribute name or nullptr if
   *                            irrelevant.
   * @param aValue         [IN] The attribute value.
   * @param aSuppressTransaction [IN] A boolean indicating, when true,
   *                                  that no transaction should be recorded.
   */
  nsresult RemoveCSSEquivalentToHTMLStyle(nsIDOMNode* aNode,
                                          nsAtom* aHTMLProperty,
                                          const nsAString* aAttribute,
                                          const nsAString* aValue,
                                          bool aSuppressTransaction);

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
  nsresult RemoveCSSEquivalentToHTMLStyle(dom::Element* aElement,
                                          nsAtom* aHTMLProperty,
                                          nsAtom* aAttribute,
                                          const nsAString* aValue,
                                          bool aSuppressTransaction);

  /**
   * Parses a "xxxx.xxxxxuuu" string where x is a digit and u an alpha char
   * we need such a parser because
   * nsIDOMCSSStyleDeclaration::GetPropertyCSSValue() is not implemented.
   *
   * @param aString        [IN] Input string to parse.
   * @param aValue         [OUT] Numeric part.
   * @param aUnit          [OUT] Unit part.
   */
  void ParseLength(const nsAString& aString, float* aValue, nsAtom** aUnit);

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
  bool IsCSSPrefChecked();

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
  bool ElementsSameStyle(dom::Element* aFirstNode,
                         dom::Element* aSecondNode);
  bool ElementsSameStyle(nsIDOMNode* aFirstNode, nsIDOMNode* aSecondNode);

  /**
   * Get the specified inline styles (style attribute) for an element.
   *
   * @param aElement        [IN] The element node.
   * @param aCssDecl        [OUT] The CSS declaration corresponding to the
   *                              style attribute.
   * @param aLength         [OUT] The number of declarations in aCssDecl.
   */
  nsresult GetInlineStyles(dom::Element* aElement,
                           nsIDOMCSSStyleDeclaration** aCssDecl,
                           uint32_t* aLength);
  nsresult GetInlineStyles(nsIDOMElement* aElement,
                           nsIDOMCSSStyleDeclaration** aCssDecl,
                           uint32_t* aLength);
private:
  nsresult GetInlineStyles(nsISupports* aElement,
                           nsIDOMCSSStyleDeclaration** aCssDecl,
                           uint32_t* aLength);

public:
  /**
   * Returns aNode itself if it is an element node, or the first ancestors
   * being an element node if aNode is not one itself.
   *
   * @param aNode           [IN] A node
   * @param aElement        [OUT] The deepest element node containing aNode
   *                              (possibly aNode itself)
   */
  dom::Element* GetElementContainerOrSelf(nsINode* aNode);
  already_AddRefed<nsIDOMElement> GetElementContainerOrSelf(nsIDOMNode* aNode);

  /**
   * Gets the computed style for a given element.  Can return null.
   */
  already_AddRefed<nsComputedDOMStyle> GetComputedStyle(dom::Element* aElement);

private:
  /**
   * Retrieves the CSS property atom from an enum.
   *
   * @param aProperty          [IN] The enum value for the property.
   * @param aAtom              [OUT] The corresponding atom.
   */
  void GetCSSPropertyAtom(nsCSSEditableProperty aProperty, nsAtom** aAtom);

  /**
   * Retrieves the CSS declarations equivalent to a HTML style value for
   * a given equivalence table.
   *
   * @param aPropertyArray     [OUT] The array of css properties.
   * @param aValueArray        [OUT] The array of values for the CSS properties
   *                                 above.
   * @param aEquivTable        [IN] The equivalence table.
   * @param aValue             [IN] The HTML style value.
   * @param aGetOrRemoveRequest [IN] A boolean value being true if the call to
   *                                 the current method is made for
   *                                 GetCSSEquivalentToHTMLInlineStyleSet() or
   *                                 RemoveCSSEquivalentToHTMLInlineStyleSet().
   */
  void BuildCSSDeclarations(nsTArray<nsAtom*>& aPropertyArray,
                            nsTArray<nsString>& cssValueArray,
                            const CSSEquivTable* aEquivTable,
                            const nsAString* aValue,
                            bool aGetOrRemoveRequest);

  /**
   * Retrieves the CSS declarations equivalent to the given HTML
   * property/attribute/value for a given node.
   *
   * @param aNode              [IN] The DOM node.
   * @param aHTMLProperty      [IN] An atom containing an HTML property.
   * @param aAttribute         [IN] An atom to an attribute name or nullptr
   *                                if irrelevant
   * @param aValue             [IN] The attribute value.
   * @param aPropertyArray     [OUT] The array of CSS properties.
   * @param aValueArray        [OUT] The array of values for the CSS properties
   *                                 above.
   * @param aGetOrRemoveRequest [IN] A boolean value being true if the call to
   *                                 the current method is made for
   *                                 GetCSSEquivalentToHTMLInlineStyleSet() or
   *                                 RemoveCSSEquivalentToHTMLInlineStyleSet().
   */
  void GenerateCSSDeclarationsFromHTMLStyle(dom::Element* aNode,
                                            nsAtom* aHTMLProperty,
                                            nsAtom* aAttribute,
                                            const nsAString* aValue,
                                            nsTArray<nsAtom*>& aPropertyArray,
                                            nsTArray<nsString>& aValueArray,
                                            bool aGetOrRemoveRequest);

  /**
   * Back-end for GetSpecifiedProperty and GetComputedProperty.
   *
   * @param aNode               [IN] A DOM node.
   * @param aProperty           [IN] A CSS property.
   * @param aValue              [OUT] The retrieved value for this property.
   * @param aStyleType          [IN] eSpecified or eComputed.
   */
  nsresult GetCSSInlinePropertyBase(nsINode* aNode, nsAtom* aProperty,
                                    nsAString& aValue, StyleType aStyleType);

private:
  HTMLEditor* mHTMLEditor;
  bool mIsCSSPrefChecked;
};

#define NS_EDITOR_INDENT_INCREMENT_IN        0.4134f
#define NS_EDITOR_INDENT_INCREMENT_CM        1.05f
#define NS_EDITOR_INDENT_INCREMENT_MM        10.5f
#define NS_EDITOR_INDENT_INCREMENT_PT        29.76f
#define NS_EDITOR_INDENT_INCREMENT_PC        2.48f
#define NS_EDITOR_INDENT_INCREMENT_EM        3
#define NS_EDITOR_INDENT_INCREMENT_EX        6
#define NS_EDITOR_INDENT_INCREMENT_PX        40
#define NS_EDITOR_INDENT_INCREMENT_PERCENT   4

} // namespace mozilla

#endif // #ifndef mozilla_CSSEditUtils_h
