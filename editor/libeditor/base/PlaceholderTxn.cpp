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
 *   Pierre Phaneuf <pp@ludusdesign.com>
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

#include "PlaceholderTxn.h"
#include "nsEditor.h"
#include "IMETextTxn.h"
#include "nsGkAtoms.h"

PlaceholderTxn::PlaceholderTxn() :  EditAggregateTxn(), 
                                    mAbsorb(PR_TRUE), 
                                    mForwarding(nsnull),
                                    mIMETextTxn(nsnull),
                                    mCommitted(PR_FALSE),
                                    mStartSel(nsnull),
                                    mEndSel(),
                                    mEditor(nsnull)
{
}

NS_IMPL_CYCLE_COLLECTION_CLASS(PlaceholderTxn)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(PlaceholderTxn,
                                                EditAggregateTxn)
  tmp->mStartSel->DoUnlink();
  tmp->mEndSel.DoUnlink();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(PlaceholderTxn,
                                                  EditAggregateTxn)
  tmp->mStartSel->DoTraverse(cb);
  tmp->mEndSel.DoTraverse(cb);
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(PlaceholderTxn)
  NS_INTERFACE_MAP_ENTRY(nsIAbsorbingTransaction)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
NS_INTERFACE_MAP_END_INHERITING(EditAggregateTxn)

NS_IMPL_ADDREF_INHERITED(PlaceholderTxn, EditAggregateTxn)
NS_IMPL_RELEASE_INHERITED(PlaceholderTxn, EditAggregateTxn)

NS_IMETHODIMP PlaceholderTxn::Init(nsIAtom *aName, nsSelectionState *aSelState, nsIEditor *aEditor)
{
  NS_ENSURE_TRUE(aEditor && aSelState, NS_ERROR_NULL_POINTER);

  mName = aName;
  mStartSel = aSelState;
  mEditor = aEditor;
  return NS_OK;
}

NS_IMETHODIMP PlaceholderTxn::DoTransaction(void)
{
  return NS_OK;
}

NS_IMETHODIMP PlaceholderTxn::UndoTransaction(void)
{
  // undo txns
  nsresult res = EditAggregateTxn::UndoTransaction();
  NS_ENSURE_SUCCESS(res, res);
  
  NS_ENSURE_TRUE(mStartSel, NS_ERROR_NULL_POINTER);

  // now restore selection
  nsCOMPtr<nsISelection> selection;
  res = mEditor->GetSelection(getter_AddRefs(selection));
  NS_ENSURE_SUCCESS(res, res);
  NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);
  return mStartSel->RestoreSelection(selection);
}


NS_IMETHODIMP PlaceholderTxn::RedoTransaction(void)
{
  // redo txns
  nsresult res = EditAggregateTxn::RedoTransaction();
  NS_ENSURE_SUCCESS(res, res);
  
  // now restore selection
  nsCOMPtr<nsISelection> selection;
  res = mEditor->GetSelection(getter_AddRefs(selection));
  NS_ENSURE_SUCCESS(res, res);
  NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);
  return mEndSel.RestoreSelection(selection);
}


NS_IMETHODIMP PlaceholderTxn::Merge(nsITransaction *aTransaction, bool *aDidMerge)
{
  NS_ENSURE_TRUE(aDidMerge && aTransaction, NS_ERROR_NULL_POINTER);

  // set out param default value
  *aDidMerge=PR_FALSE;
    
  if (mForwarding) 
  {
    NS_NOTREACHED("tried to merge into a placeholder that was in forwarding mode!");
    return NS_ERROR_FAILURE;
  }

  // check to see if aTransaction is one of the editor's
  // private transactions. If not, we want to avoid merging
  // the foreign transaction into our placeholder since we
  // don't know what it does.

  nsCOMPtr<nsPIEditorTransaction> pTxn = do_QueryInterface(aTransaction);
  NS_ENSURE_TRUE(pTxn, NS_OK); // it's foreign so just bail!

  EditTxn *editTxn = (EditTxn*)aTransaction;  //XXX: hack, not safe!  need nsIEditTransaction!
  // determine if this incoming txn is a placeholder txn
  nsCOMPtr<nsIAbsorbingTransaction> plcTxn;// = do_QueryInterface(editTxn);
  // can't do_QueryInterface() above due to our broken transaction interfaces.
  // instead have to brute it below. ugh. 
  editTxn->QueryInterface(NS_GET_IID(nsIAbsorbingTransaction), getter_AddRefs(plcTxn));

  // we are absorbing all txn's if mAbsorb is lit.
  if (mAbsorb)
  { 
    nsRefPtr<IMETextTxn> otherTxn;
    if (NS_SUCCEEDED(aTransaction->QueryInterface(IMETextTxn::GetCID(), getter_AddRefs(otherTxn))) && otherTxn)
    {
      // special handling for IMETextTxn's: they need to merge with any previous
      // IMETextTxn in this placeholder, if possible.
      if (!mIMETextTxn) 
      {
        // this is the first IME txn in the placeholder
        mIMETextTxn =otherTxn;
        AppendChild(editTxn);
      }
      else  
      {
        bool didMerge;
        mIMETextTxn->Merge(otherTxn, &didMerge);
        if (!didMerge)
        {
          // it wouldn't merge.  Earlier IME txn is already committed and will
          // not absorb further IME txns.  So just stack this one after it
          // and remember it as a candidate for further merges.
          mIMETextTxn =otherTxn;
          AppendChild(editTxn);
        }
      }
    }
    else if (!plcTxn)  // see bug 171243: just drop incoming placeholders on the floor.
    {                  // their children will be swallowed by this preexisting one.
      AppendChild(editTxn);
    }
    *aDidMerge = PR_TRUE;
//  RememberEndingSelection();
//  efficiency hack: no need to remember selection here, as we haven't yet 
//  finished the initial batch and we know we will be told when the batch ends.
//  we can remeber the selection then.
  }
  else
  { // merge typing or IME or deletion transactions if the selection matches
    if (((mName.get() == nsGkAtoms::TypingTxnName) ||
         (mName.get() == nsGkAtoms::IMETxnName)    ||
         (mName.get() == nsGkAtoms::DeleteTxnName)) 
         && !mCommitted ) 
    {
      nsCOMPtr<nsIAbsorbingTransaction> plcTxn;// = do_QueryInterface(editTxn);
      // can't do_QueryInterface() above due to our broken transaction interfaces.
      // instead have to brute it below. ugh. 
      editTxn->QueryInterface(NS_GET_IID(nsIAbsorbingTransaction), getter_AddRefs(plcTxn));
      if (plcTxn)
      {
        nsCOMPtr<nsIAtom> atom;
        plcTxn->GetTxnName(getter_AddRefs(atom));
        if (atom && (atom == mName))
        {
          // check if start selection of next placeholder matches
          // end selection of this placeholder
          bool isSame;
          plcTxn->StartSelectionEquals(&mEndSel, &isSame);
          if (isSame)
          {
            mAbsorb = PR_TRUE;  // we need to start absorbing again
            plcTxn->ForwardEndBatchTo(this);
            // AppendChild(editTxn);
            // see bug 171243: we don't need to merge placeholders
            // into placeholders.  We just reactivate merging in the pre-existing
            // placeholder and drop the new one on the floor.  The EndPlaceHolderBatch()
            // call on the new placeholder will be forwarded to this older one.
            RememberEndingSelection();
            *aDidMerge = PR_TRUE;
          }
        }
      }
    }
  }
  return NS_OK;
}

NS_IMETHODIMP PlaceholderTxn::GetTxnDescription(nsAString& aString)
{
  aString.AssignLiteral("PlaceholderTxn: ");

  if (mName)
  {
    nsAutoString name;
    mName->ToString(name);
    aString += name;
  }

  return NS_OK;
}

NS_IMETHODIMP PlaceholderTxn::GetTxnName(nsIAtom **aName)
{
  return GetName(aName);
}

NS_IMETHODIMP PlaceholderTxn::StartSelectionEquals(nsSelectionState *aSelState, bool *aResult)
{
  // determine if starting selection matches the given selection state.
  // note that we only care about collapsed selections.
  NS_ENSURE_TRUE(aResult && aSelState, NS_ERROR_NULL_POINTER);
  if (!mStartSel->IsCollapsed() || !aSelState->IsCollapsed())
  {
    *aResult = PR_FALSE;
    return NS_OK;
  }
  *aResult = mStartSel->IsEqual(aSelState);
  return NS_OK;
}

NS_IMETHODIMP PlaceholderTxn::EndPlaceHolderBatch()
{
  mAbsorb = PR_FALSE;
  
  if (mForwarding) 
  {
    nsCOMPtr<nsIAbsorbingTransaction> plcTxn = do_QueryReferent(mForwarding);
    if (plcTxn) plcTxn->EndPlaceHolderBatch();
  }
  
  // remember our selection state.
  return RememberEndingSelection();
}

NS_IMETHODIMP PlaceholderTxn::ForwardEndBatchTo(nsIAbsorbingTransaction *aForwardingAddress)
{   
  mForwarding = do_GetWeakReference(aForwardingAddress);
  return NS_OK;
}

NS_IMETHODIMP PlaceholderTxn::Commit()
{
  mCommitted = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP PlaceholderTxn::RememberEndingSelection()
{
  nsCOMPtr<nsISelection> selection;
  nsresult res = mEditor->GetSelection(getter_AddRefs(selection));
  NS_ENSURE_SUCCESS(res, res);
  NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);
  return mEndSel.SaveSelection(selection);
}

