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
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
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
  virtual nsresult GetIsBatch(PRBool *aIsBatch);
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
