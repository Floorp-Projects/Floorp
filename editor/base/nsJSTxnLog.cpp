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
#include "nsJSTxnLog.h"

#define LOCK_LOG(doc)
#define UNLOCK_LOG(doc)

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

nsJSTxnLog::nsJSTxnLog()
{
  mRefCnt      = 0;
  mIndentLevel = 0;
  mBatchCount  = 0;
}

nsJSTxnLog::~nsJSTxnLog()
{
}

#define DEBUG_JS_TXN_LOG_REFCNT 1

#ifdef DEBUG_JS_TXN_LOG_REFCNT

nsrefcnt nsJSTxnLog::AddRef(void)
{
  return ++mRefCnt;
}

nsrefcnt nsJSTxnLog::Release(void)
{
  NS_PRECONDITION(0 != mRefCnt, "dup release");
  if (--mRefCnt == 0) {
    NS_DELETEXPCOM(this);
    return 0;
  }
  return mRefCnt;
}

#else

NS_IMPL_ADDREF(nsJSTxnLog)
NS_IMPL_RELEASE(nsJSTxnLog)

#endif

NS_IMETHODIMP
nsJSTxnLog::QueryInterface(REFNSIID aIID, void** aInstancePtr)
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
nsJSTxnLog::WillDo(nsITransactionManager *aTxMgr, nsITransaction *aTransaction)
{
  LOCK_LOG(this);

  PrintIndent(mIndentLevel++);
  printf("WillDo:   %s\n", GetString(aTransaction));

  UNLOCK_LOG(this);

  return NS_OK;
}

NS_IMETHODIMP
nsJSTxnLog::DidDo(nsITransactionManager *aTxMgr, nsITransaction *aTransaction, nsresult aDoResult)
{
  LOCK_LOG(this);

  PrintIndent(--mIndentLevel);
  printf("DidDo:    %s (%d)\n", GetString(aTransaction), aDoResult);

  UNLOCK_LOG(this);

  return NS_OK;
}

NS_IMETHODIMP
nsJSTxnLog::WillUndo(nsITransactionManager *aTxMgr, nsITransaction *aTransaction)
{
  LOCK_LOG(this);

  PrintIndent(mIndentLevel++);

  if (aTransaction)
    printf("WillUndo: %s\n", GetString(aTransaction));
  else
    printf("WillUndoBatch\n");

  UNLOCK_LOG(this);

  return NS_OK;
}

NS_IMETHODIMP
nsJSTxnLog::DidUndo(nsITransactionManager *aTxMgr, nsITransaction *aTransaction, nsresult aUndoResult)
{
  LOCK_LOG(this);

  PrintIndent(--mIndentLevel);

  if (aTransaction)
    printf("DidUndo:  %s (%d)\n", GetString(aTransaction), aUndoResult);
  else
    printf("EndUndoBatch (%d)\n", aUndoResult);

  UNLOCK_LOG(this);

  return NS_OK;
}

NS_IMETHODIMP
nsJSTxnLog::WillRedo(nsITransactionManager *aTxMgr, nsITransaction *aTransaction)
{
  LOCK_LOG(this);

  PrintIndent(mIndentLevel++);

  if (aTransaction)
    printf("WillRedo: %s\n", GetString(aTransaction));
  else
    printf("WillRedoBatch\n");

  UNLOCK_LOG(this);

  return NS_OK;
}

NS_IMETHODIMP
nsJSTxnLog::DidRedo(nsITransactionManager *aTxMgr, nsITransaction *aTransaction, nsresult aRedoResult)
{
  LOCK_LOG(this);

  PrintIndent(--mIndentLevel);

  if (aTransaction)
    printf("DidRedo:  %s (%d)\n", GetString(aTransaction), aRedoResult);
  else
    printf("DidRedoBatch (%d)\n", aRedoResult);

  UNLOCK_LOG(this);

  return NS_OK;
}

NS_IMETHODIMP
nsJSTxnLog::WillBeginBatch(nsITransactionManager *aTxMgr)
{
  LOCK_LOG(this);

  PrintIndent(mIndentLevel);
  printf("WillBeginBatch: %d\n", mBatchCount);

  UNLOCK_LOG(this);

  return NS_OK;
}

NS_IMETHODIMP
nsJSTxnLog::DidBeginBatch(nsITransactionManager *aTxMgr, nsresult aResult)
{
  LOCK_LOG(this);

  PrintIndent(mIndentLevel++);
  printf("DidBeginBatch:  %d (%d)\n", mBatchCount++, aResult);

  UNLOCK_LOG(this);

  return NS_OK;
}

NS_IMETHODIMP
nsJSTxnLog::WillEndBatch(nsITransactionManager *aTxMgr)
{
  LOCK_LOG(this);

  PrintIndent(--mIndentLevel);
  printf("WillEndBatch:   %d\n", --mBatchCount);

  UNLOCK_LOG(this);

  return NS_OK;
}

NS_IMETHODIMP
nsJSTxnLog::DidEndBatch(nsITransactionManager *aTxMgr, nsresult aResult)
{
  LOCK_LOG(this);

  PrintIndent(mIndentLevel);
  printf("DidEndBatch:    %d (%d)\n", mBatchCount, aResult);

  UNLOCK_LOG(this);

  return NS_OK;
}

NS_IMETHODIMP
nsJSTxnLog::WillMerge(nsITransactionManager *aTxMgr, nsITransaction *aTopTransaction, nsITransaction *aTransaction)
{
  LOCK_LOG(this);

  PrintIndent(mIndentLevel);
  printf("WillMerge:   %s <-- %s\n",
         GetString(aTopTransaction), GetString(aTransaction));

  UNLOCK_LOG(this);

  return NS_OK;
}

NS_IMETHODIMP
nsJSTxnLog::DidMerge(nsITransactionManager *aTxMgr, nsITransaction *aTopTransaction, nsITransaction *aTransaction, PRBool aDidMerge, nsresult aMergeResult)
{
  LOCK_LOG(this);

  PrintIndent(mIndentLevel);
  printf("DidMerge:    %s <-- %s (%s, %d)\n",
         GetString(aTopTransaction), GetString(aTransaction),
         aDidMerge ? "TRUE" : "FALSE", aMergeResult);

  UNLOCK_LOG(this);

  return NS_OK;
}

const char *
nsJSTxnLog::GetString(nsITransaction *aTransaction)
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
nsJSTxnLog::PrintIndent(PRInt32 aIndentLevel)
{
  PRInt32 i;

  printf("    // ");

  for (i = 0; i < aIndentLevel; i++)
    printf("  ");

  return NS_OK;
}
