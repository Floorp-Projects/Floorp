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

#ifndef ChangeCSSInlineStyleTxn_h__
#define ChangeCSSInlineStyleTxn_h__

#include "EditTxn.h"
#include "nsCOMPtr.h"
#include "nsIDOMElement.h"
#include "nsIEditor.h"

/**
 * A transaction that changes the value of a CSS inline style of a content node. 
 * This transaction covers add, remove, and change a property's value.
 */
class ChangeCSSInlineStyleTxn : public EditTxn
{
public:
  /** Initialize the transaction.
    * @param aEditor         [IN] the object providing core editing operations
    * @param aNode           [IN] the node whose style attribute will be changed
    * @param aProperty       [IN] the name of the property to change
    * @param aValue          [IN] the new value for aProperty, if aRemoveProperty is false
    * @param aRemoveProperty [IN] if PR_TRUE, remove aProperty from style attribute
    */
  NS_IMETHOD Init(nsIEditor      * aEditor,
                  nsIDOMElement  * aElement,
                  nsIAtom        * aProperty,
                  const nsAString & aValue,
                  bool aRemoveProperty);

  /** returns true if the list of white-space separated values contains aValue
    *
    * @return                true if the value is in the list of values
    * @param aValueList      [IN] a list of white-space separated values
    * @param aValue          [IN] the value to look for in the list
    * @param aCaseSensitive  [IN] a boolean being true if a case-sensitive search is needed
    */
  static bool ValueIncludes(const nsAString & aValueList, const nsAString & aValue, bool aCaseSensitive);

  /** adds the value aNewValue to the list of white-space separated values aValues
    *
    * @param aValues         [IN/OUT] a list of wite-space separated values
    * @param aNewValue       [IN] a value this code adds to aValues if it is not already in
    */
  NS_IMETHOD AddValueToMultivalueProperty(nsAString & aValues, const nsAString  & aNewValue);

  ChangeCSSInlineStyleTxn();

private:
  /** returns true if the property accepts more than one value
    *
    * @return                true if the property accepts more than one value
    * @param aCSSProperty    [IN] the CSS property
    */
  bool AcceptsMoreThanOneValue(nsIAtom * aCSSProperty);

  /** remove a value from a list of white-space separated values
    * @param aValues         [IN] a list of white-space separated values
    * @param aRemoveValue    [IN] the value to remove from the list
    */
  void   RemoveValueFromListOfValues(nsAString & aValues, const nsAString  & aRemoveValue);

  /** If the boolean is true and if the value is not the empty string,
    * set the property in the transaction to that value; if the value
    * is empty, remove the property from element's styles. If the boolean
    * is false, just remove the style attribute.
    */
  nsresult SetStyle(bool aAttributeWasSet, nsAString & aValue);

public:
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(ChangeCSSInlineStyleTxn, EditTxn)
  NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr);

  NS_DECL_EDITTXN

  NS_IMETHOD RedoTransaction();

protected:

  /** the editor that created this transaction */
  nsIEditor *mEditor;
  
  /** the element to operate upon */
  nsCOMPtr<nsIDOMElement> mElement;
  
  /** the CSS property to change */
  nsIAtom *mProperty;

  /** the value to set the property to (ignored if mRemoveProperty==PR_TRUE) */
  nsString mValue;

  /** the value to set the property to for undo */
  nsString mUndoValue;
  /** the value to set the property to for redo */
  nsString mRedoValue;
  /** true if the style attribute was present and not empty before DoTransaction */
  bool     mUndoAttributeWasSet;
  /** true if the style attribute is present and not empty after DoTransaction */
  bool     mRedoAttributeWasSet;

  /** PR_TRUE if the operation is to remove mProperty from mElement */
  bool     mRemoveProperty;
};

#endif
