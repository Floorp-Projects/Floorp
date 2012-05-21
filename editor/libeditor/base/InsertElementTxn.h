/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef InsertElementTxn_h__
#define InsertElementTxn_h__

#include "EditTxn.h"
#include "nsIEditor.h"
#include "nsIDOMNode.h"
#include "nsCOMPtr.h"

/**
 * A transaction that inserts a single element
 */
class InsertElementTxn : public EditTxn
{
public:
  /** initialize the transaction.
    * @param aNode   the node to insert
    * @param aParent the node to insert into
    * @param aOffset the offset in aParent to insert aNode
    */
  NS_IMETHOD Init(nsIDOMNode *aNode,
                  nsIDOMNode *aParent,
                  PRInt32     aOffset,
                  nsIEditor  *aEditor);

  InsertElementTxn();

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(InsertElementTxn, EditTxn)

  NS_DECL_EDITTXN

protected:
  
  /** the element to insert */
  nsCOMPtr<nsIDOMNode> mNode;

  /** the node into which the new node will be inserted */
  nsCOMPtr<nsIDOMNode> mParent;

  /** the editor for this transaction */
  nsIEditor*           mEditor;

  /** the index in mParent for the new node */
  PRInt32 mOffset;
};

#endif
