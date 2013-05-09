/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsISupportsUtils.h"
#include "nsTransactionItem.h"
#include "nsTransactionStack.h"
#include "nscore.h"

nsTransactionStack::nsTransactionStack(nsTransactionStack::Type aType)
  : mType(aType)
{
}

nsTransactionStack::~nsTransactionStack()
{
  Clear();
}

void
nsTransactionStack::Push(nsTransactionItem *aTransaction)
{
  if (!aTransaction) {
    return;
  }

  // The stack's bottom is the front of the deque, and the top is the back.
  mDeque.push_back(aTransaction);
}

already_AddRefed<nsTransactionItem>
nsTransactionStack::Pop()
{
  if (mDeque.empty()) {
    return nullptr;
  }
  nsRefPtr<nsTransactionItem> ret = mDeque.back().forget();
  mDeque.pop_back();
  return ret.forget();
}

already_AddRefed<nsTransactionItem>
nsTransactionStack::PopBottom()
{
  if (mDeque.empty()) {
    return nullptr;
  }
  nsRefPtr<nsTransactionItem> ret = mDeque.front().forget();
  mDeque.pop_front();
  return ret.forget();
}

already_AddRefed<nsTransactionItem>
nsTransactionStack::Peek()
{
  if (mDeque.empty()) {
    return nullptr;
  }
  nsRefPtr<nsTransactionItem> ret = mDeque.back();
  return ret.forget();
}

already_AddRefed<nsTransactionItem>
nsTransactionStack::GetItem(int32_t aIndex)
{
  if (aIndex < 0 || aIndex >= static_cast<int32_t>(mDeque.size())) {
    return nullptr;
  }
  nsRefPtr<nsTransactionItem> ret = mDeque[aIndex];
  return ret.forget();
}

void
nsTransactionStack::Clear()
{
  while (!mDeque.empty()) {
    nsRefPtr<nsTransactionItem> tx = mType == FOR_UNDO ? Pop() : PopBottom();
  };
}

void
nsTransactionStack::DoTraverse(nsCycleCollectionTraversalCallback &cb)
{
  int32_t size = mDeque.size();
  for (int32_t i = 0; i < size; ++i) {
    nsTransactionItem* item = mDeque[i];
    if (item) {
      NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "transaction stack mDeque[i]");
      cb.NoteNativeChild(item, NS_CYCLE_COLLECTION_PARTICIPANT(nsTransactionItem));
    }
  }
}
