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

#ifndef nsJSEditorLog_h__
#define nsJSEditorLog_h__

#include "nsITransaction.h"
#include "nsITransactionManager.h"
#include "nsITransactionListener.h"

/** implementation of a transaction listener object.
 *
 */
class nsJSEditorLog : public nsITransactionListener
{
private:

  PRInt32 mIndentLevel;
  PRInt32 mBatchCount;

public:

  /** The default constructor.
   */
  nsJSEditorLog();

  /** The default destructor.
   */
  virtual ~nsJSEditorLog();

  /* Macro for AddRef(), Release(), and QueryInterface() */
  NS_DECL_ISUPPORTS

  /* nsITransactionListener method implementations. */
  NS_IMETHOD WillDo(nsITransactionManager *aTxMgr, nsITransaction *aTransaction);
  NS_IMETHOD DidDo(nsITransactionManager *aTxMgr, nsITransaction *aTransaction, nsresult aDoResult);
  NS_IMETHOD WillUndo(nsITransactionManager *aTxMgr, nsITransaction *aTransaction);
  NS_IMETHOD DidUndo(nsITransactionManager *aTxMgr, nsITransaction *aTransaction, nsresult aUndoResult);
  NS_IMETHOD WillRedo(nsITransactionManager *aTxMgr, nsITransaction *aTransaction);
  NS_IMETHOD DidRedo(nsITransactionManager *aTxMgr, nsITransaction *aTransaction, nsresult aRedoResult);
  NS_IMETHOD WillBeginBatch(nsITransactionManager *aTxMgr);
  NS_IMETHOD DidBeginBatch(nsITransactionManager *aTxMgr, nsresult aResult);
  NS_IMETHOD WillEndBatch(nsITransactionManager *aTxMgr);
  NS_IMETHOD DidEndBatch(nsITransactionManager *aTxMgr, nsresult aResult);
  NS_IMETHOD WillMerge(nsITransactionManager *aTxMgr, nsITransaction *aTopTransaction, nsITransaction *aTransaction);
  NS_IMETHOD DidMerge(nsITransactionManager *aTxMgr, nsITransaction *aTopTransaction, nsITransaction *aTransaction, PRBool aDidMerge, nsresult aMergeResult);


private:

  /* nsJSEditorLog private methods. */
  const char *GetString(nsITransaction *aTransaction);
  nsresult PrintIndent(PRInt32 aIndentLevel);
};

#endif // nsJSEditorLog_h__
