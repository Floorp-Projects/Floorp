/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef JoinElementTxn_h__
#define JoinElementTxn_h__

#include "EditTxn.h"
#include "nsIDOMNode.h"
#include "nsCOMPtr.h"
#include "nsIEditor.h"

#define JOIN_ELEMENT_TXN_CID \
{/* 9bc5f9f0-ac48-11d2-86d8-000064657374 */ \
0x9bc5f9f0, 0xac48, 0x11d2, \
{0x86, 0xd8, 0x0, 0x0, 0x64, 0x65, 0x73, 0x74} }

class nsEditor;

/**
 * A transaction that joins two elements E1 (left node) and E2 (right node)
 * into a single node E.  
 * The children of E are the children of E1 followed by the children of E2.
 * After DoTransaction() and RedoTransaction(), E1 is removed from the content
 * tree and E2 remains.
 */
class JoinElementTxn : public EditTxn
{
public:

  static const nsIID& GetCID() { static nsIID iid = JOIN_ELEMENT_TXN_CID; return iid; }

  /** initialize the transaction
    * @param aEditor    the provider of core editing operations
    * @param aLeftNode  the first of two nodes to join
    * @param aRightNode the second of two nodes to join
    */
  NS_IMETHOD Init(nsEditor   *aEditor,
                  nsIDOMNode *aLeftNode,
                  nsIDOMNode *aRightNode);
protected:
  JoinElementTxn();

public:

  virtual ~JoinElementTxn();

  NS_IMETHOD DoTransaction(void);

  NS_IMETHOD UndoTransaction(void);

//  NS_IMETHOD RedoTransaction(void);

  NS_IMETHOD GetIsTransient(PRBool *aIsTransient);

  NS_IMETHOD Merge(nsITransaction *aTransaction, PRBool *aDidMerge);

  NS_IMETHOD GetTxnDescription(nsAWritableString& aTxnDescription);

protected:
  
  /** the elements to operate upon.  
    * After the merge, mRightNode remains and mLeftNode is removed from the content tree.
    */
  nsCOMPtr<nsIDOMNode> mLeftNode;
  nsCOMPtr<nsIDOMNode> mRightNode;

  /** the offset into mNode where the children of mElement are split (for undo).<BR>
    * mOffset is the index of the first child in the right node. 
    * -1 means the left node had no children.
    */
  PRUint32  mOffset;

  /** the parent node containing mLeftNode and mRightNode */
  nsCOMPtr<nsIDOMNode> mParent;
  nsEditor*  mEditor;

  friend class TransactionFactory;

};

#endif
