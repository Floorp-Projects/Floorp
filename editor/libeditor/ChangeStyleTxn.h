/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ChangeStyleTxn_h__
#define ChangeStyleTxn_h__

#include "EditTxn.h"                      // base class
#include "nsCOMPtr.h"                     // nsCOMPtr members
#include "nsCycleCollectionParticipant.h" // various macros
#include "nsString.h"                     // nsString members

class nsAString;
class nsIAtom;

namespace mozilla {
namespace dom {
class Element;

/**
 * A transaction that changes the value of a CSS inline style of a content
 * node.  This transaction covers add, remove, and change a property's value.
 */
class ChangeStyleTxn : public EditTxn
{
public:
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(ChangeStyleTxn, EditTxn)

  NS_DECL_ISUPPORTS_INHERITED

  NS_DECL_EDITTXN

  NS_IMETHOD RedoTransaction() override;

  enum EChangeType { eSet, eRemove };

  /** @param aNode           [IN] the node whose style attribute will be changed
    * @param aProperty       [IN] the name of the property to change
    * @param aValue          [IN] new value for aProperty, or value to remove
    * @param aChangeType     [IN] whether to set or remove
    */
  ChangeStyleTxn(Element& aElement, nsIAtom& aProperty,
                 const nsAString& aValue, EChangeType aChangeType);

  /** Returns true if the list of white-space separated values contains aValue
    *
    * @return                true if the value is in the list of values
    * @param aValueList      [IN] a list of white-space separated values
    * @param aValue          [IN] the value to look for in the list
    */
  static bool ValueIncludes(const nsAString& aValueList,
                            const nsAString& aValue);

private:
  ~ChangeStyleTxn();

  /** Adds the value aNewValue to list of white-space separated values aValues
    *
    * @param aValues         [IN/OUT] a list of wite-space separated values
    * @param aNewValue       [IN] a value this code adds to aValues if it is not already in
    */
  void AddValueToMultivalueProperty(nsAString& aValues,
                                    const nsAString& aNewValue);

  /** Returns true if the property accepts more than one value
    *
    * @return                true if the property accepts more than one value
    * @param aCSSProperty    [IN] the CSS property
    */
  bool AcceptsMoreThanOneValue(nsIAtom& aCSSProperty);

  /** Remove a value from a list of white-space separated values
    * @param aValues         [IN] a list of white-space separated values
    * @param aRemoveValue    [IN] the value to remove from the list
    */
  void RemoveValueFromListOfValues(nsAString& aValues,
                                   const nsAString& aRemoveValue);

  /** If the boolean is true and if the value is not the empty string,
    * set the property in the transaction to that value; if the value
    * is empty, remove the property from element's styles. If the boolean
    * is false, just remove the style attribute.
    */
  nsresult SetStyle(bool aAttributeWasSet, nsAString& aValue);

  /** The element to operate upon */
  nsCOMPtr<Element> mElement;

  /** The CSS property to change */
  nsCOMPtr<nsIAtom> mProperty;

  /** The value to set the property to (ignored if mRemoveProperty==true) */
  nsString mValue;

  /** true if the operation is to remove mProperty from mElement */
  bool mRemoveProperty;

  /** The value to set the property to for undo */
  nsString mUndoValue;
  /** The value to set the property to for redo */
  nsString mRedoValue;
  /** True if the style attribute was present and not empty before DoTransaction */
  bool mUndoAttributeWasSet;
  /** True if the style attribute is present and not empty after DoTransaction */
  bool mRedoAttributeWasSet;
};

} // namespace dom
} // namespace mozilla

#endif
