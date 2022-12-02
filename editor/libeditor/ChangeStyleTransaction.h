/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ChangeStyleTransaction_h
#define ChangeStyleTransaction_h

#include "EditTransactionBase.h"  // base class

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
  static bool ValueIncludes(const nsACString& aValueList,
                            const nsACString& aValue);

  friend std::ostream& operator<<(std::ostream& aStream,
                                  const ChangeStyleTransaction& aTransaction);

 private:
  virtual ~ChangeStyleTransaction() = default;

  /**
   * Build new text-decoration value to set/remove specific values to/from the
   * rule which already has aCurrentValues.
   */
  void BuildTextDecorationValueToSet(const nsACString& aCurrentValues,
                                     const nsACString& aAddingValues,
                                     nsACString& aOutValues);
  void BuildTextDecorationValueToRemove(const nsACString& aCurrentValues,
                                        const nsACString& aRemovingValues,
                                        nsACString& aOutValues);

  /**
   * Helper method for above methods.
   */
  void BuildTextDecorationValue(bool aUnderline, bool aOverline,
                                bool aLineThrough, nsACString& aOutValues);

  /**
   * If the boolean is true and if the value is not the empty string,
   * set the property in the transaction to that value; if the value
   * is empty, remove the property from element's styles. If the boolean
   * is false, just remove the style attribute.
   */
  MOZ_CAN_RUN_SCRIPT nsresult SetStyle(bool aAttributeWasSet,
                                       nsACString& aValue);

  // The element to operate upon.
  RefPtr<nsStyledElement> mStyledElement;

  // The CSS property to change.
  RefPtr<nsAtom> mProperty;

  // The value to set the property to (ignored if mRemoveProperty==true).
  nsCString mValue;

  // The value to set the property to for undo.
  nsCString mUndoValue;
  // The value to set the property to for redo.
  nsCString mRedoValue;

  // true if the operation is to remove mProperty from mElement.
  bool mRemoveProperty;

  // True if the style attribute was present and not empty before DoTransaction.
  bool mUndoAttributeWasSet;
  // True if the style attribute is present and not empty after DoTransaction.
  bool mRedoAttributeWasSet;
};

}  // namespace mozilla

#endif  // #ifndef ChangeStyleTransaction_h
