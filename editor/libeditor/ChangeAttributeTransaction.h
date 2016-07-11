/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ChangeAttributeTransaction_h
#define ChangeAttributeTransaction_h

#include "mozilla/Attributes.h"           // override
#include "mozilla/EditTransactionBase.h"  // base class
#include "nsCOMPtr.h"                     // nsCOMPtr members
#include "nsCycleCollectionParticipant.h" // NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED
#include "nsISupportsImpl.h"              // NS_DECL_ISUPPORTS_INHERITED
#include "nsString.h"                     // nsString members

class nsIAtom;

namespace mozilla {

namespace dom {
class Element;
} // namespace dom

/**
 * A transaction that changes an attribute of a content node.  This transaction
 * covers add, remove, and change attribute.
 */
class ChangeAttributeTransaction final : public EditTransactionBase
{
public:
  /**
   * @param aElement   the element whose attribute will be changed
   * @param aAttribute the name of the attribute to change
   * @param aValue     the new value for aAttribute, or null to remove
   */
  ChangeAttributeTransaction(dom::Element& aElement,
                             nsIAtom& aAttribute,
                             const nsAString* aValue);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(ChangeAttributeTransaction,
                                           EditTransactionBase)

  NS_DECL_EDITTRANSACTIONBASE

  NS_IMETHOD RedoTransaction() override;

private:
  virtual ~ChangeAttributeTransaction();

  // The element to operate upon
  nsCOMPtr<dom::Element> mElement;

  // The attribute to change
  nsCOMPtr<nsIAtom> mAttribute;

  // The value to set the attribute to (ignored if mRemoveAttribute==true)
  nsString mValue;

  // True if the operation is to remove mAttribute from mElement
  bool mRemoveAttribute;

  // True if the mAttribute was set on mElement at the time of execution
  bool mAttributeWasSet;

  // The value to set the attribute to for undo
  nsString mUndoValue;
};

} // namespace mozilla

#endif // #ifndef ChangeAttributeTransaction_h
