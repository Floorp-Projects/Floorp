/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TransactionStack.h"

#include "nsCycleCollectionParticipant.h"
#include "nsDebug.h"
#include "nsISupportsUtils.h"
#include "nscore.h"
#include "TransactionItem.h"

namespace mozilla {

TransactionStack::TransactionStack(Type aType) : mType(aType) {}

TransactionStack::~TransactionStack() { Clear(); }

void TransactionStack::Push(TransactionItem* aTransactionItem) {
  if (NS_WARN_IF(!aTransactionItem)) {
    return;
  }
  nsRefPtrDeque<TransactionItem>::Push(aTransactionItem);
}

void TransactionStack::Push(
    already_AddRefed<TransactionItem> aTransactionItem) {
  TransactionItem* transactionItem = aTransactionItem.take();
  if (NS_WARN_IF(!transactionItem)) {
    return;
  }
  nsRefPtrDeque<TransactionItem>::Push(dont_AddRef(transactionItem));
}

already_AddRefed<TransactionItem> TransactionStack::Pop() {
  return nsRefPtrDeque<TransactionItem>::Pop();
}

already_AddRefed<TransactionItem> TransactionStack::PopBottom() {
  return nsRefPtrDeque<TransactionItem>::PopFront();
}

already_AddRefed<TransactionItem> TransactionStack::Peek() const {
  RefPtr<TransactionItem> item = nsRefPtrDeque<TransactionItem>::Peek();
  return item.forget();
}

already_AddRefed<TransactionItem> TransactionStack::GetItemAt(
    size_t aIndex) const {
  RefPtr<TransactionItem> item =
      nsRefPtrDeque<TransactionItem>::ObjectAt(aIndex);
  return item.forget();
}

void TransactionStack::Clear() {
  while (GetSize()) {
    RefPtr<TransactionItem> item = mType == FOR_UNDO ? Pop() : PopBottom();
  }
}

void TransactionStack::DoTraverse(nsCycleCollectionTraversalCallback& cb) {
  size_t size = GetSize();
  for (size_t i = 0; i < size; ++i) {
    auto* item = nsRefPtrDeque<TransactionItem>::ObjectAt(i);
    if (item) {
      NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "transaction stack mDeque[i]");
      cb.NoteNativeChild(item,
                         NS_CYCLE_COLLECTION_PARTICIPANT(TransactionItem));
    }
  }
}

}  // namespace mozilla
