/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://wwwt.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef nsTransactionManager_h__
#define nsTransactionManager_h__

class nsITransaction;
class nsITransactionManager;
class nsITransactionListener;
class nsTransactionItem;
class nsTransactionStack;
class nsTransactionRedoStack;
class nsVoidArray;

/** implementation of a transaction manager object.
 *
 */
class nsTransactionManager : public nsITransactionManager
{
private:

  PRInt32                mMaxTransactionCount;
  nsTransactionStack     mDoStack;
  nsTransactionStack     mUndoStack;
  nsTransactionRedoStack mRedoStack;
  nsVoidArray            *mListeners;

public:

  /** The default constructor.
   */
  nsTransactionManager(PRInt32 aMaxTransactionCount=-1);

  /** The default destructor.
   */
  virtual ~nsTransactionManager();

  /* Macro for AddRef(), Release(), and QueryInterface() */
  NS_DECL_ISUPPORTS

  /* nsITransactionManager method implementations. */
  NS_IMETHOD Do(nsITransaction *aTransaction);
  NS_IMETHOD Undo(void);
  NS_IMETHOD Redo(void);
  NS_IMETHOD Clear(void);
  NS_IMETHOD BeginBatch(void);
  NS_IMETHOD EndBatch(void);
  NS_IMETHOD GetNumberOfUndoItems(PRInt32 *aNumItems);
  NS_IMETHOD GetNumberOfRedoItems(PRInt32 *aNumItems);
  NS_IMETHOD SetMaxTransactionCount(PRInt32 aMaxCount);
  NS_IMETHOD PeekUndoStack(nsITransaction **aTransaction);
  NS_IMETHOD PeekRedoStack(nsITransaction **aTransaction);
  NS_IMETHOD Write(nsIOutputStream *aOutputStream);
  NS_IMETHOD AddListener(nsITransactionListener *aListener);
  NS_IMETHOD RemoveListener(nsITransactionListener *aListener);

  /* nsTransactionManager specific methods. */
  virtual nsresult ClearUndoStack(void);
  virtual nsresult ClearRedoStack(void);

  virtual nsresult WillDoNotify(nsITransaction *aTransaction);
  virtual nsresult DidDoNotify(nsITransaction *aTransaction, nsresult aDoResult);
  virtual nsresult WillUndoNotify(nsITransaction *aTransaction);
  virtual nsresult DidUndoNotify(nsITransaction *aTransaction, nsresult aUndoResult);
  virtual nsresult WillRedoNotify(nsITransaction *aTransaction);
  virtual nsresult DidRedoNotify(nsITransaction *aTransaction, nsresult aRedoResult);
  virtual nsresult WillBeginBatchNotify();
  virtual nsresult DidBeginBatchNotify(nsresult aResult);
  virtual nsresult WillEndBatchNotify();
  virtual nsresult DidEndBatchNotify(nsresult aResult);
  virtual nsresult WillMergeNotify(nsITransaction *aTop,
                                   nsITransaction *aTransaction);
  virtual nsresult DidMergeNotify(nsITransaction *aTop,
                                  nsITransaction *aTransaction,
                                  PRBool aDidMerge,
                                  nsresult aMergeResult);

private:

  /* nsTransactionManager specific private methods. */
  virtual nsresult BeginTransaction(nsITransaction *aTransaction);
  virtual nsresult EndTransaction(void);
};

#endif // nsTransactionManager_h__
