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

#ifndef DeleteElementTxn_h__
#define DeleteElementTxn_h__

#include "EditTxn.h"
#include "nsIDOMNode.h"
#include "nsCOMPtr.h"

#define DELETE_ELEMENT_TXN_CID \
{/* 6fd77770-ac49-11d2-86d8-000064657374 */ \
0x6fd77770, 0xac49, 0x11d2, \
{0x86, 0xd8, 0x0, 0x0, 0x64, 0x65, 0x73, 0x74} }

/**
 * A transaction that deletes a single element
 */
class DeleteElementTxn : public EditTxn
{
public:

  static const nsIID& GetCID() { static nsIID iid = DELETE_ELEMENT_TXN_CID; return iid; }
 
  /** initialize the transaction.
    * @param aElement the node to delete
    */
  NS_IMETHOD Init(nsIDOMNode *aElement);

private:
  DeleteElementTxn();

public:

  virtual ~DeleteElementTxn();

  NS_IMETHOD DoTransaction(void);

  NS_IMETHOD UndoTransaction(void);

  NS_IMETHOD RedoTransaction(void);

  NS_IMETHOD Merge(nsITransaction *aTransaction, PRBool *aDidMerge);

  NS_IMETHOD GetTxnDescription(nsAWritableString& aTxnDescription);

protected:
  
  /** the element to delete */
  nsCOMPtr<nsIDOMNode> mElement;

  /** the node into which the new node will be inserted */
  nsCOMPtr<nsIDOMNode> mParent;

  /** the index in mParent for the new node */
  PRUint32 mOffsetInParent;

  /** the node we will insert mNewNode before.  We compute this ourselves. */
  nsCOMPtr<nsIDOMNode> mRefNode;

  friend class TransactionFactory;

};

#endif
