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

#ifndef nsTransactionItem_h__
#define nsTransactionItem_h__

class nsITransaction;
class nsTransactionStack;
class nsTransactionRedoStack;
class nsTransactionManager;

class nsTransactionItem
{
  nsITransaction         *mTransaction;
  nsTransactionStack     *mUndoStack;
  nsTransactionRedoStack *mRedoStack;

public:

  nsTransactionItem(nsITransaction *aTransaction);
  virtual ~nsTransactionItem();

  virtual nsresult AddChild(nsTransactionItem *aTransactionItem);
  virtual nsresult GetTransaction(nsITransaction **aTransaction);
  virtual nsresult GetNumberOfChildren(PRInt32 *aNumChildren);

  virtual nsresult Do(void);
  virtual nsresult Undo(nsTransactionManager *aTxMgr);
  virtual nsresult Redo(nsTransactionManager *aTxMgr);
  virtual nsresult Write(nsIOutputStream *aOutputStream);

private:

  virtual nsresult UndoChildren(nsTransactionManager *aTxMgr);
  virtual nsresult RedoChildren(nsTransactionManager *aTxMgr);

  virtual nsresult RecoverFromUndoError(nsTransactionManager *aTxMgr);
  virtual nsresult RecoverFromRedoError(nsTransactionManager *aTxMgr);

  virtual nsresult GetNumberOfUndoItems(PRInt32 *aNumItems);
  virtual nsresult GetNumberOfRedoItems(PRInt32 *aNumItems);
};

#endif // nsTransactionItem_h__
