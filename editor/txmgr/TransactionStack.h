/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_TransactionStack_h
#define mozilla_TransactionStack_h

#include "mozilla/AlreadyAddRefed.h"
#include "nsDeque.h"

class nsCycleCollectionTraversalCallback;

namespace mozilla {

class TransactionItem;

class TransactionStack : private nsDeque {
 public:
  enum Type { FOR_UNDO, FOR_REDO };

  explicit TransactionStack(Type aType);
  ~TransactionStack();

  void Push(TransactionItem* aTransactionItem);
  void Push(already_AddRefed<TransactionItem> aTransactionItem);
  already_AddRefed<TransactionItem> Pop();
  already_AddRefed<TransactionItem> PopBottom();
  already_AddRefed<TransactionItem> Peek();
  already_AddRefed<TransactionItem> GetItemAt(size_t aIndex) const;
  void Clear();
  size_t GetSize() const { return nsDeque::GetSize(); }
  bool IsEmpty() const { return GetSize() == 0; }

  void DoUnlink() { Clear(); }
  void DoTraverse(nsCycleCollectionTraversalCallback& cb);

 private:
  const Type mType;
};

}  // namespace mozilla

#endif  // mozilla_TransactionStack_h
