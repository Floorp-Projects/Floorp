/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef JoinElementTxn_h__
#define JoinElementTxn_h__

#include "EditTxn.h"
#include "nsIDOMNode.h"
#include "nsCOMPtr.h"
#include "nsIEditor.h"

#define JOIN_ELEMENT_TXN_IID \
{/* 9bc5f9f0-ac48-11d2-86d8-000064657374 */ \
0x9bc5f9f0, 0xac48, 0x11d2, \
{0x86, 0xd8, 0x0, 0x0, 0x64, 0x65, 0x73, 0x74} }


/**
 * A transaction that joins two elements E1 (left node) and E2 (right node)
 * into a single node E.  
 * The children of E are the children of E1 followed by the children of E2.
 * After Do() and Redo(), E1 is removed from the content tree and E2 remains.
 */
class JoinElementTxn : public EditTxn
{
public:

  /** initialize the transaction
    * @param aEditor    the provider of core editing operations
    * @param aLeftNode  the first of two nodes to join
    * @param aRightNode the second of two nodes to join
    */
  NS_IMETHOD Init(nsIEditor  *aEditor,
                  nsIDOMNode *aLeftNode,
                  nsIDOMNode *aRightNode);
protected:
  JoinElementTxn();

public:

  virtual ~JoinElementTxn();

  NS_IMETHOD Do(void);

  NS_IMETHOD Undo(void);

//  NS_IMETHOD Redo(void);

  NS_IMETHOD GetIsTransient(PRBool *aIsTransient);

  NS_IMETHOD Merge(PRBool *aDidMerge, nsITransaction *aTransaction);

  NS_IMETHOD Write(nsIOutputStream *aOutputStream);

  NS_IMETHOD GetUndoString(nsString **aString);

  NS_IMETHOD GetRedoString(nsString **aString);

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
  nsCOMPtr<nsIEditor>  mEditor;

  friend class TransactionFactory;

};

#endif
