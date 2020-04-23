/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ChangeStyleTransaction_h
#define mozilla_ChangeStyleTransaction_h

#include "mozilla/EditTransactionBase.h"   // base class
#include "nsCOMPtr.h"                      // nsCOMPtr members
#include "nsCycleCollectionParticipant.h"  // various macros
#include "nsString.h"                      // nsString members

class nsAtom;
class nsStyledElement;

namespace mozilla {

namespace dom {
class Element;
}  // namespace dom

/**
 * A transaction that changes the value of a CSS inline style of a content
 * node.  This transaction covers add, remove, and change a property's value.
 */
class ChangeStyleTransaction final : public EditTransactionBase {
 protected:
  ChangeStyleTransaction(nsStyledElement& aStyledElement, nsAtom& aProperty,
                         const nsAString& aValue, bool aRemove);

 public:
  /**
   * Creates a change style transaction.  This never returns nullptr.
   *
   * @param aStyledElement  The node whose style attribute will be changed.
   * @param aProperty       The name of the property to change.
   * @param aValue          New value for aProperty.
   */
  static already_AddRefed<ChangeStyleTransaction> Create(
      nsStyledElement& aStyledElement, nsAtom& aProperty,
      const nsAString& aValue);

  /**
   * Creates a change style transaction.  This never returns nullptr.
   *
   * @param aStyledElement  The node whose style attribute will be changed.
   * @param aProperty       The name of the property to change.
   * @param aValue          The value to remove from aProperty.
   */
  static already_AddRefed<ChangeStyleTransaction> CreateToRemove(
      nsStyledElement& aStyledElement, nsAtom& aProperty,
      const nsAString& aValue);

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(ChangeStyleTransaction,
                                           EditTransactionBase)

  NS_DECL_ISUPPORTS_INHERITED

  NS_DECL_EDITTRANSACTIONBASE
  NS_DECL_EDITTRANSACTIONBASE_GETASMETHODS_OVERRIDE(ChangeStyleTransaction)

  MOZ_CAN_RUN_SCRIPT NS_IMETHOD RedoTransaction() override;

  /**
   * Returns true if the list of white-space separated values contains aValue
   *
   * @param aValueList      [IN] a list of white-space separated values
   * @param aValue          [IN] the value to look for in the list
   * @return                true if the value is in the list of values
   */
  static bool ValueIncludes(const nsAString& aValueList,
                            const nsAString& aValue);

 private:
  virtual ~ChangeStyleTransaction() = default;

  /*
   * Adds the value aNewValue to list of white-space separated values aValues.
   *
   * @param aValues         [IN/OUT] a list of wite-space separated values
   * @param aNewValue       [IN] a value this code adds to aValues if it is not
   *                        already in
   */
  void AddValueToMultivalueProperty(nsAString& aValues,
                                    const nsAString& aNewValue);

  /**
   * Returns true if the property accepts more than one value.
   *
   * @param aCSSProperty    [IN] the CSS property
   * @return                true if the property accepts more than one value
   */
  bool AcceptsMoreThanOneValue(nsAtom& aCSSProperty);

  /**
   * Remove a value from a list of white-space separated values.
   * @param aValues         [IN] a list of white-space separated values
   * @param aRemoveValue    [IN] the value to remove from the list
   */
  void RemoveValueFromListOfValues(nsAString& aValues,
                                   const nsAString& aRemoveValue);

  /**
   * If the boolean is true and if the value is not the empty string,
   * set the property in the transaction to that value; if the value
   * is empty, remove the property from element's styles. If the boolean
   * is false, just remove the style attribute.
   */
  MOZ_CAN_RUN_SCRIPT nsresult SetStyle(bool aAttributeWasSet,
                                       nsAString& aValue);

  // The element to operate upon.
  RefPtr<nsStyledElement> mStyledElement;

  // The CSS property to change.
  RefPtr<nsAtom> mProperty;

  // The value to set the property to (ignored if mRemoveProperty==true).
  nsString mValue;

  // The value to set the property to for undo.
  nsString mUndoValue;
  // The value to set the property to for redo.
  nsString mRedoValue;

  // true if the operation is to remove mProperty from mElement.
  bool mRemoveProperty;

  // True if the style attribute was present and not empty before DoTransaction.
  bool mUndoAttributeWasSet;
  // True if the style attribute is present and not empty after DoTransaction.
  bool mRedoAttributeWasSet;
};

}  // namespace mozilla

#endif  // #ifndef mozilla_ChangeStyleTransaction_h
