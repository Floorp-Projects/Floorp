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

#include "nsITransactionManager.h"
#include "nsTransactionStack.h"

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
  virtual nsresult Do(nsITransaction *aTransaction);
  virtual nsresult Undo(void);
  virtual nsresult Redo(void);
  virtual nsresult Clear(void);
  virtual nsresult BeginBatch(void);
  virtual nsresult EndBatch(void);
  virtual nsresult GetNumberOfUndoItems(PRInt32 *aNumItems);
  virtual nsresult GetNumberOfRedoItems(PRInt32 *aNumItems);
  virtual nsresult SetMaxTransactionCount(PRInt32 aMaxCount);
  virtual nsresult PeekUndoStack(nsITransaction **aTransaction);
  virtual nsresult PeekRedoStack(nsITransaction **aTransaction);
  virtual nsresult Write(nsIOutputStream *aOutputStream);
  virtual nsresult AddListener(nsITransactionListener *aListener);
  virtual nsresult RemoveListener(nsITransactionListener *aListener);

  /* nsTransactionManager specific methods. */
  virtual nsresult ClearUndoStack(void);
  virtual nsresult ClearRedoStack(void);

private:

  /* nsTransactionManager specific private methods. */
  virtual nsresult BeginTransaction(nsITransaction *aTransaction);
  virtual nsresult EndTransaction(void);
};

#endif // nsTransactionManager_h__
