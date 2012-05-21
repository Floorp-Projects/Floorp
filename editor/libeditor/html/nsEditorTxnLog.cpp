/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
