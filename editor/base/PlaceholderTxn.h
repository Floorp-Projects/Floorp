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

  NS_IMETHOD Do(void);

  NS_IMETHOD Undo(void);

  NS_IMETHOD Merge(PRBool *aDidMerge, nsITransaction *aTransaction);

// ------------ nsIAbsorbingTransaction -----------------------

  NS_IMETHOD Init(nsWeakPtr aPresShellWeak, nsIAtom *aName, nsIDOMNode *aStartNode, PRInt32 aStartOffset);
  
  NS_IMETHOD GetTxnName(nsIAtom **aName);
  
  NS_IMETHOD GetStartNodeAndOffset(nsCOMPtr<nsIDOMNode> *aTxnStartNode, PRInt32 *aTxnStartOffset);

  NS_IMETHOD EndPlaceHolderBatch();

  NS_IMETHOD ForwardEndBatchTo(nsIAbsorbingTransaction *aForwardingAddress);

  NS_IMETHOD Commit();

  friend class TransactionFactory;

  enum { kTransactionID = 11260 };

protected:

  /** the presentation shell, which we'll need to get the selection */
  nsWeakPtr   mPresShellWeak;   // weak reference to the nsIPresShell
  PRBool      mAbsorb;          // do we auto absorb any and all transaction?
  nsCOMPtr<nsIDOMNode> mStartNode, mEndNode; // selection nodes at beginning and end of operation
  PRInt32     mStartOffset, mEndOffset;      // selection offsets at beginning and end of operation
  nsWeakPtr   mForwarding;
  IMETextTxn *mIMETextTxn;      // first IME txn in this placeholder - used for IME merging
                                // non-owning for now - cant nsCOMPtr it due to broken transaction interfaces
  PRBool      mCommitted;       // do we stop auto absorbing any matching placeholder txns?
};


#endif
