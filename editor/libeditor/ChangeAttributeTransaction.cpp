/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ChangeAttributeTransaction.h"

#include "mozilla/dom/Element.h"  // for Element

#include "nsAString.h"
#include "nsError.h"  // for NS_ERROR_NOT_INITIALIZED, etc.

namespace mozilla {

using namespace dom;

// static
already_AddRefed<ChangeAttributeTransaction> ChangeAttributeTransaction::Create(
    Element& aElement, nsAtom& aAttribute, const nsAString& aValue) {
  RefPtr<ChangeAttributeTransaction> transaction =
      new ChangeAttributeTransaction(aElement, aAttribute, &aValue);
  return transaction.forget();
}

// static
already_AddRefed<ChangeAttributeTransaction>
ChangeAttributeTransaction::CreateToRemove(Element& aElement,
                                           nsAtom& aAttribute) {
  RefPtr<ChangeAttributeTransaction> transaction =
      new ChangeAttributeTransaction(aElement, aAttribute, nullptr);
  return transaction.forget();
}

ChangeAttributeTransaction::ChangeAttributeTransaction(Element& aElement,
                                                       nsAtom& aAttribute,
                                                       const nsAString* aValue)
    : EditTransactionBase(),
      mElement(&aElement),
      mAttribute(&aAttribute),
      mValue(aValue ? *aValue : EmptyString()),
      mRemoveAttribute(!aValue),
      mAttributeWasSet(false) {}

NS_IMPL_CYCLE_COLLECTION_INHERITED(ChangeAttributeTransaction,
                                   EditTransactionBase, mElement)

NS_IMPL_ADDREF_INHERITED(ChangeAttributeTransaction, EditTransactionBase)
NS_IMPL_RELEASE_INHERITED(ChangeAttributeTransaction, EditTransactionBase)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ChangeAttributeTransaction)
NS_INTERFACE_MAP_END_INHERITING(EditTransactionBase)

NS_IMETHODIMP ChangeAttributeTransaction::DoTransaction() {
  // Need to get the current value of the attribute and save it, and set
  // mAttributeWasSet
  mAttributeWasSet =
      mElement->GetAttr(kNameSpaceID_None, mAttribute, mUndoValue);

  // XXX: hack until attribute-was-set code is implemented
  if (!mUndoValue.IsEmpty()) {
    mAttributeWasSet = true;
  }
  // XXX: end hack

  // Now set the attribute to the new value
  if (mRemoveAttribute) {
    OwningNonNull<Element> element = *mElement;
    nsresult rv = element->UnsetAttr(kNameSpaceID_None, mAttribute, true);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "Element::UnsetAttr() failed");
    return rv;
  }

  OwningNonNull<Element> element = *mElement;
  nsresult rv = element->SetAttr(kNameSpaceID_None, mAttribute, mValue, true);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "Element::SetAttr() failed");
  return rv;
}

NS_IMETHODIMP ChangeAttributeTransaction::UndoTransaction() {
  if (NS_WARN_IF(!mElement)) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  if (mAttributeWasSet) {
    OwningNonNull<Element> element = *mElement;
    nsresult rv =
        element->SetAttr(kNameSpaceID_None, mAttribute, mUndoValue, true);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "Element::SetAttr() failed");
    return rv;
  }
  OwningNonNull<Element> element = *mElement;
  nsresult rv = element->UnsetAttr(kNameSpaceID_None, mAttribute, true);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "Element::UnsetAttr() failed");
  return rv;
}

NS_IMETHODIMP ChangeAttributeTransaction::RedoTransaction() {
  if (NS_WARN_IF(!mElement)) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  if (mRemoveAttribute) {
    OwningNonNull<Element> element = *mElement;
    nsresult rv = element->UnsetAttr(kNameSpaceID_None, mAttribute, true);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "Element::UnsetAttr() failed");
    return rv;
  }

  OwningNonNull<Element> element = *mElement;
  nsresult rv = element->SetAttr(kNameSpaceID_None, mAttribute, mValue, true);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "Element::SetAttr() failed");
  return rv;
}

}  // namespace mozilla
