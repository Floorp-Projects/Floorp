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

/**
 * A transaction that deletes a single element
 */
class DeleteNodeTransaction final : public EditTransactionBase {
 protected:
  DeleteNodeTransaction(EditorBase& aEditorBase, nsIContent& aContentToDelete);

 public:
  /**
   * Creates a delete node transaction instance.  This returns nullptr if
   * it cannot remove the node from its parent.
   *
   * @param aEditorBase         The editor.
   * @param aContentToDelete    The node to be removed from the DOM tree.
   */
  static already_AddRefed<DeleteNodeTransaction> MaybeCreate(
      EditorBase& aEditorBase, nsIContent& aContentToDelete);

  /**
   * CanDoIt() returns true if there are enough members and can modify the
   * parent.  Otherwise, false.
   */
  bool CanDoIt() const;

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(DeleteNodeTransaction,
                                           EditTransactionBase)

  NS_DECL_EDITTRANSACTIONBASE
  NS_DECL_EDITTRANSACTIONBASE_GETASMETHODS_OVERRIDE(DeleteNodeTransaction)

  MOZ_CAN_RUN_SCRIPT NS_IMETHOD RedoTransaction() override;

 protected:
  virtual ~DeleteNodeTransaction() = default;

  // The editor for this transaction.
  RefPtr<EditorBase> mEditorBase;

  // The element to delete.
  nsCOMPtr<nsIContent> mContentToDelete;

  // Parent of node to delete.
  nsCOMPtr<nsINode> mParentNode;

  // Next sibling to remember for undo/redo purposes.
  nsCOMPtr<nsIContent> mRefContent;
};

}  // namespace mozilla

#endif  // #ifndef DeleteNodeTransaction_h
