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

#ifndef nsEditorTxnLog_h__
#define nsEditorTxnLog_h__

#include "nsITransaction.h"
#include "nsITransactionManager.h"
#include "nsITransactionListener.h"

class nsHTMLEditorLog;

/** implementation of a transaction listener object.
 *
 */
class nsEditorTxnLog : public nsITransactionListener
{
private:

  nsHTMLEditorLog *mEditorLog;
  PRInt32 mIndentLevel;
  PRInt32 mBatchCount;

public:

  /** The default constructor.
   */
  nsEditorTxnLog(nsHTMLEditorLog *aEditorLog=0);

  /** The default destructor.
   */
  virtual ~nsEditorTxnLog();

  /* Macro for AddRef(), Release(), and QueryInterface() */
  NS_DECL_ISUPPORTS

  /* nsITransactionListener method implementations. */
  NS_IMETHOD WillDo(nsITransactionManager *aTxMgr, nsITransaction *aTransaction, PRBool *aInterrupt);
  NS_IMETHOD DidDo(nsITransactionManager *aTxMgr, nsITransaction *aTransaction, nsresult aDoResult);
  NS_IMETHOD WillUndo(nsITransactionManager *aTxMgr, nsITransaction *aTransaction, PRBool *aInterrupt);
  NS_IMETHOD DidUndo(nsITransactionManager *aTxMgr, nsITransaction *aTransaction, nsresult aUndoResult);
  NS_IMETHOD WillRedo(nsITransactionManager *aTxMgr, nsITransaction *aTransaction, PRBool *aInterrupt);
  NS_IMETHOD DidRedo(nsITransactionManager *aTxMgr, nsITransaction *aTransaction, nsresult aRedoResult);
  NS_IMETHOD WillBeginBatch(nsITransactionManager *aTxMgr, PRBool *aInterrupt);
  NS_IMETHOD DidBeginBatch(nsITransactionManager *aTxMgr, nsresult aResult);
  NS_IMETHOD WillEndBatch(nsITransactionManager *aTxMgr, PRBool *aInterrupt);
  NS_IMETHOD DidEndBatch(nsITransactionManager *aTxMgr, nsresult aResult);
  NS_IMETHOD WillMerge(nsITransactionManager *aTxMgr, nsITransaction *aTopTransaction, nsITransaction *aTransaction, PRBool *aInterrupt);
  NS_IMETHOD DidMerge(nsITransactionManager *aTxMgr, nsITransaction *aTopTransaction, nsITransaction *aTransaction, PRBool aDidMerge, nsresult aMergeResult);


private:

  /* nsEditorTxnLog private methods. */
  const char *GetString(nsITransaction *aTransaction, char *aBuffer, PRInt32 aBufLength);
  nsresult PrintIndent(PRInt32 aIndentLevel);
  nsresult Write(const char *aBuffer);
  nsresult WriteInt(const char *aFormat, PRInt32 aInt);
  nsresult Flush();
};

#endif // nsEditorTxnLog_h__
