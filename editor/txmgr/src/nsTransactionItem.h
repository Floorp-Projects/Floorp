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
 * Portions created by the Initial Developer are Copyright (C) 1998
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

#ifndef nsTransactionItem_h__
#define nsTransactionItem_h__

#include "nsITransaction.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"

class nsTransactionStack;
class nsTransactionManager;

class nsTransactionItem
{
  nsCOMPtr<nsITransaction> mTransaction;
  nsTransactionStack      *mUndoStack;
  nsTransactionStack      *mRedoStack;
  nsAutoRefCnt             mRefCnt;

public:

  nsTransactionItem(nsITransaction *aTransaction);
  virtual ~nsTransactionItem();
  nsrefcnt AddRef();
  nsrefcnt Release();

  NS_DECL_CYCLE_COLLECTION_NATIVE_CLASS(nsTransactionItem)

  virtual nsresult AddChild(nsTransactionItem *aTransactionItem);
  virtual nsresult GetTransaction(nsITransaction **aTransaction);
  virtual nsresult GetIsBatch(bool *aIsBatch);
  virtual nsresult GetNumberOfChildren(PRInt32 *aNumChildren);
  virtual nsresult GetChild(PRInt32 aIndex, nsTransactionItem **aChild);

  virtual nsresult DoTransaction(void);
  virtual nsresult UndoTransaction(nsTransactionManager *aTxMgr);
  virtual nsresult RedoTransaction(nsTransactionManager *aTxMgr);

private:

  virtual nsresult UndoChildren(nsTransactionManager *aTxMgr);
  virtual nsresult RedoChildren(nsTransactionManager *aTxMgr);

  virtual nsresult RecoverFromUndoError(nsTransactionManager *aTxMgr);
  virtual nsresult RecoverFromRedoError(nsTransactionManager *aTxMgr);

  virtual nsresult GetNumberOfUndoItems(PRInt32 *aNumItems);
  virtual nsresult GetNumberOfRedoItems(PRInt32 *aNumItems);
};

#endif // nsTransactionItem_h__
