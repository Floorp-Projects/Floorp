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

#include "nsITransaction.h"
#include "nsTransactionStack.h"

class nsTransactionStack;
class nsTransactionRedoStack;

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

  virtual nsresult Do(void);
  virtual nsresult Undo(void);
  virtual nsresult Redo(void);
  virtual nsresult Write(nsIOutputStream *aOutputStream);

private:

  virtual nsresult RecoverFromUndo();
  virtual nsresult RecoverFromRedo();

  virtual nsresult GetNumberOfUndoItems(PRInt32 *aNumItems);
  virtual nsresult GetNumberOfRedoItems(PRInt32 *aNumItems);
};

#endif // nsTransactionItem_h__
