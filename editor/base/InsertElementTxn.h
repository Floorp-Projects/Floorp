/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 1998-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

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
