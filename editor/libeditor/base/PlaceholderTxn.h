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

#ifndef AggregatePlaceholderTxn_h__
#define AggregatePlaceholderTxn_h__

#include "EditAggregateTxn.h"
#include "nsEditorUtils.h"
#include "nsIAbsorbingTransaction.h"
#include "nsIDOMNode.h"
#include "nsCOMPtr.h"
#include "nsWeakPtr.h"
#include "nsWeakReference.h"
#include "nsAutoPtr.h"

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
  NS_DECL_ISUPPORTS_INHERITED  
  
  PlaceholderTxn();

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(PlaceholderTxn, EditAggregateTxn)
// ------------ EditAggregateTxn -----------------------

  NS_DECL_EDITTXN

  NS_IMETHOD RedoTransaction();
  NS_IMETHOD Merge(nsITransaction *aTransaction, bool *aDidMerge);

// ------------ nsIAbsorbingTransaction -----------------------

  NS_IMETHOD Init(nsIAtom *aName, nsSelectionState *aSelState, nsIEditor *aEditor);
  
  NS_IMETHOD GetTxnName(nsIAtom **aName);
  
  NS_IMETHOD StartSelectionEquals(nsSelectionState *aSelState, bool *aResult);

  NS_IMETHOD EndPlaceHolderBatch();

  NS_IMETHOD ForwardEndBatchTo(nsIAbsorbingTransaction *aForwardingAddress);

  NS_IMETHOD Commit();

  NS_IMETHOD RememberEndingSelection();

protected:

  /** the presentation shell, which we'll need to get the selection */
  bool        mAbsorb;          // do we auto absorb any and all transaction?
  nsWeakPtr   mForwarding;
  IMETextTxn *mIMETextTxn;      // first IME txn in this placeholder - used for IME merging
                                // non-owning for now - can't nsCOMPtr it due to broken transaction interfaces
  bool        mCommitted;       // do we stop auto absorbing any matching placeholder txns?
  // these next two members store the state of the selection in a safe way. 
  // selection at the start of the txn is stored, as is the selection at the end.
  // This is so that UndoTransaction() and RedoTransaction() can restore the
  // selection properly.
  nsAutoPtr<nsSelectionState> mStartSel; // use a pointer because this is constructed before we exist
  nsSelectionState  mEndSel;
  nsIEditor*        mEditor;   /** the editor for this transaction */
};


#endif
