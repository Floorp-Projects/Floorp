/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef InsertNodeTransaction_h
#define InsertNodeTransaction_h

#include "mozilla/EditTransactionBase.h" // for EditTransactionBase, etc.
#include "nsCOMPtr.h"                   // for nsCOMPtr
#include "nsCycleCollectionParticipant.h"
#include "nsIContent.h"                 // for nsIContent
#include "nsISupportsImpl.h"            // for NS_DECL_ISUPPORTS_INHERITED

namespace mozilla {

class EditorBase;

/**
 * A transaction that inserts a single element
 */
class InsertNodeTransaction final : public EditTransactionBase
{
public:
  /**
   * Initialize the transaction.
   * @param aNode       The node to insert.
   * @param aParent     The node to insert into.
   * @param aOffset     The offset in aParent to insert aNode.
   */
  InsertNodeTransaction(nsIContent& aNode, nsINode& aParent, int32_t aOffset,
                        EditorBase& aEditorBase);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(InsertNodeTransaction,
                                           EditTransactionBase)

  NS_DECL_EDITTRANSACTIONBASE

protected:
  virtual ~InsertNodeTransaction();

  // The element to insert.
  nsCOMPtr<nsIContent> mNode;

  // The node into which the new node will be inserted.
  nsCOMPtr<nsINode> mParent;

  // The index in mParent for the new node.
  int32_t mOffset;

  // The editor for this transaction.
  EditorBase& mEditorBase;
};

} // namespace mozilla

#endif // #ifndef InsertNodeTransaction_h
