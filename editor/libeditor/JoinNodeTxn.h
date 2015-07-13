/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef JoinNodeTxn_h__
#define JoinNodeTxn_h__

#include "EditTxn.h"                    // for EditTxn, NS_DECL_EDITTXN
#include "nsCOMPtr.h"                   // for nsCOMPtr
#include "nsCycleCollectionParticipant.h"
#include "nsID.h"                       // for REFNSIID
#include "nscore.h"                     // for NS_IMETHOD

class nsEditor;
class nsINode;

namespace mozilla {
namespace dom {

/**
 * A transaction that joins two nodes E1 (left node) and E2 (right node) into a
 * single node E.  The children of E are the children of E1 followed by the
 * children of E2.  After DoTransaction() and RedoTransaction(), E1 is removed
 * from the content tree and E2 remains.
 */
class JoinNodeTxn : public EditTxn
{
public:
  /** @param aEditor    the provider of core editing operations
    * @param aLeftNode  the first of two nodes to join
    * @param aRightNode the second of two nodes to join
    */
  JoinNodeTxn(nsEditor& aEditor, nsINode& aLeftNode, nsINode& aRightNode);

  /* Call this after constructing to ensure the inputs are correct */
  nsresult CheckValidity();

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(JoinNodeTxn, EditTxn)
  NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr) override;

  NS_DECL_EDITTXN

protected:
  nsEditor&  mEditor;

  /** The nodes to operate upon.  After the merge, mRightNode remains and
   * mLeftNode is removed from the content tree.
    */
  nsCOMPtr<nsINode> mLeftNode;
  nsCOMPtr<nsINode> mRightNode;

  /** The offset into mNode where the children of mElement are split (for
    * undo). mOffset is the index of the first child in the right node.  -1
    * means the left node had no children.
    */
  uint32_t  mOffset;

  /** The parent node containing mLeftNode and mRightNode */
  nsCOMPtr<nsINode> mParent;
};

} // namespace dom
} // namespace mozilla

#endif
