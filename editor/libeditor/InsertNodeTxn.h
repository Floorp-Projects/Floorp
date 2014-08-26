/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef InsertNodeTxn_h__
#define InsertNodeTxn_h__

#include "EditTxn.h"                    // for EditTxn, NS_DECL_EDITTXN
#include "nsCOMPtr.h"                   // for nsCOMPtr
#include "nsCycleCollectionParticipant.h"
#include "nsIContent.h"                 // for nsIContent
#include "nsISupportsImpl.h"            // for NS_DECL_ISUPPORTS_INHERITED

class nsEditor;

namespace mozilla {
namespace dom {

/**
 * A transaction that inserts a single element
 */
class InsertNodeTxn : public EditTxn
{
public:
  /** initialize the transaction.
    * @param aNode   the node to insert
    * @param aParent the node to insert into
    * @param aOffset the offset in aParent to insert aNode
    */
  InsertNodeTxn(nsIContent& aNode, nsINode& aParent, int32_t aOffset,
                nsEditor& aEditor);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(InsertNodeTxn, EditTxn)

  NS_DECL_EDITTXN

protected:
  virtual ~InsertNodeTxn();

  /** the element to insert */
  nsCOMPtr<nsIContent> mNode;

  /** the node into which the new node will be inserted */
  nsCOMPtr<nsINode> mParent;

  /** the index in mParent for the new node */
  int32_t mOffset;

  /** the editor for this transaction */
  nsEditor& mEditor;
};

}
}

#endif
