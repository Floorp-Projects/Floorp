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
 *   Original Author: Daniel Glazman <glazman@netscape.com>
 *   Ms2ger <ms2ger@gmail.com>
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

#ifndef nsHTMLCSSUtils_h__
#define nsHTMLCSSUtils_h__

#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsTArray.h"
#include "nsIDOMNode.h"
#include "nsIDOMElement.h"
#include "nsIHTMLEditor.h"
#include "ChangeCSSInlineStyleTxn.h"
#include "nsEditProperty.h"
#include "nsIDOMCSSStyleDeclaration.h"

#define SPECIFIED_STYLE_TYPE    1
#define COMPUTED_STYLE_TYPE     2

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
    * @param aValue         [IN] an optional string containing the attribute's
    *                            HTML value -- this matters for <font size>,
    *                            since size=7 has no CSS equivalent.  Make sure
    *                            you pass the HTML value (e.g. "4"), not the
    *                            CSS value (e.g. "large").
    */
  bool IsCSSEditableProperty(nsIContent* aNode, nsIAtom* aProperty,
                             const nsAString* aAttribute,
                             const nsAString* aValue = nsnull);
  bool IsCSSEditableProperty(nsIDOMNode* aNode, nsIAtom* aProperty,
                             const nsAString* aAttribute,
                             const nsAString* aValue = nsnull);

  /** adds/remove a CSS declaration to the STYLE atrribute carried by a given element
    *
    * @param aElement       [IN] a DOM element
    * @param aProperty      [IN] an atom containing the CSS property to set
    * @param aValue         [IN] a string containing the value of the CSS property
    * @param aSuppressTransaction [IN] a boolean indicating, when true,
    *                                  that no transaction should be recorded
    */
  nsresult    SetCSSProperty(nsIDOMElement * aElement, nsIAtom * aProperty,
                             const nsAString & aValue,
                             bool aSuppressTransaction);
  nsresult    SetCSSPropertyPixels(nsIDOMElement *aElement, nsIAtom *aProperty,
                                   PRInt32 aIntValue, bool aSuppressTxn);
  nsresult    RemoveCSSProperty(nsIDOMElement * aElement, nsIAtom * aProperty,
                                const nsAString & aPropertyValue, bool aSuppressTransaction);

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
                                   PRInt32 aIntValue);

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
  bool        IsCSSInvertable(nsIAtom * aProperty, const nsAString * aAttribute);

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
    * @param aAttribute     [IN] a pointer to an attribute name or nsnull if irrelevant
    * @param aValueString   [OUT] the list of css values
    * @param aStyleType     [IN] SPECIFIED_STYLE_TYPE to query the specified style values
                                 COMPUTED_STYLE_TYPE  to query the computed style values
    */
  nsresult    GetCSSEquivalentToHTMLInlineStyleSet(nsINode* aNode,
                                                   nsIAtom * aHTMLProperty,
                                                   const nsAString * aAttribute,
                                                   nsAString & aValueString,
                                                   PRUint8 aStyleType);

  /** Does the node aNode (or his parent if it is not an element node) carries
    * the CSS equivalent styles to the HTML style for this node ?
    *
    * @param aNode          [IN] a DOM node
    * @param aHTMLProperty  [IN] an atom containing an HTML property
    * @param aAttribute     [IN] a pointer to an attribute name or nsnull if irrelevant
    * @param aIsSet         [OUT] a boolean being true if the css properties are set
    * @param aValueString   [IN/OUT] the attribute value (in) the list of css values (out)
    * @param aStyleType     [IN] SPECIFIED_STYLE_TYPE to query the specified style values
                                 COMPUTED_STYLE_TYPE  to query the computed style values
    */
  nsresult    IsCSSEquivalentToHTMLInlineStyleSet(nsIDOMNode * aNode,
                                                  nsIAtom * aHTMLProperty,
                                                  const nsAString * aAttribute,
                                                  bool & aIsSet,
                                                  nsAString & aValueString,
                                                  PRUint8 aStyleType);

  /** Adds to the node the CSS inline styles equivalent to the HTML style
    * and return the number of CSS properties set by the call
    *
    * @param aNode          [IN] a DOM node
    * @param aHTMLProperty  [IN] an atom containing an HTML property
    * @param aAttribute     [IN] a pointer to an attribute name or nsnull if irrelevant
    * @param aValue         [IN] the attribute value
    * @param aCount         [OUT] the number of CSS properties set by the call
    * @param aSuppressTransaction [IN] a boolean indicating, when true,
    *                                  that no transaction should be recorded
    */
  nsresult    SetCSSEquivalentToHTMLStyle(nsIDOMNode * aNode,
                                          nsIAtom * aHTMLProperty,
                                          const nsAString * aAttribute,
                                          const nsAString * aValue,
                                          PRInt32 * aCount,
                                          bool aSuppressTransaction);

  /** removes from the node the CSS inline styles equivalent to the HTML style
    *
    * @param aNode          [IN] a DOM node
    * @param aHTMLProperty  [IN] an atom containing an HTML property
    * @param aAttribute     [IN] a pointer to an attribute name or nsnull if irrelevant
    * @param aValue         [IN] the attribute value
    * @param aSuppressTransaction [IN] a boolean indicating, when true,
    *                                  that no transaction should be recorded
    */
  nsresult    RemoveCSSEquivalentToHTMLStyle(nsIDOMNode * aNode,
                                             nsIAtom *aHTMLProperty,
                                             const nsAString *aAttribute,
                                             const nsAString *aValue,
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
  nsresult    SetCSSEnabled(bool aIsCSSPrefChecked);

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
  bool ElementsSameStyle(nsIDOMNode *aFirstNode, nsIDOMNode *aSecondNode);

  /** get the specified inline styles (style attribute) for an element
    *
    * @param aElement        [IN] the element node
    * @param aCssDecl        [OUT] the CSS declaration corresponding to the style attr
    * @param aLength         [OUT] the number of declarations in aCssDecl
    */
  nsresult GetInlineStyles(nsIDOMElement * aElement, nsIDOMCSSStyleDeclaration ** aCssDecl,
                           PRUint32 * aLength);

  /** returns aNode itself if it is an element node, or the first ancestors being an element
    * node if aNode is not one itself
    *
    * @param aNode           [IN] a node
    * @param aElement        [OUT] the deepest element node containing aNode (possibly aNode itself)
    */
  mozilla::dom::Element* GetElementContainerOrSelf(nsINode* aNode);
  already_AddRefed<nsIDOMElement> GetElementContainerOrSelf(nsIDOMNode* aNode);

  /**
   * Gets the default Window for a given node.
   *
   * @param aNode    the node we want the default Window for
   * @param aWindow  [OUT] the default Window
   */
  nsresult GetDefaultViewCSS(nsINode* aNode, nsIDOMWindow** aWindow);
  nsresult GetDefaultViewCSS(nsIDOMNode* aNode, nsIDOMWindow** aWindow);


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
    * @param aAttribute         [IN] a pointer to an attribute name or nsnull if irrelevant
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

  /** creates a Transaction for setting or removing a css property
    *
    * @param aElement           [IN] a DOM element
    * @param aProperty          [IN] a CSS property
    * @param aValue             [IN] the value to remove for this CSS property or the empty string if irrelevant
    * @param aTxn               [OUT] the created transaction
    * @param aRemoveProperty    [IN] true if we create a "remove" transaction, false for a "set"
    */
  nsresult    CreateCSSPropertyTxn(nsIDOMElement * aElement, 
                                   nsIAtom * aProperty,
                                   const nsAString & aValue,
                                   ChangeCSSInlineStyleTxn ** aTxn,
                                   bool aRemoveProperty);

  /** back-end for GetSpecifiedProperty and GetComputedProperty
   *
   * @param aNode               [IN] a DOM node
   * @param aProperty           [IN] a CSS property
   * @param aValue              [OUT] the retrieved value for this property
   * @param aWindow             [IN] the window we need in case we query computed styles
   * @param aStyleType          [IN] SPECIFIED_STYLE_TYPE to query the specified style values
   *                                 COMPUTED_STYLE_TYPE  to query the computed style values
   */
  nsresult GetCSSInlinePropertyBase(nsINode* aNode, nsIAtom* aProperty,
                                    nsAString& aValue, nsIDOMWindow* aWindow,
                                    PRUint8 aStyleType);
  nsresult GetCSSInlinePropertyBase(nsIDOMNode* aNode, nsIAtom* aProperty,
                                    nsAString& aValue, nsIDOMWindow* aWindow,
                                    PRUint8 aStyleType);


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
