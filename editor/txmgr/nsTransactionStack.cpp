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

class nsTransactionStackDeallocator : public nsDequeFunctor {
  virtual void* operator() (void* aObject) {
    RefPtr<nsTransactionItem> releaseMe = dont_AddRef(static_cast<nsTransactionItem*>(aObject));
    return nullptr;
  }
};

nsTransactionStack::nsTransactionStack(Type aType)
  : nsDeque(new nsTransactionStackDeallocator())
  , mType(aType)
{
}

nsTransactionStack::~nsTransactionStack()
{
  Clear();
}

void
nsTransactionStack::Push(nsTransactionItem* aTransactionItem)
{
  if (!aTransactionItem) {
    return;
  }

  RefPtr<nsTransactionItem> item(aTransactionItem);
  Push(item.forget());
}

void
nsTransactionStack::Push(already_AddRefed<nsTransactionItem> aTransactionItem)
{
  RefPtr<nsTransactionItem> item(aTransactionItem);
  if (!item) {
    return;
  }

  nsDeque::Push(item.forget().take());
}

already_AddRefed<nsTransactionItem>
nsTransactionStack::Pop()
{
  RefPtr<nsTransactionItem> item =
    dont_AddRef(static_cast<nsTransactionItem*>(nsDeque::Pop()));
  return item.forget();
}

already_AddRefed<nsTransactionItem>
nsTransactionStack::PopBottom()
{
  RefPtr<nsTransactionItem> item =
    dont_AddRef(static_cast<nsTransactionItem*>(nsDeque::PopFront()));
  return item.forget();
}

already_AddRefed<nsTransactionItem>
nsTransactionStack::Peek()
{
  RefPtr<nsTransactionItem> item =
    static_cast<nsTransactionItem*>(nsDeque::Peek());
  return item.forget();
}

already_AddRefed<nsTransactionItem>
nsTransactionStack::GetItem(int32_t aIndex)
{
  if (aIndex < 0 || aIndex >= static_cast<int32_t>(nsDeque::GetSize())) {
    return nullptr;
  }
  RefPtr<nsTransactionItem> item =
    static_cast<nsTransactionItem*>(nsDeque::ObjectAt(aIndex));
  return item.forget();
}

void
nsTransactionStack::Clear()
{
  while (GetSize() != 0) {
    RefPtr<nsTransactionItem> item =
      mType == FOR_UNDO ? Pop() : PopBottom();
  }
}

void
nsTransactionStack::DoTraverse(nsCycleCollectionTraversalCallback &cb)
{
  int32_t size = GetSize();
  for (int32_t i = 0; i < size; ++i) {
    nsTransactionItem* item = static_cast<nsTransactionItem*>(nsDeque::ObjectAt(i));
    if (item) {
      NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "transaction stack mDeque[i]");
      cb.NoteNativeChild(item, NS_CYCLE_COLLECTION_PARTICIPANT(nsTransactionItem));
    }
  }
}
