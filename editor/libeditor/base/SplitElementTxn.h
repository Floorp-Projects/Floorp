/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SplitElementTxn_h__
#define SplitElementTxn_h__

#include "EditTxn.h"                    // for EditTxn, NS_DECL_EDITTXN
#include "nsCOMPtr.h"                   // for nsCOMPtr
#include "nsCycleCollectionParticipant.h"
#include "nsIDOMNode.h"                 // for nsIDOMNode
#include "nsISupportsImpl.h"            // for NS_DECL_ISUPPORTS_INHERITED
#include "nscore.h"                     // for NS_IMETHOD

class nsEditor;

/**
 * A transaction that splits an element E into two identical nodes, E1 and E2
 * with the children of E divided between E1 and E2.
 */
class SplitElementTxn : public EditTxn
{
public:
  /** initialize the transaction.
    * @param aEditor  the provider of core editing operations
    * @param aNode    the node to split
    * @param aOffset  the location within aNode to do the split.
    *                 aOffset may refer to children of aNode, or content of aNode.
    *                 The left node will have child|content 0..aOffset-1.
    */
  NS_IMETHOD Init (nsEditor   *aEditor,
                   nsIDOMNode *aNode,
                   int32_t     aOffset);

  SplitElementTxn();

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(SplitElementTxn, EditTxn)

  NS_DECL_EDITTXN

  NS_IMETHOD RedoTransaction(void);

  NS_IMETHOD GetNewNode(nsIDOMNode **aNewNode);

protected:
  
  /** the element to operate upon */
  nsCOMPtr<nsIDOMNode> mExistingRightNode;

  /** the offset into mElement where the children of mElement are split.<BR>
    * mOffset is the index of the first child in the right node. 
    * -1 means the new node gets no children.
    */
  int32_t  mOffset;

  /** the element we create when splitting mElement */
  nsCOMPtr<nsIDOMNode> mNewLeftNode;

  /** the parent shared by mExistingRightNode and mNewLeftNode */
  nsCOMPtr<nsIDOMNode> mParent;
  nsEditor*  mEditor;
};

#endif
