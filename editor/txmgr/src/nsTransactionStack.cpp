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
  : mQue(0)
  , mType(aType)
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

  /* nsDeque's Push() method adds new items at the back
   * of the deque.
   */
  NS_ADDREF(aTransaction);
  mQue.Push(aTransaction);
}

already_AddRefed<nsTransactionItem>
nsTransactionStack::Pop()
{
  /* nsDeque is a FIFO, so the top of our stack is actually
   * the back of the deque.
   */
  return static_cast<nsTransactionItem*> (mQue.Pop());
}

already_AddRefed<nsTransactionItem>
nsTransactionStack::PopBottom()
{
  /* nsDeque is a FIFO, so the bottom of our stack is actually
   * the front of the deque.
   */
  return static_cast<nsTransactionItem*> (mQue.PopFront());
}

already_AddRefed<nsTransactionItem>
nsTransactionStack::Peek()
{
  nsTransactionItem* transaction = nsnull;
  if (mQue.GetSize()) {
    NS_IF_ADDREF(transaction = static_cast<nsTransactionItem*>(mQue.Last()));
  }

  return transaction;
}

already_AddRefed<nsTransactionItem>
nsTransactionStack::GetItem(PRInt32 aIndex)
{
  nsTransactionItem* transaction = nsnull;
  if (aIndex >= 0 && aIndex < mQue.GetSize()) {
    NS_IF_ADDREF(transaction =
                 static_cast<nsTransactionItem*>(mQue.ObjectAt(aIndex)));
  }

  return transaction;
}

void
nsTransactionStack::Clear()
{
  nsRefPtr<nsTransactionItem> tx;

  do {
    tx = mType == FOR_UNDO ? Pop() : PopBottom();
  } while (tx);
}

void
nsTransactionStack::DoTraverse(nsCycleCollectionTraversalCallback &cb)
{
  for (PRInt32 i = 0, qcount = mQue.GetSize(); i < qcount; ++i) {
    nsTransactionItem *item =
      static_cast<nsTransactionItem*>(mQue.ObjectAt(i));
    if (item) {
      NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "transaction stack mQue[i]");
      cb.NoteNativeChild(item, NS_CYCLE_COLLECTION_PARTICIPANT(nsTransactionItem));
    }
  }
}
