/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "EditAggregateTxn.h"
#include "nsCOMPtr.h"

EditAggregateTxn::EditAggregateTxn()
  : EditTxn()
{
}
NS_IMPL_CYCLE_COLLECTION_CLASS(EditAggregateTxn)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(EditAggregateTxn, EditTxn)
  tmp->mChildren.Clear();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(EditAggregateTxn, EditTxn)
  for (PRUint32 i = 0; i < tmp->mChildren.Length(); ++i) {
    NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mChildren[i]");
    cb.NoteXPCOMChild(static_cast<nsITransaction*>(tmp->mChildren[i]));
  }
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_ADDREF_INHERITED(EditAggregateTxn, EditTxn)
NS_IMPL_RELEASE_INHERITED(EditAggregateTxn, EditTxn)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(EditAggregateTxn)
NS_INTERFACE_MAP_END_INHERITING(EditTxn)

NS_IMETHODIMP EditAggregateTxn::DoTransaction(void)
{
  nsresult result=NS_OK;  // it's legal (but not very useful) to have an empty child list
  for (PRUint32 i = 0, length = mChildren.Length(); i < length; ++i)
  {
    nsITransaction *txn = mChildren[i];
    if (!txn) { return NS_ERROR_NULL_POINTER; }
    result = txn->DoTransaction();
    if (NS_FAILED(result))
      break;
  }
  return result;
}

NS_IMETHODIMP EditAggregateTxn::UndoTransaction(void)
{
  nsresult result=NS_OK;  // it's legal (but not very useful) to have an empty child list
  // undo goes through children backwards
  for (PRUint32 i = mChildren.Length(); i-- != 0; )
  {
    nsITransaction *txn = mChildren[i];
    if (!txn) { return NS_ERROR_NULL_POINTER; }
    result = txn->UndoTransaction();
    if (NS_FAILED(result))
      break;
  }
  return result;
}

NS_IMETHODIMP EditAggregateTxn::RedoTransaction(void)
{
  nsresult result=NS_OK;  // it's legal (but not very useful) to have an empty child list
  for (PRUint32 i = 0, length = mChildren.Length(); i < length; ++i)
  {
    nsITransaction *txn = mChildren[i];
    if (!txn) { return NS_ERROR_NULL_POINTER; }
    result = txn->RedoTransaction();
    if (NS_FAILED(result))
      break;
  }
  return result;
}

NS_IMETHODIMP EditAggregateTxn::Merge(nsITransaction *aTransaction, bool *aDidMerge)
{
  nsresult result=NS_OK;  // it's legal (but not very useful) to have an empty child list
  if (aDidMerge)
    *aDidMerge = PR_FALSE;
  // FIXME: Is this really intended not to loop?  It looks like the code
  // that used to be here sort of intended to loop, but didn't.
  if (mChildren.Length() > 0)
  {
    nsITransaction *txn = mChildren[0];
    if (!txn) { return NS_ERROR_NULL_POINTER; }
    result = txn->Merge(aTransaction, aDidMerge);
  }
  return result;
}

NS_IMETHODIMP EditAggregateTxn::GetTxnDescription(nsAString& aString)
{
  aString.AssignLiteral("EditAggregateTxn: ");

  if (mName)
  {
    nsAutoString name;
    mName->ToString(name);
    aString += name;
  }

  return NS_OK;
}

NS_IMETHODIMP EditAggregateTxn::AppendChild(EditTxn *aTxn)
{
  if (!aTxn) {
    return NS_ERROR_NULL_POINTER;
  }

  nsRefPtr<EditTxn> *slot = mChildren.AppendElement();
  if (!slot) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  *slot = aTxn;
  return NS_OK;
}

NS_IMETHODIMP EditAggregateTxn::GetCount(PRUint32 *aCount)
{
  if (!aCount) {
    return NS_ERROR_NULL_POINTER;
  }
  *aCount = mChildren.Length();
  return NS_OK;
}

NS_IMETHODIMP EditAggregateTxn::GetTxnAt(PRInt32 aIndex, EditTxn **aTxn)
{
  // preconditions
  NS_PRECONDITION(aTxn, "null out param");

  if (!aTxn) {
    return NS_ERROR_NULL_POINTER;
  }
  *aTxn = nsnull; // initialize out param as soon as we know it's a valid pointer
  // get the transaction at aIndex
  PRUint32 txnCount = mChildren.Length();
  if (0>aIndex || ((PRInt32)txnCount)<=aIndex) {
    return NS_ERROR_UNEXPECTED;
  }
  // ugh, this is all wrong - what a mess we have with editor transaction interfaces
  *aTxn = mChildren[aIndex];
  NS_ENSURE_TRUE(*aTxn, NS_ERROR_UNEXPECTED);
  NS_ADDREF(*aTxn);
  return NS_OK;
}


NS_IMETHODIMP EditAggregateTxn::SetName(nsIAtom *aName)
{
  mName = do_QueryInterface(aName);
  return NS_OK;
}

NS_IMETHODIMP EditAggregateTxn::GetName(nsIAtom **aName)
{
  if (aName && mName)
  {
    *aName = mName;
    NS_ADDREF(*aName);
    return NS_OK;
  }
  return NS_ERROR_NULL_POINTER;
}
