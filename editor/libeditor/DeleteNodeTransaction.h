/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DeleteNodeTransaction_h
#define DeleteNodeTransaction_h

#include "mozilla/EditTransactionBase.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIContent.h"
#include "nsINode.h"
#include "nsISupportsImpl.h"
#include "nscore.h"

namespace mozilla {

class EditorBase;
class RangeUpdater;

/**
 * A transaction that deletes a single element
 */
class DeleteNodeTransaction final : public EditTransactionBase
{
public:
  /**
   * Initialize the transaction.
   * @param aElement        The node to delete.
   */
  nsresult Init(EditorBase* aEditorBase, nsINode* aNode,
                RangeUpdater* aRangeUpdater);

  DeleteNodeTransaction();

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(DeleteNodeTransaction,
                                           EditTransactionBase)

  NS_DECL_EDITTRANSACTIONBASE

  NS_IMETHOD RedoTransaction() override;

protected:
  virtual ~DeleteNodeTransaction();

  // The element to delete.
  nsCOMPtr<nsINode> mNode;

  // Parent of node to delete.
  nsCOMPtr<nsINode> mParent;

  // Next sibling to remember for undo/redo purposes.
  nsCOMPtr<nsIContent> mRefNode;

  // The editor for this transaction.
  EditorBase* mEditorBase;

  // Range updater object.
  RangeUpdater* mRangeUpdater;
};

} // namespace mozilla

#endif // #ifndef DeleteNodeTransaction_h
