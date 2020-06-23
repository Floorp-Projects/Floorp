/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TransactionStack.h"

#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsDebug.h"
#include "nsISupportsUtils.h"
#include "nscore.h"
#include "TransactionItem.h"

namespace mozilla {

class TransactionStackDeallocator final
    : public nsDequeFunctor<TransactionItem> {
  virtual void operator()(TransactionItem* aObject) override {
    RefPtr<TransactionItem> releaseMe = dont_AddRef(aObject);
  }
};

TransactionStack::TransactionStack(Type aType)
    : nsDeque<TransactionItem>(new TransactionStackDeallocator()),
      mType(aType) {}

TransactionStack::~TransactionStack() { Clear(); }

void TransactionStack::Push(TransactionItem* aTransactionItem) {
  if (NS_WARN_IF(!aTransactionItem)) {
    return;
  }

  RefPtr<TransactionItem> item(aTransactionItem);
  Push(item.forget());
}

void TransactionStack::Push(
    already_AddRefed<TransactionItem> aTransactionItem) {
  RefPtr<TransactionItem> transactionItem(aTransactionItem);
  if (NS_WARN_IF(!transactionItem)) {
    return;
  }

  nsDeque<TransactionItem>::Push(transactionItem.forget().take());
}

already_AddRefed<TransactionItem> TransactionStack::Pop() {
  RefPtr<TransactionItem> item = dont_AddRef(nsDeque<TransactionItem>::Pop());
  return item.forget();
}

already_AddRefed<TransactionItem> TransactionStack::PopBottom() {
  RefPtr<TransactionItem> item =
      dont_AddRef(nsDeque<TransactionItem>::PopFront());
  return item.forget();
}

already_AddRefed<TransactionItem> TransactionStack::Peek() {
  RefPtr<TransactionItem> item = nsDeque<TransactionItem>::Peek();
  return item.forget();
}

already_AddRefed<TransactionItem> TransactionStack::GetItemAt(
    size_t aIndex) const {
  if (NS_WARN_IF(aIndex >= nsDeque<TransactionItem>::GetSize())) {
    return nullptr;
  }
  RefPtr<TransactionItem> item = nsDeque<TransactionItem>::ObjectAt(aIndex);
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
    TransactionItem* item = nsDeque<TransactionItem>::ObjectAt(i);
    if (item) {
      NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "transaction stack mDeque[i]");
      cb.NoteNativeChild(item,
                         NS_CYCLE_COLLECTION_PARTICIPANT(TransactionItem));
    }
  }
}

}  // namespace mozilla
