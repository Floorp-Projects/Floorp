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
 *   Pierre Phaneuf <pp@ludusdesign.com>
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

#include "PlaceholderTxn.h"
#include "nsVoidArray.h"
#include "nsEditor.h"
#include "nsIPresShell.h"
#include "IMETextTxn.h"

#if defined(NS_DEBUG) && defined(DEBUG_buster)
static PRBool gNoisy = PR_TRUE;
#else
static const PRBool gNoisy = PR_FALSE;
#endif


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


PlaceholderTxn::~PlaceholderTxn()
{
  delete mStartSel;
}

NS_IMPL_ADDREF_INHERITED(PlaceholderTxn, EditAggregateTxn)
NS_IMPL_RELEASE_INHERITED(PlaceholderTxn, EditAggregateTxn)

//NS_IMPL_QUERY_INTERFACE_INHERITED1(Class, Super, AdditionalInterface)
NS_IMETHODIMP PlaceholderTxn::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (!aInstancePtr) return NS_ERROR_NULL_POINTER;
 
  if (aIID.Equals(NS_GET_IID(nsIAbsorbingTransaction))) {
    *aInstancePtr = (nsISupports*)(nsIAbsorbingTransaction*)(this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(NS_GET_IID(nsISupportsWeakReference))) {
    *aInstancePtr = (nsISupports*)(nsISupportsWeakReference*) this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return EditAggregateTxn::QueryInterface(aIID, aInstancePtr);
}

NS_IMETHODIMP PlaceholderTxn::Init(nsIAtom *aName, nsSelectionState *aSelState, nsIEditor *aEditor)
{
  if (!aEditor || !aSelState) return NS_ERROR_NULL_POINTER;

  mName = aName;
  mStartSel = aSelState;
  mEditor = aEditor;
  return NS_OK;
}

NS_IMETHODIMP PlaceholderTxn::DoTransaction(void)
{
  if (gNoisy) { printf("PlaceholderTxn Do\n"); }
  return NS_OK;
}

NS_IMETHODIMP PlaceholderTxn::UndoTransaction(void)
{
  // undo txns
  nsresult res = EditAggregateTxn::UndoTransaction();
  if (NS_FAILED(res)) return res;
  
  // now restore selection
  nsCOMPtr<nsISelection> selection;
  res = mEditor->GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(res)) return res;
  if (!selection) return NS_ERROR_NULL_POINTER;
  if (!mStartSel) return NS_ERROR_NULL_POINTER;
  res = mStartSel->RestoreSelection(selection);
  return res;
}


NS_IMETHODIMP PlaceholderTxn::RedoTransaction(void)
{
  // redo txns
  nsresult res = EditAggregateTxn::RedoTransaction();
  if (NS_FAILED(res)) return res;
  
  // now restore selection
  nsCOMPtr<nsISelection> selection;
  res = mEditor->GetSelection(getter_AddRefs(selection));
  if (NS_FAILED(res)) return res;
  if (!selection) return NS_ERROR_NULL_POINTER;
  res = mEndSel.RestoreSelection(selection);
  return res;
}


NS_IMETHODIMP PlaceholderTxn::Merge(nsITransaction *aTransaction, PRBool *aDidMerge)
{
  if (!aDidMerge || !aTransaction) return NS_ERROR_NULL_POINTER;

  // set out param default value
  *aDidMerge=PR_FALSE;
    
  nsresult res = NS_OK;
    
  if (mForwarding) 
  {
    NS_NOTREACHED("tried to merge into a placeholder that was in forwarding mode!");
    return NS_ERROR_FAILURE;
  }

  EditTxn *editTxn = (EditTxn*)aTransaction;  //XXX: hack, not safe!  need nsIEditTransaction!
  if (PR_TRUE==mAbsorb)
  { // yep, it's one of ours.  Assimilate it.
    IMETextTxn*  otherTxn = nsnull;
    if (NS_SUCCEEDED(aTransaction->QueryInterface(IMETextTxn::GetCID(),(void**)&otherTxn)) && otherTxn)
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
        PRBool didMerge;
        mIMETextTxn->Merge(otherTxn, &didMerge);
        if (!didMerge)
        {
          // it wouldn't merge.  Earlier IME txn is already commited and will 
          // not absorb frther IME txns.  So just stack this one after it
          // and remember it as a candidate for furthre merges.
          mIMETextTxn =otherTxn;
          AppendChild(editTxn);
        }
      }
      NS_IF_RELEASE(otherTxn);
    }
    else
    {
      AppendChild(editTxn);
    }
    *aDidMerge = PR_TRUE;
//  RememberEndingSelection();
//  efficiency hack: no need to remember selection here, as we haven't yet 
//  finished the inital batch and we know we will be told when the batch ends.
//  we can remeber the selection then.
    if (gNoisy) { printf("Placeholder txn assimilated %p\n", (void*)aTransaction); }
  }
  else
  { // merge typing or IME or deletion transactions if the selection matches
    if (((mName.get() == nsEditor::gTypingTxnName) ||
         (mName.get() == nsEditor::gIMETxnName)    ||
         (mName.get() == nsEditor::gDeleteTxnName)) 
         && !mCommitted ) 
    {
      // but only if this placeholder started with a collapsed selection
      if (mStartSel->IsCollapsed())
      {
        nsCOMPtr<nsIAbsorbingTransaction> plcTxn;// = do_QueryInterface(editTxn);
        // cant do_QueryInterface() above due to our broken transaction interfaces.
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
            PRBool isSame;
            plcTxn->StartSelectionEquals(&mEndSel, &isSame);
            if (isSame)
            {
              mAbsorb = PR_TRUE;  // we need to start absorbing again
              plcTxn->ForwardEndBatchTo(this);
              AppendChild(editTxn);
              RememberEndingSelection();
              *aDidMerge = PR_TRUE;
            }
          }
        }
      }
    }
  }
  return res;
}

NS_IMETHODIMP PlaceholderTxn::GetTxnDescription(nsAWritableString& aString)
{
  aString.Assign(NS_LITERAL_STRING("PlaceholderTxn: "));

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

NS_IMETHODIMP PlaceholderTxn::StartSelectionEquals(nsSelectionState *aSelState, PRBool *aResult)
{
  // determine if starting selection matches the given selection state.
  // note that we only care about collapsed selections.
  if (!aResult || !aSelState) return NS_ERROR_NULL_POINTER;
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
};

NS_IMETHODIMP PlaceholderTxn::ForwardEndBatchTo(nsIAbsorbingTransaction *aForwardingAddress)
{   
  mForwarding = getter_AddRefs( NS_GetWeakReference(aForwardingAddress) );
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
  if (NS_FAILED(res)) return res;
  if (!selection) return NS_ERROR_NULL_POINTER;
  res = mEndSel.SaveSelection(selection);
  
  return res;
}


