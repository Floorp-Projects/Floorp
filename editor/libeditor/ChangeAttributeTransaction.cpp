/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ChangeAttributeTransaction.h"

#include "mozilla/Logging.h"
#include "mozilla/ToString.h"
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
      mValue(aValue ? *aValue : u""_ns),
      mRemoveAttribute(!aValue),
      mAttributeWasSet(false) {}

std::ostream& operator<<(std::ostream& aStream,
                         const ChangeAttributeTransaction& aTransaction) {
  aStream << "{ mElement=" << aTransaction.mElement.get();
  if (aTransaction.mElement) {
    aStream << " (" << *aTransaction.mElement << ")";
  }
  aStream << ", mAttribute=" << nsAtomCString(aTransaction.mAttribute).get()
          << ", mValue=\"" << NS_ConvertUTF16toUTF8(aTransaction.mValue).get()
          << "\", mUndoValue=\""
          << NS_ConvertUTF16toUTF8(aTransaction.mUndoValue).get()
          << "\", mRemoveAttribute="
          << (aTransaction.mRemoveAttribute ? "true" : "false")
          << ", mAttributeWasSet="
          << (aTransaction.mAttributeWasSet ? "true" : "false") << " }";
  return aStream;
}

NS_IMPL_CYCLE_COLLECTION_INHERITED(ChangeAttributeTransaction,
                                   EditTransactionBase, mElement)

NS_IMPL_ADDREF_INHERITED(ChangeAttributeTransaction, EditTransactionBase)
NS_IMPL_RELEASE_INHERITED(ChangeAttributeTransaction, EditTransactionBase)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ChangeAttributeTransaction)
NS_INTERFACE_MAP_END_INHERITING(EditTransactionBase)

NS_IMETHODIMP ChangeAttributeTransaction::DoTransaction() {
  // Need to get the current value of the attribute and save it, and set
  // mAttributeWasSet
  mAttributeWasSet = mElement->GetAttr(mAttribute, mUndoValue);

  // XXX: hack until attribute-was-set code is implemented
  if (!mUndoValue.IsEmpty()) {
    mAttributeWasSet = true;
  }
  // XXX: end hack

  MOZ_LOG(GetLogModule(), LogLevel::Info,
          ("%p ChangeAttributeTransaction::%s this=%s", this, __FUNCTION__,
           ToString(*this).c_str()));

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
  MOZ_LOG(GetLogModule(), LogLevel::Info,
          ("%p ChangeAttributeTransaction::%s this=%s", this, __FUNCTION__,
           ToString(*this).c_str()));

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
  MOZ_LOG(GetLogModule(), LogLevel::Info,
          ("%p ChangeAttributeTransaction::%s this=%s", this, __FUNCTION__,
           ToString(*this).c_str()));

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
