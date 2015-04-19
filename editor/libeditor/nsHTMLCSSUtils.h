/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsHTMLCSSUtils_h__
#define nsHTMLCSSUtils_h__

#include "ChangeStyleTxn.h"             // for ChangeStyleTxn::EChangeType
#include "nsCOMPtr.h"                   // for already_AddRefed
#include "nsTArray.h"                   // for nsTArray
#include "nscore.h"                     // for nsAString, nsresult, nullptr

class nsComputedDOMStyle;
class nsIAtom;
class nsIContent;
class nsIDOMCSSStyleDeclaration;
class nsIDOMElement;
class nsIDOMNode;
class nsINode;
class nsString;
namespace mozilla {
namespace dom {
class Element;
}  // namespace dom
}  // namespace mozilla

class nsHTMLEditor;
class nsIDOMWindow;

typedef void (*nsProcessValueFunc)(const nsAString * aInputString, nsAString & aOutputString,
                                   const char * aDefaultValueString,
                                   const char * aPrependString, const char* aAppendString);

class nsHTMLCSSUtils
{
public:
  explicit nsHTMLCSSUtils(nsHTMLEditor* aEditor);
  ~nsHTMLCSSUtils();

  enum nsCSSEditableProperty {
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


  struct CSSEquivTable {
    nsCSSEditableProperty cssProperty;
    nsProcessValueFunc processValueFunctor;
    const char * defaultValue;
    const char * prependValue;
    const char * appendValue;
    bool gettable;
    bool caseSensitiveValue;
  };

  /** answers true if the given combination element_name/attribute_name
    * has a CSS equivalence in this implementation
    *
    * @return               a boolean saying if the tag/attribute has a css equiv
    * @param aNode          [IN] a DOM node
    * @param aProperty      [IN] an atom containing a HTML tag name
    * @param aAttribute     [IN] a string containing the name of a HTML
    *                            attribute carried by the element above
    */
  bool IsCSSEditableProperty(nsINode* aNode, nsIAtom* aProperty,
                             const nsAString* aAttribute);

  /** adds/remove a CSS declaration to the STYLE atrribute carried by a given element
    *
    * @param aElement       [IN] a DOM element
    * @param aProperty      [IN] an atom containing the CSS property to set
    * @param aValue         [IN] a string containing the value of the CSS property
    * @param aSuppressTransaction [IN] a boolean indicating, when true,
    *                                  that no transaction should be recorded
    */
  nsresult SetCSSProperty(mozilla::dom::Element& aElement, nsIAtom& aProperty,
                          const nsAString& aValue, bool aSuppressTxn = false);
  nsresult SetCSSPropertyPixels(mozilla::dom::Element& aElement,
                                nsIAtom& aProperty, int32_t aIntValue);
  nsresult RemoveCSSProperty(mozilla::dom::Element& aElement,
                             nsIAtom& aProperty,
                             const nsAString& aPropertyValue,
                             bool aSuppressTxn = false);

  /** directly adds/remove a CSS declaration to the STYLE atrribute carried by
    * a given element without going through the txn manager
    *
    * @param aElement       [IN] a DOM element
    * @param aProperty      [IN] a string containing the CSS property to set/remove
    * @param aValue         [IN] a string containing the new value of the CSS property
    */
  nsresult    SetCSSProperty(nsIDOMElement * aElement,
                             const nsAString & aProperty,
                             const nsAString & aValue);
  nsresult    SetCSSPropertyPixels(nsIDOMElement * aElement,
                                   const nsAString & aProperty,
                                   int32_t aIntValue);

  /** gets the specified/computed style value of a CSS property for a given node (or its element
    * ancestor if it is not an element)
    *
    * @param aNode          [IN] a DOM node
    * @param aProperty      [IN] an atom containing the CSS property to get
    * @param aPropertyValue [OUT] the retrieved value of the property
    */
  nsresult    GetSpecifiedProperty(nsIDOMNode *aNode, nsIAtom *aProperty,
                                   nsAString & aValue);
  nsresult    GetComputedProperty(nsIDOMNode *aNode, nsIAtom *aProperty,
                                  nsAString & aValue);

  /** Removes a CSS property from the specified declarations in STYLE attribute
   ** and removes the node if it is an useless span
   *
   * @param aNode           [IN] the specific node we want to remove a style from
   * @param aProperty       [IN] the CSS property atom to remove
   * @param aPropertyValue  [IN] the value of the property we have to rremove if the property
   *                             accepts more than one value
   */
  nsresult    RemoveCSSInlineStyle(nsIDOMNode * aNode, nsIAtom * aProperty, const nsAString & aPropertyValue);

   /** Answers true is the property can be removed by setting a "none" CSS value
     * on a node
     *
     * @return              a boolean saying if the property can be remove by setting a "none" value
     * @param aProperty     [IN] an atom containing a CSS property
     * @param aAttribute    [IN] pointer to an attribute name or null if this information is irrelevant
     */
  bool        IsCSSInvertible(nsIAtom& aProperty, const nsAString* aAttribute);

  /** Get the default browser background color if we need it for GetCSSBackgroundColorState
    *
    * @param aColor         [OUT] the default color as it is defined in prefs
    */
  void        GetDefaultBackgroundColor(nsAString & aColor);

  /** Get the default length unit used for CSS Indent/Outdent
    *
    * @param aLengthUnit    [OUT] the default length unit as it is defined in prefs
    */
  void        GetDefaultLengthUnit(nsAString & aLengthUnit);

  /** returns the list of values for the CSS equivalences to
    * the passed HTML style for the passed node
    *
    * @param aNode          [IN] a DOM node
    * @param aHTMLProperty  [IN] an atom containing an HTML property
    * @param aAttribute     [IN] a pointer to an attribute name or nullptr if irrelevant
    * @param aValueString   [OUT] the list of css values
    * @param aStyleType     [IN] eSpecified or eComputed
    */
  nsresult    GetCSSEquivalentToHTMLInlineStyleSet(nsINode* aNode,
                                                   nsIAtom * aHTMLProperty,
                                                   const nsAString * aAttribute,
                                                   nsAString & aValueString,
                                                   StyleType aStyleType);

  /** Does the node aNode (or his parent if it is not an element node) carries
    * the CSS equivalent styles to the HTML style for this node ?
    *
    * @param aNode          [IN] a DOM node
    * @param aHTMLProperty  [IN] an atom containing an HTML property
    * @param aAttribute     [IN] a pointer to an attribute name or nullptr if irrelevant
    * @param aIsSet         [OUT] a boolean being true if the css properties are set
    * @param aValueString   [IN/OUT] the attribute value (in) the list of css values (out)
    * @param aStyleType     [IN] eSpecified or eComputed
    *
    * The nsIContent variant returns aIsSet instead of using an out parameter.
    */
  bool IsCSSEquivalentToHTMLInlineStyleSet(nsINode* aContent,
                                           nsIAtom* aProperty,
                                           const nsAString* aAttribute,
                                           const nsAString& aValue,
                                           StyleType aStyleType);

  bool IsCSSEquivalentToHTMLInlineStyleSet(nsINode* aContent,
                                           nsIAtom* aProperty,
                                           const nsAString* aAttribute,
                                           nsAString& aValue,
                                           StyleType aStyleType);

  nsresult    IsCSSEquivalentToHTMLInlineStyleSet(nsIDOMNode * aNode,
                                                  nsIAtom * aHTMLProperty,
                                                  const nsAString * aAttribute,
                                                  bool & aIsSet,
                                                  nsAString & aValueString,
                                                  StyleType aStyleType);

  /** Adds to the node the CSS inline styles equivalent to the HTML style
    * and return the number of CSS properties set by the call
    *
    * @param aNode          [IN] a DOM node
    * @param aHTMLProperty  [IN] an atom containing an HTML property
    * @param aAttribute     [IN] a pointer to an attribute name or nullptr if irrelevant
    * @param aValue         [IN] the attribute value
    * @param aCount         [OUT] the number of CSS properties set by the call
    * @param aSuppressTransaction [IN] a boolean indicating, when true,
    *                                  that no transaction should be recorded
    *
    * aCount is returned by the dom::Element variant instead of being an out
    * parameter.
    */
  int32_t     SetCSSEquivalentToHTMLStyle(mozilla::dom::Element* aElement,
                                          nsIAtom* aProperty,
                                          const nsAString* aAttribute,
                                          const nsAString* aValue,
                                          bool aSuppressTransaction);
  nsresult    SetCSSEquivalentToHTMLStyle(nsIDOMNode * aNode,
                                          nsIAtom * aHTMLProperty,
                                          const nsAString * aAttribute,
                                          const nsAString * aValue,
                                          int32_t * aCount,
                                          bool aSuppressTransaction);

  /** removes from the node the CSS inline styles equivalent to the HTML style
    *
    * @param aNode          [IN] a DOM node
    * @param aHTMLProperty  [IN] an atom containing an HTML property
    * @param aAttribute     [IN] a pointer to an attribute name or nullptr if irrelevant
    * @param aValue         [IN] the attribute value
    * @param aSuppressTransaction [IN] a boolean indicating, when true,
    *                                  that no transaction should be recorded
    */
  nsresult    RemoveCSSEquivalentToHTMLStyle(nsIDOMNode * aNode,
                                             nsIAtom *aHTMLProperty,
                                             const nsAString *aAttribute,
                                             const nsAString *aValue,
                                             bool aSuppressTransaction);
  /** removes from the node the CSS inline styles equivalent to the HTML style
    *
    * @param aElement       [IN] a DOM Element (must not be null)
    * @param aHTMLProperty  [IN] an atom containing an HTML property
    * @param aAttribute     [IN] a pointer to an attribute name or nullptr if irrelevant
    * @param aValue         [IN] the attribute value
    * @param aSuppressTransaction [IN] a boolean indicating, when true,
    *                                  that no transaction should be recorded
    */
  nsresult    RemoveCSSEquivalentToHTMLStyle(mozilla::dom::Element* aElement,
                                             nsIAtom* aHTMLProperty,
                                             const nsAString* aAttribute,
                                             const nsAString* aValue,
                                             bool aSuppressTransaction);

  /** parses a "xxxx.xxxxxuuu" string where x is a digit and u an alpha char
    * we need such a parser because nsIDOMCSSStyleDeclaration::GetPropertyCSSValue() is not
    * implemented
    *
    * @param aString        [IN] input string to parse
    * @param aValue         [OUT] numeric part
    * @param aUnit          [OUT] unit part
    */
  void        ParseLength(const nsAString & aString, float * aValue, nsIAtom ** aUnit);

  /** sets the mIsCSSPrefChecked private member ; used as callback from observer when
    * the css pref state is changed
    *
    * @param aIsCSSPrefChecked [IN] the new boolean state for the pref
    */
  void        SetCSSEnabled(bool aIsCSSPrefChecked);

  /** retrieves the mIsCSSPrefChecked private member, true if the css pref is checked,
    * false if it is not
    *
    * @return                 the boolean value of the css pref
    */
  bool        IsCSSPrefChecked();

  /** ElementsSameStyle compares two elements and checks if they have the same
    * specified CSS declarations in the STYLE attribute 
    * The answer is always false if at least one of them carries an ID or a class
    *
    * @return                     true if the two elements are considered to have same styles
    * @param aFirstNode           [IN] a DOM node
    * @param aSecondNode          [IN] a DOM node
    */
  bool ElementsSameStyle(mozilla::dom::Element* aFirstNode,
                         mozilla::dom::Element* aSecondNode);
  bool ElementsSameStyle(nsIDOMNode *aFirstNode, nsIDOMNode *aSecondNode);

  /** get the specified inline styles (style attribute) for an element
    *
    * @param aElement        [IN] the element node
    * @param aCssDecl        [OUT] the CSS declaration corresponding to the style attr
    * @param aLength         [OUT] the number of declarations in aCssDecl
    */
  nsresult GetInlineStyles(mozilla::dom::Element* aElement,
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
  /** returns aNode itself if it is an element node, or the first ancestors being an element
    * node if aNode is not one itself
    *
    * @param aNode           [IN] a node
    * @param aElement        [OUT] the deepest element node containing aNode (possibly aNode itself)
    */
  mozilla::dom::Element* GetElementContainerOrSelf(nsINode* aNode);
  already_AddRefed<nsIDOMElement> GetElementContainerOrSelf(nsIDOMNode* aNode);

  /**
   * Gets the computed style for a given element.  Can return null.
   */
  already_AddRefed<nsComputedDOMStyle>
    GetComputedStyle(mozilla::dom::Element* aElement);


private:

  /** retrieves the css property atom from an enum
    *
    * @param aProperty          [IN] the enum value for the property
    * @param aAtom              [OUT] the corresponding atom
    */
  void  GetCSSPropertyAtom(nsCSSEditableProperty aProperty, nsIAtom ** aAtom);

  /** retrieves the CSS declarations equivalent to a HTML style value for
    * a given equivalence table
    *
    * @param aPropertyArray     [OUT] the array of css properties
    * @param aValueArray        [OUT] the array of values for the css properties above
    * @param aEquivTable        [IN] the equivalence table
    * @param aValue             [IN] the HTML style value
    * @param aGetOrRemoveRequest [IN] a boolean value being true if the call to the current method
    *                                 is made for GetCSSEquivalentToHTMLInlineStyleSet or
    *                                 RemoveCSSEquivalentToHTMLInlineStyleSet
    */

  void      BuildCSSDeclarations(nsTArray<nsIAtom*> & aPropertyArray,
                                 nsTArray<nsString> & cssValueArray,
                                 const CSSEquivTable * aEquivTable,
                                 const nsAString * aValue,
                                 bool aGetOrRemoveRequest);

  /** retrieves the CSS declarations equivalent to the given HTML property/attribute/value
    * for a given node
    *
    * @param aNode              [IN] the DOM node
    * @param aHTMLProperty      [IN] an atom containing an HTML property
    * @param aAttribute         [IN] a pointer to an attribute name or nullptr if irrelevant
    * @param aValue             [IN] the attribute value
    * @param aPropertyArray     [OUT] the array of css properties
    * @param aValueArray        [OUT] the array of values for the css properties above
    * @param aGetOrRemoveRequest [IN] a boolean value being true if the call to the current method
    *                                 is made for GetCSSEquivalentToHTMLInlineStyleSet or
    *                                 RemoveCSSEquivalentToHTMLInlineStyleSet
    */
  void      GenerateCSSDeclarationsFromHTMLStyle(mozilla::dom::Element* aNode,
                                                 nsIAtom* aHTMLProperty,
                                                 const nsAString* aAttribute,
                                                 const nsAString* aValue,
                                                 nsTArray<nsIAtom*>& aPropertyArray,
                                                 nsTArray<nsString>& aValueArray,
                                                 bool aGetOrRemoveRequest);

  /** Creates a Transaction for setting or removing a CSS property.  Never
    * returns null.
    *
    * @param aElement           [IN] a DOM element
    * @param aProperty          [IN] a CSS property
    * @param aValue             [IN] the value to set for this CSS property
    * @param aChangeType        [IN] eSet to set, eRemove to remove
    */
  already_AddRefed<mozilla::dom::ChangeStyleTxn>
  CreateCSSPropertyTxn(mozilla::dom::Element& aElement,
      nsIAtom& aProperty, const nsAString& aValue,
      mozilla::dom::ChangeStyleTxn::EChangeType aChangeType);

  /** back-end for GetSpecifiedProperty and GetComputedProperty
   *
   * @param aNode               [IN] a DOM node
   * @param aProperty           [IN] a CSS property
   * @param aValue              [OUT] the retrieved value for this property
   * @param aStyleType          [IN] eSpecified or eComputed
   */
  nsresult GetCSSInlinePropertyBase(nsINode* aNode, nsIAtom* aProperty,
                                    nsAString& aValue, StyleType aStyleType);
  nsresult GetCSSInlinePropertyBase(nsIDOMNode* aNode, nsIAtom* aProperty,
                                    nsAString& aValue, StyleType aStyleType);


private:
  nsHTMLEditor            *mHTMLEditor;
  bool                    mIsCSSPrefChecked; 
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

#endif /* nsHTMLCSSUtils_h__ */
