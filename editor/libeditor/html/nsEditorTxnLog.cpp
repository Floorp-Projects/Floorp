/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
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

#include <stdio.h>
#include "nsHTMLEditorLog.h"
#include "nsEditorTxnLog.h"

#define LOCK_LOG(doc)
#define UNLOCK_LOG(doc)

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

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
    NS_DELETEXPCOM(this);
    return 0;
  }
  return mRefCnt;
}

#else

NS_IMPL_ADDREF(nsEditorTxnLog)
NS_IMPL_RELEASE(nsEditorTxnLog)

#endif

NS_IMETHODIMP
nsEditorTxnLog::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (nsnull == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtr = (void*)(nsISupports*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(nsITransactionListener::GetIID())) {
    *aInstancePtr = (void*)(nsITransactionListener*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  *aInstancePtr = 0;
  return NS_NOINTERFACE;
}

NS_IMETHODIMP
nsEditorTxnLog::WillDo(nsITransactionManager *aTxMgr, nsITransaction *aTransaction)
{
  LOCK_LOG(this);

  PrintIndent(mIndentLevel++);
  Write("WillDo:   ");
  Write(GetString(aTransaction));
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
  Write(GetString(aTransaction));
  Write("(");
  WriteInt("%d", aDoResult);
  Write(")\n");
  Flush();

  UNLOCK_LOG(this);

  return NS_OK;
}

NS_IMETHODIMP
nsEditorTxnLog::WillUndo(nsITransactionManager *aTxMgr, nsITransaction *aTransaction)
{
  LOCK_LOG(this);

  PrintIndent(mIndentLevel++);

  if (aTransaction)
  {
    Write("WillUndo:   ");
    Write(GetString(aTransaction));
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
    Write(GetString(aTransaction));
    Write("(");
    WriteInt("%d", aUndoResult);
    Write(")\n");
  }
  else
  {
    Write("EndUndoBatch (");
    WriteInt("%d", aUndoResult);
    Write(")\n");
  }

  Flush();

  UNLOCK_LOG(this);

  return NS_OK;
}

NS_IMETHODIMP
nsEditorTxnLog::WillRedo(nsITransactionManager *aTxMgr, nsITransaction *aTransaction)
{
  LOCK_LOG(this);

  PrintIndent(mIndentLevel++);

  if (aTransaction)
  {
    Write("WillRedo: ");
    Write(GetString(aTransaction));
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
    Write(GetString(aTransaction));
    Write(" (");
    WriteInt("%d", aRedoResult);
    Write(")\n");
  }
  else
  {
    Write("DidRedoBatch (");
    WriteInt("%d", aRedoResult);
    Write(")\n");
  }

  Flush();

  UNLOCK_LOG(this);

  return NS_OK;
}

NS_IMETHODIMP
nsEditorTxnLog::WillBeginBatch(nsITransactionManager *aTxMgr)
{
  LOCK_LOG(this);

  PrintIndent(mIndentLevel);
  Write("WillBeginBatch: ");
  WriteInt("%d", mBatchCount);
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
  WriteInt("%d", mBatchCount++);
  Write(" (");
  WriteInt("%d", aResult);
  Write(")\n");
  Flush();

  UNLOCK_LOG(this);

  return NS_OK;
}

NS_IMETHODIMP
nsEditorTxnLog::WillEndBatch(nsITransactionManager *aTxMgr)
{
  LOCK_LOG(this);

  PrintIndent(--mIndentLevel);
  Write("WillEndBatch:   ");
  WriteInt("%d", --mBatchCount);
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
  WriteInt("%d", mBatchCount);
  Write(" (");
  WriteInt("%d", aResult);
  Write(")\n");
  Flush();

  UNLOCK_LOG(this);

  return NS_OK;
}

NS_IMETHODIMP
nsEditorTxnLog::WillMerge(nsITransactionManager *aTxMgr, nsITransaction *aTopTransaction, nsITransaction *aTransaction)
{
  LOCK_LOG(this);

  PrintIndent(mIndentLevel);
  Write("WillMerge:   ");
  Write(GetString(aTopTransaction));
  Write(" <-- ");
  Write(GetString(aTransaction));
  Write("\n");
  Flush();

  UNLOCK_LOG(this);

  return NS_OK;
}

NS_IMETHODIMP
nsEditorTxnLog::DidMerge(nsITransactionManager *aTxMgr, nsITransaction *aTopTransaction, nsITransaction *aTransaction, PRBool aDidMerge, nsresult aMergeResult)
{
  LOCK_LOG(this);

  PrintIndent(mIndentLevel);
  Write("DidMerge:    ");
  Write(GetString(aTopTransaction));
  Write(" <-- ");
  Write(GetString(aTransaction));
  Write(" (");
  Write(aDidMerge ? "TRUE" : "FALSE");
  Write(", ");
  WriteInt("%d", aMergeResult);
  Write(")\n");
  Flush();

  UNLOCK_LOG(this);

  return NS_OK;
}

const char *
nsEditorTxnLog::GetString(nsITransaction *aTransaction)
{
  static char buf[256];

  nsString str = "";

  aTransaction->GetRedoString(&str);

  if (str.Length() == 0)
    str = "<NULL>";

  buf[0] = '\0';
  str.ToCString(buf, 256);

  return buf;
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
  if (!aBuffer)
    return NS_ERROR_NULL_POINTER;

  if (mEditorLog)
    mEditorLog->Write(aBuffer);
  else
    printf(aBuffer);

  return NS_OK;
}

nsresult
nsEditorTxnLog::WriteInt(const char *aFormat, PRInt32 aInt)
{
  if (!aFormat)
    return NS_ERROR_NULL_POINTER;

  if (mEditorLog)
    mEditorLog->WriteInt(aFormat, aInt);
  else
    printf(aFormat, aInt);

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
