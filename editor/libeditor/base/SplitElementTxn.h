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

#ifndef SplitElementTxn_h__
#define SplitElementTxn_h__

#include "EditTxn.h"
#include "nsIDOMNode.h"
#include "nsCOMPtr.h"
#include "nsIEditor.h"

#define SPLIT_ELEMENT_TXN_IID \
{/* 690c6290-ac48-11d2-86d8-000064657374 */ \
0x690c6290, 0xac48, 0x11d2, \
{0x86, 0xd8, 0x0, 0x0, 0x64, 0x65, 0x73, 0x74} }

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
  NS_IMETHOD Init (nsIEditor  *aEditor,
                   nsIDOMNode *aNode,
                   PRInt32     aOffset);
protected:
  SplitElementTxn();

public:
  virtual ~SplitElementTxn();

  NS_IMETHOD Do(void);

  NS_IMETHOD Undo(void);

  //NS_IMETHOD Redo(void);

  NS_IMETHOD Merge(PRBool *aDidMerge, nsITransaction *aTransaction);

  NS_IMETHOD Write(nsIOutputStream *aOutputStream);

  NS_IMETHOD GetUndoString(nsString **aString);

  NS_IMETHOD GetRedoString(nsString **aString);

  NS_IMETHOD GetNewNode(nsIDOMNode **aNewNode);

protected:
  
  /** the element to operate upon */
  nsCOMPtr<nsIDOMNode> mExistingRightNode;

  /** the offset into mElement where the children of mElement are split.<BR>
    * mOffset is the index of the first child in the right node. 
    * -1 means the new node gets no children.
    */
  PRInt32  mOffset;

  /** the element we create when splitting mElement */
  nsCOMPtr<nsIDOMNode> mNewLeftNode;

  /** the parent shared by mExistingRightNode and mNewLeftNode */
  nsCOMPtr<nsIDOMNode> mParent;
  nsCOMPtr<nsIEditor>  mEditor;

  friend class TransactionFactory;

};

#endif
