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
 *   Pierre Phaneuf <pp@ludusdesign.com>
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

#include <stdio.h>
#include "nsHTMLEditorLog.h"
#include "nsEditorTxnLog.h"
#include "nsPIEditorTransaction.h"

#define LOCK_LOG(doc)
#define UNLOCK_LOG(doc)


nsEditorTxnLog::nsEditorTxnLog(nsHTMLEditorLog *aEditorLog)
{
  mRefCnt      = 0;
  mIndentLevel = 0;
  mBatchCount  = 0;
  mEditorLog   = aEditorLog;
}

nsEditorTxnLog::~nsEditorTxnLog()
{
}

// #define DEBUG_EDITOR_TXN_LOG_REFCNT 1

#ifdef DEBUG_EDITOR_TXN_LOG_REFCNT

nsrefcnt nsEditorTxnLog::AddRef(void)
{
  return ++mRefCnt;
}

nsrefcnt nsEditorTxnLog::Release(void)
{
  NS_PRECONDITION(0 != mRefCnt, "dup release");
  if (--mRefCnt == 0) {
    delete this;
    return 0;
  }
  return mRefCnt;
}

#else

NS_IMPL_ADDREF(nsEditorTxnLog)
NS_IMPL_RELEASE(nsEditorTxnLog)

#endif

NS_IMPL_QUERY_INTERFACE1(nsEditorTxnLog, nsITransactionListener)

NS_IMETHODIMP
nsEditorTxnLog::WillDo(nsITransactionManager *aTxMgr, nsITransaction *aTransaction, bool *aInterrupt)
{
  LOCK_LOG(this);

  PrintIndent(mIndentLevel++);
  Write("WillDo:   ");
  WriteTransaction(aTransaction);
  Write("\n");
  Flush();

  UNLOCK_LOG(this);

  return NS_OK;
}

NS_IMETHODIMP
nsEditorTxnLog::DidDo(nsITransactionManager *aTxMgr, nsITransaction *aTransaction, nsresult aDoResult)
{
  LOCK_LOG(this);

  PrintIndent(--mIndentLevel);
  Write("DidDo:    ");
  WriteTransaction(aTransaction);
  Write("(");
  WriteInt(aDoResult);
  Write(")\n");
  Flush();

  UNLOCK_LOG(this);

  return NS_OK;
}

NS_IMETHODIMP
nsEditorTxnLog::WillUndo(nsITransactionManager *aTxMgr, nsITransaction *aTransaction, bool *aInterrupt)
{
  LOCK_LOG(this);

  PrintIndent(mIndentLevel++);

  if (aTransaction)
  {
    Write("WillUndo:   ");
    WriteTransaction(aTransaction);
    Write("\n");
  }
  else
    Write("WillUndoBatch\n");

  Flush();

  UNLOCK_LOG(this);

  return NS_OK;
}

NS_IMETHODIMP
nsEditorTxnLog::DidUndo(nsITransactionManager *aTxMgr, nsITransaction *aTransaction, nsresult aUndoResult)
{
  LOCK_LOG(this);

  PrintIndent(--mIndentLevel);

  if (aTransaction)
  {
    Write("DidUndo:  ");
    WriteTransaction(aTransaction);
    Write("(");
    WriteInt(aUndoResult);
    Write(")\n");
  }
  else
  {
    Write("EndUndoBatch (");
    WriteInt(aUndoResult);
    Write(")\n");
  }

  Flush();

  UNLOCK_LOG(this);

  return NS_OK;
}

NS_IMETHODIMP
nsEditorTxnLog::WillRedo(nsITransactionManager *aTxMgr, nsITransaction *aTransaction, bool *aInterrupt)
{
  LOCK_LOG(this);

  PrintIndent(mIndentLevel++);

  if (aTransaction)
  {
    Write("WillRedo: ");
    WriteTransaction(aTransaction);
    Write("\n");
  }
  else
    Write("WillRedoBatch\n");

  Flush();

  UNLOCK_LOG(this);

  return NS_OK;
}

NS_IMETHODIMP
nsEditorTxnLog::DidRedo(nsITransactionManager *aTxMgr, nsITransaction *aTransaction, nsresult aRedoResult)
{
  LOCK_LOG(this);

  PrintIndent(--mIndentLevel);

  if (aTransaction)
  {
    Write("DidRedo:  ");
    WriteTransaction(aTransaction);
    Write(" (");
    WriteInt(aRedoResult);
    Write(")\n");
  }
  else
  {
    Write("DidRedoBatch (");
    WriteInt(aRedoResult);
    Write(")\n");
  }

  Flush();

  UNLOCK_LOG(this);

  return NS_OK;
}

NS_IMETHODIMP
nsEditorTxnLog::WillBeginBatch(nsITransactionManager *aTxMgr, bool *aInterrupt)
{
  LOCK_LOG(this);

  PrintIndent(mIndentLevel);
  Write("WillBeginBatch: ");
  WriteInt(mBatchCount);
  Write("\n");
  Flush();

  UNLOCK_LOG(this);

  return NS_OK;
}

NS_IMETHODIMP
nsEditorTxnLog::DidBeginBatch(nsITransactionManager *aTxMgr, nsresult aResult)
{
  LOCK_LOG(this);

  PrintIndent(mIndentLevel++);
  Write("DidBeginBatch:  ");
  WriteInt(mBatchCount++);
  Write(" (");
  WriteInt(aResult);
  Write(")\n");
  Flush();

  UNLOCK_LOG(this);

  return NS_OK;
}

NS_IMETHODIMP
nsEditorTxnLog::WillEndBatch(nsITransactionManager *aTxMgr, bool *aInterrupt)
{
  LOCK_LOG(this);

  PrintIndent(--mIndentLevel);
  Write("WillEndBatch:   ");
  WriteInt(--mBatchCount);
  Write("\n");
  Flush();

  UNLOCK_LOG(this);

  return NS_OK;
}

NS_IMETHODIMP
nsEditorTxnLog::DidEndBatch(nsITransactionManager *aTxMgr, nsresult aResult)
{
  LOCK_LOG(this);

  PrintIndent(mIndentLevel);
  Write("DidEndBatch:    ");
  WriteInt(mBatchCount);
  Write(" (");
  WriteInt(aResult);
  Write(")\n");
  Flush();

  UNLOCK_LOG(this);

  return NS_OK;
}

NS_IMETHODIMP
nsEditorTxnLog::WillMerge(nsITransactionManager *aTxMgr, nsITransaction *aTopTransaction, nsITransaction *aTransaction, bool *aInterrupt)
{
  LOCK_LOG(this);

  PrintIndent(mIndentLevel);
  Write("WillMerge:   ");
  WriteTransaction(aTopTransaction);
  Write(" <-- ");
  WriteTransaction(aTransaction);
  Write("\n");
  Flush();

  UNLOCK_LOG(this);

  return NS_OK;
}

NS_IMETHODIMP
nsEditorTxnLog::DidMerge(nsITransactionManager *aTxMgr, nsITransaction *aTopTransaction, nsITransaction *aTransaction, bool aDidMerge, nsresult aMergeResult)
{
  LOCK_LOG(this);

  PrintIndent(mIndentLevel);
  Write("DidMerge:    ");
  WriteTransaction(aTopTransaction);
  Write(" <-- ");
  WriteTransaction(aTransaction);
  Write(" (");
  Write(aDidMerge ? "TRUE" : "FALSE");
  Write(", ");
  WriteInt(aMergeResult);
  Write(")\n");
  Flush();

  UNLOCK_LOG(this);

  return NS_OK;
}

nsresult
nsEditorTxnLog::WriteTransaction(nsITransaction *aTransaction)
{
  nsString str;

  nsCOMPtr<nsPIEditorTransaction> txn = do_QueryInterface(aTransaction);
  if (txn) {
    txn->GetTxnDescription(str);
    if (str.IsEmpty())
      str.AssignLiteral("<NULL>");
  }

  return Write(NS_LossyConvertUTF16toASCII(str).get());
}

nsresult
nsEditorTxnLog::PrintIndent(PRInt32 aIndentLevel)
{
  PRInt32 i;

  Write("    // ");

  for (i = 0; i < aIndentLevel; i++)
    Write("  ");

  return NS_OK;
}

nsresult
nsEditorTxnLog::Write(const char *aBuffer)
{
  NS_ENSURE_TRUE(aBuffer, NS_ERROR_NULL_POINTER);

  if (mEditorLog)
    mEditorLog->Write(aBuffer);
  else
  {
    PRInt32 len = strlen(aBuffer);
    if (len > 0)
      fwrite(aBuffer, 1, len, stdout);
  }

  return NS_OK;
}

nsresult
nsEditorTxnLog::WriteInt(PRInt32 aInt)
{
  if (mEditorLog)
    mEditorLog->WriteInt(aInt);
  else
    printf("%d", aInt);

  return NS_OK;
}

nsresult
nsEditorTxnLog::Flush()
{
  nsresult result = NS_OK;

#ifdef SLOWS_THINGS_WAY_DOWN

  if (mEditorLog)
    result = mEditorLog->Flush();
  else
    fflush(stdout);

#endif // SLOWS_THINGS_WAY_DOWN

  return result;
}
