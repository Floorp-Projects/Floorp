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

#ifndef AggregatePlaceholderTxn_h__
#define AggregatePlaceholderTxn_h__

#include "EditAggregateTxn.h"
#include "nsEditorUtils.h"
#include "nsIAbsorbingTransaction.h"
#include "nsIDOMNode.h"
#include "nsCOMPtr.h"
#include "nsWeakPtr.h"
#include "nsWeakReference.h"

#define PLACEHOLDER_TXN_CID \
{/* {0CE9FB00-D9D1-11d2-86DE-000064657374} */ \
0x0CE9FB00, 0xD9D1, 0x11d2, \
{0x86, 0xde, 0x0, 0x0, 0x64, 0x65, 0x73, 0x74} }

class nsHTMLEditor;
class IMETextTxn;

/**
 * An aggregate transaction that knows how to absorb all subsequent
 * transactions with the same name.  This transaction does not "Do" anything.
 * But it absorbs other transactions via merge, and can undo/redo the
 * transactions it has absorbed.
 */
 
class PlaceholderTxn : public EditAggregateTxn, 
                       public nsIAbsorbingTransaction, 
                       public nsSupportsWeakReference
{
public:

  static const nsIID& GetCID() { static nsIID iid = PLACEHOLDER_TXN_CID; return iid; }

  NS_DECL_ISUPPORTS_INHERITED  
  
private:
  PlaceholderTxn();

public:

  virtual ~PlaceholderTxn();

// ------------ EditAggregateTxn -----------------------

  NS_IMETHOD DoTransaction(void);

  NS_IMETHOD UndoTransaction(void);
  
  NS_IMETHOD RedoTransaction(void);

  NS_IMETHOD Merge(nsITransaction *aTransaction, PRBool *aDidMerge);

  NS_IMETHOD GetTxnDescription(nsAWritableString& aTxnDescription);

// ------------ nsIAbsorbingTransaction -----------------------

  NS_IMETHOD Init(nsIAtom *aName, nsSelectionState *aSelState, nsIEditor *aEditor);
  
  NS_IMETHOD GetTxnName(nsIAtom **aName);
  
  NS_IMETHOD StartSelectionEquals(nsSelectionState *aSelState, PRBool *aResult);

  NS_IMETHOD EndPlaceHolderBatch();

  NS_IMETHOD ForwardEndBatchTo(nsIAbsorbingTransaction *aForwardingAddress);

  NS_IMETHOD Commit();

  NS_IMETHOD RememberEndingSelection();
  
  friend class TransactionFactory;

protected:

  /** the presentation shell, which we'll need to get the selection */
  PRBool      mAbsorb;          // do we auto absorb any and all transaction?
  nsWeakPtr   mForwarding;
  IMETextTxn *mIMETextTxn;      // first IME txn in this placeholder - used for IME merging
                                // non-owning for now - cant nsCOMPtr it due to broken transaction interfaces
  PRBool      mCommitted;       // do we stop auto absorbing any matching placeholder txns?
  // these next two members store the state of the selection in a safe way. 
  // selection at the start of the txn is stored, as is the selection at the end.
  // This is so that UndoTransaction() and RedoTransaction() can restore the
  // selection properly.
  nsSelectionState *mStartSel; // use a pointer because this is constructed before we exist
  nsSelectionState  mEndSel;
  nsIEditor*        mEditor;   /** the editor for this transaction */
};


#endif
