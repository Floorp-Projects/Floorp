/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef InsertNodeTransaction_h
#define InsertNodeTransaction_h

#include "mozilla/EditTransactionBase.h"  // for EditTransactionBase, etc.
#include "mozilla/EditorDOMPoint.h"       // for EditorDOMPoint
#include "nsCOMPtr.h"                     // for nsCOMPtr
#include "nsCycleCollectionParticipant.h"
#include "nsIContent.h"       // for nsIContent
#include "nsISupportsImpl.h"  // for NS_DECL_ISUPPORTS_INHERITED

namespace mozilla {

class EditorBase;

/**
 * A transaction that inserts a single element
 */
class InsertNodeTransaction final : public EditTransactionBase {
 protected:
  template <typename PT, typename CT>
  InsertNodeTransaction(EditorBase& aEditorBase, nsIContent& aContentToInsert,
                        const EditorDOMPointBase<PT, CT>& aPointToInsert);

 public:
  /**
   * Create a transaction for inserting aContentToInsert before the child
   * at aPointToInsert.
   *
   * @param aEditorBase         The editor which manages the transaction.
   * @param aContentToInsert    The node to be inserted.
   * @param aPointToInsert      The insertion point of aContentToInsert.
   *                            If this refers end of the container, the
   *                            transaction will append the node to the
   *                            container.  Otherwise, will insert the node
   *                            before child node referred by this.
   * @return                    A InsertNodeTranaction which was initialized
   *                            with the arguments.
   */
  template <typename PT, typename CT>
  static already_AddRefed<InsertNodeTransaction> Create(
      EditorBase& aEditorBase, nsIContent& aContentToInsert,
      const EditorDOMPointBase<PT, CT>& aPointToInsert);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(InsertNodeTransaction,
                                           EditTransactionBase)

  NS_DECL_EDITTRANSACTIONBASE
  NS_DECL_EDITTRANSACTIONBASE_GETASMETHODS_OVERRIDE(InsertNodeTransaction)

 protected:
  virtual ~InsertNodeTransaction() = default;

  // The element to insert.
  nsCOMPtr<nsIContent> mContentToInsert;

  // The DOM point we will insert mContentToInsert.
  EditorDOMPoint mPointToInsert;

  // The editor for this transaction.
  RefPtr<EditorBase> mEditorBase;
};

}  // namespace mozilla

#endif  // #ifndef InsertNodeTransaction_h
