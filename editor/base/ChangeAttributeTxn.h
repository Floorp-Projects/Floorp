/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef ChangeAttributeTxn_h__
#define ChangeAttributeTxn_h__

#include "EditTxn.h"
#include "nsCOMPtr.h"
#include "nsIDOMElement.h"
#include "nsIEditor.h"

#define CHANGE_ATTRIBUTE_TXN_CID \
{/* 97818860-ac48-11d2-86d8-000064657374 */ \
0x97818860, 0xac48, 0x11d2, \
{0x86, 0xd8, 0x0, 0x0, 0x64, 0x65, 0x73, 0x74} }

/**
 * A transaction that changes an attribute of a content node. 
 * This transaction covers add, remove, and change attribute.
 */
class ChangeAttributeTxn : public EditTxn
{
public:

  static const nsIID& GetCID() { static nsIID iid = CHANGE_ATTRIBUTE_TXN_CID; return iid; }

  virtual ~ChangeAttributeTxn();

  /** Initialize the transaction.
    * @param aEditor the object providing core editing operations
    * @param aNode   the node whose attribute will be changed
    * @param aAttribute the name of the attribute to change
    * @param aValue     the new value for aAttribute, if aRemoveAttribute is false
    * @param aRemoveAttribute if PR_TRUE, remove aAttribute from aNode
    */
  NS_IMETHOD Init(nsIEditor      *aEditor,
                  nsIDOMElement  *aNode,
                  const nsAReadableString& aAttribute,
                  const nsAReadableString& aValue,
                  PRBool aRemoveAttribute);

private:
  ChangeAttributeTxn();

public:

  NS_IMETHOD DoTransaction(void);

  NS_IMETHOD UndoTransaction(void);

  NS_IMETHOD RedoTransaction(void);

  NS_IMETHOD Merge(nsITransaction *aTransaction, PRBool *aDidMerge);

  NS_IMETHOD GetTxnDescription(nsAWritableString& aTxnDescription);

protected:

  /** the editor that created this transaction */
  nsIEditor*  mEditor;
  
  /** the element to operate upon */
  nsCOMPtr<nsIDOMElement> mElement;
  
  /** the attribute to change */
  nsString mAttribute;

  /** the value to set the attribute to (ignored if mRemoveAttribute==PR_TRUE) */
  nsString mValue;

  /** the value to set the attribute to for undo */
  nsString mUndoValue;

  /** PR_TRUE if the mAttribute was set on mElement at the time of execution */
  PRBool   mAttributeWasSet;

  /** PR_TRUE if the operation is to remove mAttribute from mElement */
  PRBool   mRemoveAttribute;

  friend class TransactionFactory;
};

#endif
