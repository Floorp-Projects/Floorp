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
#include "nsJSEditorLog.h"

#define LOCK_LOG(doc)
#define UNLOCK_LOG(doc)

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

nsJSEditorLog::nsJSEditorLog()
{
  mRefCnt      = 0;
  mIndentLevel = 0;
  mBatchCount  = 0;
}

nsJSEditorLog::~nsJSEditorLog()
{
}

#define DEBUG_JS_EDITOR_LOG_REFCNT 1

#ifdef DEBUG_JS_EDITOR_LOG_REFCNT

nsrefcnt nsJSEditorLog::AddRef(void)
{
  return ++mRefCnt;
}

nsrefcnt nsJSEditorLog::Release(void)
{
  NS_PRECONDITION(0 != mRefCnt, "dup release");
  if (--mRefCnt == 0) {
    NS_DELETEXPCOM(this);
    return 0;
  }
  return mRefCnt;
}

#else

NS_IMPL_ADDREF(nsJSEditorLog)
NS_IMPL_RELEASE(nsJSEditorLog)

#endif

NS_IMETHODIMP
nsJSEditorLog::QueryInterface(REFNSIID aIID, void** aInstancePtr)
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
nsJSEditorLog::WillDo(nsITransactionManager *aTxMgr, nsITransaction *aTransaction)
{
  LOCK_LOG(this);

  PrintIndent(mIndentLevel++);
  printf("WillDo:   %s\n", GetString(aTransaction));

  UNLOCK_LOG(this);

  return NS_OK;
}

NS_IMETHODIMP
nsJSEditorLog::DidDo(nsITransactionManager *aTxMgr, nsITransaction *aTransaction, nsresult aDoResult)
{
  LOCK_LOG(this);

  PrintIndent(--mIndentLevel);
  printf("DidDo:    %s (%d)\n", GetString(aTransaction), aDoResult);

  UNLOCK_LOG(this);

  return NS_OK;
}

NS_IMETHODIMP
nsJSEditorLog::WillUndo(nsITransactionManager *aTxMgr, nsITransaction *aTransaction)
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
nsJSEditorLog::DidUndo(nsITransactionManager *aTxMgr, nsITransaction *aTransaction, nsresult aUndoResult)
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
nsJSEditorLog::WillRedo(nsITransactionManager *aTxMgr, nsITransaction *aTransaction)
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
nsJSEditorLog::DidRedo(nsITransactionManager *aTxMgr, nsITransaction *aTransaction, nsresult aRedoResult)
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
nsJSEditorLog::WillBeginBatch(nsITransactionManager *aTxMgr)
{
  LOCK_LOG(this);

  PrintIndent(mIndentLevel);
  printf("WillBeginBatch: %d\n", mBatchCount);

  UNLOCK_LOG(this);

  return NS_OK;
}

NS_IMETHODIMP
nsJSEditorLog::DidBeginBatch(nsITransactionManager *aTxMgr, nsresult aResult)
{
  LOCK_LOG(this);

  PrintIndent(mIndentLevel++);
  printf("DidBeginBatch:  %d (%d)\n", mBatchCount++, aResult);

  UNLOCK_LOG(this);

  return NS_OK;
}

NS_IMETHODIMP
nsJSEditorLog::WillEndBatch(nsITransactionManager *aTxMgr)
{
  LOCK_LOG(this);

  PrintIndent(--mIndentLevel);
  printf("WillEndBatch:   %d\n", --mBatchCount);

  UNLOCK_LOG(this);

  return NS_OK;
}

NS_IMETHODIMP
nsJSEditorLog::DidEndBatch(nsITransactionManager *aTxMgr, nsresult aResult)
{
  LOCK_LOG(this);

  PrintIndent(mIndentLevel);
  printf("DidEndBatch:    %d (%d)\n", mBatchCount, aResult);

  UNLOCK_LOG(this);

  return NS_OK;
}

NS_IMETHODIMP
nsJSEditorLog::WillMerge(nsITransactionManager *aTxMgr, nsITransaction *aTopTransaction, nsITransaction *aTransaction)
{
  LOCK_LOG(this);

  PrintIndent(mIndentLevel);
  printf("WillMerge:   %s <-- %s\n",
         GetString(aTopTransaction), GetString(aTransaction));

  UNLOCK_LOG(this);

  return NS_OK;
}

NS_IMETHODIMP
nsJSEditorLog::DidMerge(nsITransactionManager *aTxMgr, nsITransaction *aTopTransaction, nsITransaction *aTransaction, PRBool aDidMerge, nsresult aMergeResult)
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
nsJSEditorLog::GetString(nsITransaction *aTransaction)
{
  static char buf[256];

  nsString defaultStr = "<NULL>";
  nsString *str = &defaultStr;

  aTransaction->GetRedoString(&str);

  if (!str)
    str = &defaultStr;

  buf[0] = '\0';
  str->ToCString(buf, 256);

  return buf;
}

nsresult
nsJSEditorLog::PrintIndent(PRInt32 aIndentLevel)
{
  PRInt32 i;

  for (i = 0; i < aIndentLevel; i++)
    printf("  ");

  return NS_OK;
}
