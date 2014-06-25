/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DeleteNodeTxn_h__
#define DeleteNodeTxn_h__

#include "EditTxn.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIContent.h"
#include "nsINode.h"
#include "nsISupportsImpl.h"
#include "nscore.h"

class nsEditor;
class nsRangeUpdater;

/**
 * A transaction that deletes a single element
 */
class DeleteNodeTxn : public EditTxn
{
public:
  /** initialize the transaction.
    * @param aElement the node to delete
    */
  nsresult Init(nsEditor* aEditor, nsINode* aNode,
                nsRangeUpdater* aRangeUpdater);

  DeleteNodeTxn();

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(DeleteNodeTxn, EditTxn)

  NS_DECL_EDITTXN

  NS_IMETHOD RedoTransaction();

protected:

  /** the element to delete */
  nsCOMPtr<nsINode> mNode;

  /** parent of node to delete */
  nsCOMPtr<nsINode> mParent;

  /** next sibling to remember for undo/redo purposes */
  nsCOMPtr<nsIContent> mRefNode;

  /** the editor for this transaction */
  nsEditor* mEditor;

  /** range updater object */
  nsRangeUpdater* mRangeUpdater;
};

#endif
