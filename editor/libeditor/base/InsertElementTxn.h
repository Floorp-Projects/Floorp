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

#ifndef InsertElementTxn_h__
#define InsertElementTxn_h__

#include "EditTxn.h"
#include "nsIEditor.h"
#include "nsIDOMNode.h"
#include "nsCOMPtr.h"

#define INSERT_ELEMENT_TXN_CID \
{/* b5762440-cbb0-11d2-86db-000064657374 */ \
0xb5762440, 0xcbb0, 0x11d2, \
{0x86, 0xdb, 0x0, 0x0, 0x64, 0x65, 0x73, 0x74} }

/**
 * A transaction that inserts a single element
 */
class InsertElementTxn : public EditTxn
{
public:

  static const nsIID& GetCID() { static nsIID iid = INSERT_ELEMENT_TXN_CID; return iid; }

  /** initialize the transaction.
    * @param aNode   the node to insert
    * @param aParent the node to insert into
    * @param aOffset the offset in aParent to insert aNode
    */
  NS_IMETHOD Init(nsIDOMNode *aNode,
                  nsIDOMNode *aParent,
                  PRInt32     aOffset,
                  nsIEditor  *aEditor);

private:
  InsertElementTxn();

public:

  virtual ~InsertElementTxn();

  NS_IMETHOD DoTransaction(void);

  NS_IMETHOD UndoTransaction(void);

  NS_IMETHOD Merge(nsITransaction *aTransaction, PRBool *aDidMerge);

  NS_IMETHOD GetTxnDescription(nsAWritableString& aTxnDescription);

protected:
  
  /** the element to insert */
  nsCOMPtr<nsIDOMNode> mNode;

  /** the node into which the new node will be inserted */
  nsCOMPtr<nsIDOMNode> mParent;

  /** the editor for this transaction */
  nsIEditor*           mEditor;

  /** the index in mParent for the new node */
  PRInt32 mOffset;

  friend class TransactionFactory;

};

#endif
