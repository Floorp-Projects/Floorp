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

#include "PlaceholderTxn.h"
#include "nsVoidArray.h"
#include "nsHTMLEditor.h"
#include "nsIPresShell.h"
#include "IMETextTxn.h"

#if defined(NS_DEBUG) && defined(DEBUG_buster)
static PRBool gNoisy = PR_TRUE;
#else
static const PRBool gNoisy = PR_FALSE;
#endif


PlaceholderTxn::PlaceholderTxn() :  EditAggregateTxn(), 
                                    mPresShellWeak(nsnull), 
                                    mAbsorb(PR_TRUE), 
                                    mForwarding(nsnull),
                                    mIMETextTxn(nsnull),
                                    mCommitted(PR_FALSE)
{
  SetTransactionDescriptionID( kTransactionID );
  /* log description initialized in parent constructor */
}


PlaceholderTxn::~PlaceholderTxn()
{
}

NS_IMPL_ADDREF_INHERITED(PlaceholderTxn, EditAggregateTxn)
NS_IMPL_RELEASE_INHERITED(PlaceholderTxn, EditAggregateTxn)

//NS_IMPL_QUERY_INTERFACE_INHERITED(Class, Super, AdditionalInterface)
NS_IMETHODIMP PlaceholderTxn::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (!aInstancePtr) return NS_ERROR_NULL_POINTER;
 
  if (aIID.Equals(nsIAbsorbingTransaction::GetIID())) {
    *aInstancePtr = (nsISupports*)(nsIAbsorbingTransaction*)(this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(nsCOMTypeInfo<nsISupportsWeakReference>::GetIID())) {
    *aInstancePtr = (nsISupports*)(nsISupportsWeakReference*) this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return EditAggregateTxn::QueryInterface(aIID, aInstancePtr);
}

NS_IMETHODIMP PlaceholderTxn::Init(nsWeakPtr aPresShellWeak, nsIAtom *aName, 
                                   nsIDOMNode *aStartNode, PRInt32 aStartOffset)
{
  NS_ASSERTION(aPresShellWeak, "bad args");
  if (!aPresShellWeak) return NS_ERROR_NULL_POINTER;

  mPresShellWeak = aPresShellWeak;
  mName = aName;
  mStartNode = do_QueryInterface(aStartNode);
  mStartOffset = aStartOffset;
  return NS_OK;
}

NS_IMETHODIMP PlaceholderTxn::Do(void)
{
  if (gNoisy) { printf("PlaceholderTxn Do\n"); }
  return NS_OK;
}

NS_IMETHODIMP PlaceholderTxn::Undo(void)
{
  // using this to debug
  return EditAggregateTxn::Undo();
}


NS_IMETHODIMP PlaceholderTxn::Merge(PRBool *aDidMerge, nsITransaction *aTransaction)
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
        mIMETextTxn->Merge(&didMerge, otherTxn);
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
    if (gNoisy) { printf("Placeholder txn assimilated %p\n", aTransaction); }
  }
  else
  { // merge typing or IME or deletion transactions if the selection matches
    if (((mName.get() == nsHTMLEditor::gTypingTxnName) ||
         (mName.get() == nsHTMLEditor::gIMETxnName)    ||
         (mName.get() == nsHTMLEditor::gDeleteTxnName)) 
         && !mCommitted ) 
    {
      nsCOMPtr<nsIAbsorbingTransaction> plcTxn;// = do_QueryInterface(editTxn);
      // cant do_QueryInterface() above due to our broken transaction interfaces.
      // instead have to brute it below. ugh. 
      editTxn->QueryInterface(nsIAbsorbingTransaction::GetIID(), getter_AddRefs(plcTxn));
      if (plcTxn)
      {
        nsIAtom *atom;
        plcTxn->GetTxnName(&atom);
        if (atom && (atom == mName.get()))
        {
          nsCOMPtr<nsIDOMNode> otherTxnStartNode;
          PRInt32 otherTxnStartOffset;
          res = plcTxn->GetStartNodeAndOffset(&otherTxnStartNode, &otherTxnStartOffset);
          if (NS_FAILED(res)) return res;
            
          if ((otherTxnStartNode == mEndNode) && (otherTxnStartOffset == mEndOffset))
          {
            mAbsorb = PR_TRUE;  // we need to start absorbing again
            plcTxn->ForwardEndBatchTo(this);
            AppendChild(editTxn);
            *aDidMerge = PR_TRUE;
          }
        }
        NS_IF_RELEASE(atom);
      }
    }
  }
  return res;
}

NS_IMETHODIMP PlaceholderTxn::GetTxnName(nsIAtom **aName)
{
  return GetName(aName);
}

NS_IMETHODIMP PlaceholderTxn::GetStartNodeAndOffset(nsCOMPtr<nsIDOMNode> *aTxnStartNode, PRInt32 *aTxnStartOffset)
{
  if (!aTxnStartNode || !aTxnStartOffset) return NS_ERROR_NULL_POINTER;
  *aTxnStartNode = mStartNode;
  *aTxnStartOffset = mStartOffset;
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
  
  // if we are a typing or IME or deleting transaction, remember our selection state
  if ( (mName.get() == nsHTMLEditor::gTypingTxnName) ||
       (mName.get() == nsHTMLEditor::gIMETxnName)    || 
       (mName.get() == nsHTMLEditor::gDeleteTxnName) ) 
  {
    nsCOMPtr<nsIPresShell> ps = do_QueryReferent(mPresShellWeak);
    if (!ps) return NS_ERROR_NOT_INITIALIZED;
    nsCOMPtr<nsIDOMSelection> selection;
    nsresult res = ps->GetSelection(SELECTION_NORMAL, getter_AddRefs(selection));
    if (NS_FAILED(res)) return res;
    if (!selection) return NS_ERROR_NULL_POINTER;
    res = nsEditor::GetStartNodeAndOffset(selection, &mEndNode, &mEndOffset);
    if (NS_FAILED(res)) return res;
  }
  return NS_OK;
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



