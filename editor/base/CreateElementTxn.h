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

#ifndef CreateElementTxn_h__
#define CreateElementTxn_h__

#include "EditTxn.h"
#include "nsEditor.h"
#include "nsIDOMNode.h"
#include "nsCOMPtr.h"

#define CREATE_ELEMENT_TXN_CID \
{/* 7a6393c0-ac48-11d2-86d8-000064657374 */ \
0x7a6393c0, 0xac48, 0x11d2, \
{0x86, 0xd8, 0x0, 0x0, 0x64, 0x65, 0x73, 0x74} }

/**
 * A transaction that creates a new node in the content tree.
 */
class CreateElementTxn : public EditTxn
{
public:

  static const nsIID& GetCID() { static nsIID iid = CREATE_ELEMENT_TXN_CID; return iid; }

  enum { eAppend=-1 };

  /** Initialize the transaction.
    * @param aEditor the provider of basic editing functionality
    * @param aTag    the tag (P, HR, TABLE, etc.) for the new element
    * @param aParent the node into which the new element will be inserted
    * @param aOffsetInParent the location in aParent to insert the new element
    *                        if eAppend, the new element is appended as the last child
    */
  NS_IMETHOD Init(nsEditor *aEditor,
                  const nsAReadableString& aTag,
                  nsIDOMNode *aParent,
                  PRUint32 aOffsetInParent);

private:
  CreateElementTxn();

public:

  virtual ~CreateElementTxn();

  NS_IMETHOD DoTransaction(void);

  NS_IMETHOD UndoTransaction(void);

  NS_IMETHOD RedoTransaction(void);

  NS_IMETHOD Merge(nsITransaction *aTransaction, PRBool *aDidMerge);

  NS_IMETHOD GetTxnDescription(nsAWritableString& aTxnDescription);

  NS_IMETHOD GetNewNode(nsIDOMNode **aNewNode);

protected:
  
  /** the document into which the new node will be inserted */
  nsEditor* mEditor;
  
  /** the tag (mapping to object type) for the new element */
  nsString mTag;

  /** the node into which the new node will be inserted */
  nsCOMPtr<nsIDOMNode> mParent;

  /** the index in mParent for the new node */
  PRUint32 mOffsetInParent;

  /** the new node to insert */
  nsCOMPtr<nsIDOMNode> mNewNode;  

  /** the node we will insert mNewNode before.  We compute this ourselves. */
  nsCOMPtr<nsIDOMNode> mRefNode;

  friend class TransactionFactory;

};

#endif
