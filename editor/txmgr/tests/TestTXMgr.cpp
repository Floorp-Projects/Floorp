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

#include "nsTransactionManager.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kITransactionIID, NS_ITRANSACTION_IID);
static NS_DEFINE_IID(kITransactionManagerIID, NS_ITRANSACTIONMANAGER_IID);
static NS_DEFINE_IID(kIOutputStreamIID, NS_IOUTPUTSTREAM_IID);

class ConsoleOutput : public nsIOutputStream
{
public:
  ConsoleOutput()  {}
  virtual ~ConsoleOutput() {}

  NS_DECL_ISUPPORTS

  nsresult Close(void) {}
  nsresult Write(const char *str, PRInt32 offset, PRInt32 len, PRInt32 *wcnt)
  {
    *wcnt = fwrite(&str[offset], 1, len, stdout);
    fflush(stdout);
    return NS_OK;
  }
};

NS_IMPL_ADDREF(ConsoleOutput)
NS_IMPL_RELEASE(ConsoleOutput)

nsresult
ConsoleOutput::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (nsnull == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtr = (void*)(nsISupports*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kIOutputStreamIID)) {
    *aInstancePtr = (void*)(nsITransaction*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  *aInstancePtr = 0;
  return NS_NOINTERFACE;
}

ConsoleOutput console;

class TestTransaction : public nsITransaction
{
public:

  TestTransaction() : mRefCnt(0) {}
  virtual ~TestTransaction()     {}

  NS_DECL_ISUPPORTS
};

#if 1

nsrefcnt TestTransaction::AddRef()
{
  return ++mRefCnt;
}

nsrefcnt TestTransaction::Release()
{
  NS_PRECONDITION(0 != mRefCnt, "dup release");
  if (--mRefCnt == 0) {
    NS_DELETEXPCOM(this);
    return 0;
  }
  return mRefCnt;
}

#else

NS_IMPL_ADDREF(TestTransaction)
NS_IMPL_RELEASE(TestTransaction)

#endif

nsresult
TestTransaction::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtr = (void*)(nsISupports*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kITransactionIID)) {
    *aInstancePtr = (void*)(nsITransaction*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  *aInstancePtr = 0;
  return NS_NOINTERFACE;
}

class SimpleTransaction : public TestTransaction
{
public:
  static PRInt32 sConstructorCount;
  static PRInt32 sDestructorCount;
  PRInt32 mVal;

  SimpleTransaction() : mVal(++sConstructorCount) {}

  virtual ~SimpleTransaction()
  {
    ++sDestructorCount;

    printf("\n~SimpleTransaction: %d - 0x%.8x\n", mVal, this);
    mVal = -1;
  }

  virtual nsresult Do()
  {
    return NS_OK;
  }

  virtual nsresult Undo()
  {
    return NS_OK;
  }

  virtual nsresult Redo()
  {
    return NS_OK;
  }

  virtual nsresult Merge(PRBool *aDidMerge, nsITransaction *aTransaction)
  {
    if (aDidMerge)
      *aDidMerge = 0;

    return NS_OK;
  }

  virtual nsresult Write(nsIOutputStream *aOutputStream)
  {
    char buf[256];
    PRInt32 amt;

    sprintf(buf, "Transaction: %d - 0x%.8x\n", mVal, this);
    return aOutputStream->Write(buf, 0, strlen(buf), &amt);
  }

  virtual nsresult GetUndoString(nsString **aString)
  {
    *aString = 0;
    return NS_OK;
  }

  virtual nsresult GetRedoString(nsString **aString)
  {
    *aString = 0;
    return NS_OK;
  }

};

PRInt32 SimpleTransaction::sConstructorCount = 0;
PRInt32 SimpleTransaction::sDestructorCount  = 0;

class DoErrorTransaction : public SimpleTransaction
{
public:

  virtual nsresult Do()
  {
    return NS_ERROR_FAILURE;
  }
};

class UndoErrorTransaction : public SimpleTransaction
{
public:

  virtual nsresult Undo()
  {
    return NS_ERROR_FAILURE;
  }
};

class RedoErrorTransaction : public SimpleTransaction
{
public:

  virtual nsresult Redo()
  {
    return NS_ERROR_FAILURE;
  }
};

nsresult
simple_test ()
{
  /*******************************************************************
   *
   * Create a transaction manager implementation:
   *
   *******************************************************************/

  printf("Create a transaction manager with 10 levels of undo ... ");

  nsTransactionManager  *mgrimpl = new nsTransactionManager(10);
  nsITransactionManager  *mgr    = 0;
  nsITransaction *tx             = 0;
  SimpleTransaction *tximpl      = 0;

  if (!mgrimpl) {
    printf("ERROR: Failed to create nsTransactionManager.\n");
    return NS_ERROR_OUT_OF_MEMORY;
  }

  printf("passed\n");

  /*******************************************************************
   *
   * Test QueryInterface():
   *
   *******************************************************************/

  printf("Get the nsITransactionManager interface ... ");

  nsresult result = mgrimpl->QueryInterface(kITransactionManagerIID,
                                            (void **)&mgr);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: Failed to get TransactionManager interface. (%d)\n", result);
    return result;
  }

  if (!mgr) {
    printf("ERROR: QueryInterface() returned NULL pointer.\n");
    return NS_ERROR_NULL_POINTER;
  }

  printf("passed\n");

  /*******************************************************************
   *
   * Call Do() with a null transaction:
   *
   *******************************************************************/

  printf("Call Do() with null transaction ... ");
  result = mgr->Do(0);

  if (!NS_SUCCEEDED(result)
      && result != NS_ERROR_NULL_POINTER) {
    printf("ERROR: Do() returned unexpected error. (%d)\n", result);
    return result;
  }

  printf("passed\n");

  /*******************************************************************
   *
   * Call Undo() with an empty undo stack:
   *
   *******************************************************************/

  printf("Call Undo() with empty undo stack ... ");
  result = mgr->Undo();

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: Undo on empty undo stack failed. (%d)\n", result);
    return result;
  }

  printf("passed\n");

  /*******************************************************************
   *
   * Call Redo() with an empty redo stack:
   *
   *******************************************************************/

  printf("Call Redo() with empty redo stack ... ");
  result = mgr->Redo();

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: Redo on empty redo stack failed. (%d)\n", result);
    return result;
  }

  printf("passed\n");

  /*******************************************************************
   *
   * Call Clear() with empty undo and redo stacks:
   *
   *******************************************************************/

  printf("Call Clear() with empty undo and redo stack ... ");
  result = mgr->Clear();

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: Clear on empty undo and redo stack failed. (%d)\n", result);
    return result;
  }

  printf("passed\n");

  /*******************************************************************
   *
   * Call GetNumberOfUndoItems() with an empty undo stack:
   *
   *******************************************************************/

  PRInt32 numitems = 0;

  printf("Call GetNumberOfUndoItems() with empty undo stack ... ");
  result = mgr->GetNumberOfUndoItems(&numitems);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: GetNumberOfUndoItems() on empty undo stack failed. (%d)\n",
           result);
    return result;
  }

  if (numitems != 0) {
    printf("ERROR: GetNumberOfUndoItems() expected 0 got %d. (%d)\n",
           numitems, result);
    return result;
  }

  printf("passed\n");

  /*******************************************************************
   *
   * Call GetNumberOfRedoItems() with an empty redo stack:
   *
   *******************************************************************/

  printf("Call GetNumberOfRedoItems() with empty redo stack ... ");
  result = mgr->GetNumberOfRedoItems(&numitems);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: GetNumberOfRedoItems() on empty redo stack failed. (%d)\n",
           result);
    return result;
  }

  if (numitems != 0) {
    printf("ERROR: GetNumberOfRedoItems() expected 0 got %d. (%d)\n",
           numitems, result);
    return result;
  }

  printf("passed\n");

  /*******************************************************************
   *
   * Call PeekUndoStack() with an empty undo stack:
   *
   *******************************************************************/

  printf("Call PeekUndoStack() with empty undo stack ... ");

  tx = 0;
  result = mgr->PeekUndoStack(&tx);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: PeekUndoStack() on empty undo stack failed. (%d)\n", result);
    return result;
  }

  if (tx != 0) {
    printf("ERROR: PeekUndoStack() on empty undo stack failed. (%d)\n", result);
    return result;
  }

  printf("passed\n");

  /*******************************************************************
   *
   * Call PeekRedoStack() with an empty undo stack:
   *
   *******************************************************************/

  printf("Call PeekRedoStack() with empty undo stack ... ");

  tx = 0;
  result = mgr->PeekRedoStack(&tx);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: PeekRedoStack() on empty redo stack failed. (%d)\n", result);
    return result;
  }

  if (tx != 0) {
    printf("ERROR: PeekRedoStack() on empty redo stack failed. (%d)\n", result);
    return result;
  }

  printf("passed\n");

  /*******************************************************************
   *
   * Call Write() with a null output stream:
   *
   *******************************************************************/

  printf("Call Write() with null output stream ... ");

  result = mgr->Write(0);

  if (!NS_SUCCEEDED(result)
      && result != NS_ERROR_NULL_POINTER) {
    printf("ERROR: Write() returned unexpected error. (%d)\n", result);
    return result;
  }

  printf("passed\n");

  /*******************************************************************
   *
   * Call AddListener() with a null listener pointer:
   *
   *******************************************************************/

  printf("Call AddListener() with null listener ... ");

  result = mgr->AddListener(0);

  if (!NS_SUCCEEDED(result)
      && result != NS_ERROR_NOT_IMPLEMENTED) {
    printf("ERROR: AddListener() returned unexpected error. (%d)\n", result);
    return result;
  }

  printf("passed\n");

  /*******************************************************************
   *
   * Call RemoveListener() with a null listener pointer:
   *
   *******************************************************************/

  printf("Call RemoveListener() with null listener ... ");

  result = mgr->RemoveListener(0);

  if (!NS_SUCCEEDED(result)
      && result != NS_ERROR_NOT_IMPLEMENTED) {
    printf("ERROR: RemoveListener() returned unexpected error. (%d)\n", result);
    return result;
  }

  printf("passed\n");

  /*******************************************************************
   *
   * Execute 20 transactions. Afterwards, we should have 10
   * transactions on the undo stack:
   *
   *******************************************************************/

  int i = 0;

  printf("Execute 20 simple transactions ... ");

  for (i = 1; i <= 20; i++) {
    tximpl = new SimpleTransaction();

    tx = 0;
    result = tximpl->QueryInterface(kITransactionIID, (void **)&tx);
    if (!NS_SUCCEEDED(result)) {
      printf("ERROR: QueryInterface() failed for transaction %d. (%d)\n",
             i, result);
      return result;
    }

    result = mgr->Do(tx);
    if (!NS_SUCCEEDED(result)) {
      printf("ERROR: Failed to execute transaction %d. (%d)\n", i, result);
      return result;
    }

    tx->Release();
  }

  result = mgr->GetNumberOfUndoItems(&numitems);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: GetNumberOfUndoItems() on undo stack with 10 items failed. (%d)\n",
           result);
    return result;
  }

  if (numitems != 10) {
    printf("ERROR: GetNumberOfUndoItems() expected 10 got %d. (%d)\n",
           numitems, result);
    return result;
  }

  result = mgr->GetNumberOfRedoItems(&numitems);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: GetNumberOfRedoItems() on empty redo stack failed. (%d)\n",
           result);
    return result;
  }

  if (numitems != 0) {
    printf("ERROR: GetNumberOfRedoItems() expected 0 got %d. (%d)\n",
           numitems, result);
    return result;
  }

  printf("passed\n");

  /*******************************************************************
   *
   * Undo 4 transactions. Afterwards, we should have 6 transactions
   * on the undo stack, and 4 on the redo stack:
   *
   *******************************************************************/

  printf("Undo 4 transactions ... ");

  for (i = 1; i <= 4; i++) {
    result = mgr->Undo();
    if (!NS_SUCCEEDED(result)) {
      printf("ERROR: Failed to undo transaction %d. (%d)\n", i, result);
      return result;
    }
  }

  result = mgr->GetNumberOfUndoItems(&numitems);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: GetNumberOfUndoItems() on undo stack with 6 items failed. (%d)\n",
           result);
    return result;
  }

  if (numitems != 6) {
    printf("ERROR: GetNumberOfUndoItems() expected 6 got %d. (%d)\n",
           numitems, result);
    return result;
  }

  result = mgr->GetNumberOfRedoItems(&numitems);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: GetNumberOfRedoItems() on empty redo stack failed. (%d)\n",
           result);
    return result;
  }

  if (numitems != 4) {
    printf("ERROR: GetNumberOfRedoItems() expected 4 got %d. (%d)\n",
           numitems, result);
    return result;
  }

  printf("passed\n");

  /*******************************************************************
   *
   * Redo 2 transactions. Afterwards, we should have 8 transactions
   * on the undo stack, and 2 on the redo stack:
   *
   *******************************************************************/

  printf("Redo 2 transactions ... ");

  for (i = 1; i <= 2; ++i) {
    result = mgr->Redo();
    if (!NS_SUCCEEDED(result)) {
      printf("ERROR: Failed to redo transaction %d. (%d)\n", i, result);
      return result;
    }
  }

  result = mgr->GetNumberOfUndoItems(&numitems);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: GetNumberOfUndoItems() on undo stack with 8 items failed. (%d)\n",
           result);
    return result;
  }

  if (numitems != 8) {
    printf("ERROR: GetNumberOfUndoItems() expected 8 got %d. (%d)\n",
           numitems, result);
    return result;
  }

  result = mgr->GetNumberOfRedoItems(&numitems);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: GetNumberOfRedoItems() on redo stack with 2 items failed. (%d)\n",
           result);
    return result;
  }

  if (numitems != 2) {
    printf("ERROR: GetNumberOfRedoItems() expected 2 got %d. (%d)\n",
           numitems, result);
    return result;
  }

  printf("passed\n");

  /*******************************************************************
   *
   * Execute a new transaction. The redo stack should get pruned!
   *
   *******************************************************************/

  printf("Check if new transactions prune the redo stack ... ");

  tximpl = new SimpleTransaction();
  tx     = 0;

  result = tximpl->QueryInterface(kITransactionIID, (void **)&tx);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: QueryInterface() failed for transaction. (%d)\n", result);
    return result;
  }

  result = mgr->Do(tx);
  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: Failed to execute transaction. (%d)\n", result);
    return result;
  }

  tx->Release();

  result = mgr->GetNumberOfUndoItems(&numitems);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: GetNumberOfUndoItems() on undo stack with 9 items failed. (%d)\n",
           result);
    return result;
  }

  if (numitems != 9) {
    printf("ERROR: GetNumberOfUndoItems() expected 9 got %d. (%d)\n",
           numitems, result);
    return result;
  }

  result = mgr->GetNumberOfRedoItems(&numitems);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: GetNumberOfRedoItems() on redo stack with 0 items failed. (%d)\n",
           result);
    return result;
  }

  if (numitems != 0) {
    printf("ERROR: GetNumberOfRedoItems() expected 0 got %d. (%d)\n",
           numitems, result);
    return result;
  }

  printf("passed\n");

  /*******************************************************************
   *
   * Undo 4 transactions then clear the undo and redo stacks.
   *
   *******************************************************************/

  printf("Undo 4 transactions then clear the undo and redo stacks ... ");

  for (i = 1; i <= 4; ++i) {
    result = mgr->Undo();
    if (!NS_SUCCEEDED(result)) {
      printf("ERROR: Failed to undo transaction %d. (%d)\n", i, result);
      return result;
    }
  }

  result = mgr->GetNumberOfUndoItems(&numitems);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: GetNumberOfUndoItems() on undo stack with 5 items failed. (%d)\n",
           result);
    return result;
  }

  if (numitems != 5) {
    printf("ERROR: GetNumberOfUndoItems() expected 5 got %d. (%d)\n",
           numitems, result);
    return result;
  }

  result = mgr->GetNumberOfRedoItems(&numitems);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: GetNumberOfRedoItems() on redo stack with 4 items failed. (%d)\n",
           result);
    return result;
  }

  if (numitems != 4) {
    printf("ERROR: GetNumberOfRedoItems() expected 4 got %d. (%d)\n",
           numitems, result);
    return result;
  }

  result = mgr->Clear();

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: Clear() failed. (%d)\n",
           result);
    return result;
  }

  result = mgr->GetNumberOfUndoItems(&numitems);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: GetNumberOfUndoItems() on cleared undo stack failed. (%d)\n",
           result);
    return result;
  }

  if (numitems != 0) {
    printf("ERROR: GetNumberOfUndoItems() expected 0 got %d. (%d)\n",
           numitems, result);
    return result;
  }

  result = mgr->GetNumberOfRedoItems(&numitems);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: GetNumberOfRedoItems() on empty cleared stack failed. (%d)\n",
           result);
    return result;
  }

  if (numitems != 0) {
    printf("ERROR: GetNumberOfRedoItems() expected 0 got %d. (%d)\n",
           numitems, result);
    return result;
  }

  printf("passed\n");

  /*******************************************************************
   *
   * Execute 5 transactions.
   *
   *******************************************************************/

  printf("Execute 5 simple transactions ... ");

  for (i = 1; i <= 5; i++) {
    tximpl = new SimpleTransaction();

    tx = 0;
    result = tximpl->QueryInterface(kITransactionIID, (void **)&tx);
    if (!NS_SUCCEEDED(result)) {
      printf("ERROR: QueryInterface() failed for transaction %d. (%d)\n",
             i, result);
      return result;
    }

    result = mgr->Do(tx);
    if (!NS_SUCCEEDED(result)) {
      printf("ERROR: Failed to execute transaction %d. (%d)\n", i, result);
      return result;
    }

    tx->Release();
  }

  result = mgr->GetNumberOfUndoItems(&numitems);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: GetNumberOfUndoItems() on undo stack with 5 items failed. (%d)\n",
           result);
    return result;
  }

  if (numitems != 5) {
    printf("ERROR: GetNumberOfUndoItems() expected 5 got %d. (%d)\n",
           numitems, result);
    return result;
  }

  result = mgr->GetNumberOfRedoItems(&numitems);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: GetNumberOfRedoItems() on empty redo stack failed. (%d)\n",
           result);
    return result;
  }

  if (numitems != 0) {
    printf("ERROR: GetNumberOfRedoItems() expected 0 got %d. (%d)\n",
           numitems, result);
    return result;
  }

  printf("passed\n");

  /*******************************************************************
   *
   * Test transaction Do() error:
   *
   *******************************************************************/

  printf("Test transaction Do() error ... ");

  DoErrorTransaction *de = new DoErrorTransaction();
  tx     = 0;

  result = de->QueryInterface(kITransactionIID, (void **)&tx);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: QueryInterface() failed for transaction. (%d)\n", result);
    return result;
  }

  nsITransaction *u1 = 0, *u2 = 0;
  nsITransaction *r1 = 0, *r2 = 0;

  result = mgr->PeekUndoStack(&u1);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: Initial PeekUndoStack() failed. (%d)\n", result);
    return result;
  }

  result = mgr->PeekRedoStack(&r1);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: Initial PeekRedoStack() failed. (%d)\n", result);
    return result;
  }

  result = mgr->Do(tx);

  if (!NS_SUCCEEDED(result) && result != NS_ERROR_FAILURE) {
    printf("ERROR: Do() returned unexpected error. (%d)\n", result);
    return result;
  }

  tx->Release();

  result = mgr->PeekUndoStack(&u2);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: Initial PeekUndoStack() failed. (%d)\n", result);
    return result;
  }

  if (u1 != u2) {
    printf("ERROR: Top of undo stack changed. (%d)\n", result);
    return result;
  }

  result = mgr->PeekRedoStack(&r2);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: Initial PeekRedoStack() failed. (%d)\n", result);
    return result;
  }

  if (r1 != r2) {
    printf("ERROR: Top of redo stack changed. (%d)\n", result);
    return result;
  }

  result = mgr->GetNumberOfUndoItems(&numitems);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: GetNumberOfUndoItems() on undo stack with 5 items failed. (%d)\n",
           result);
    return result;
  }

  if (numitems != 5) {
    printf("ERROR: GetNumberOfUndoItems() expected 5 got %d. (%d)\n",
           numitems, result);
    return result;
  }

  result = mgr->GetNumberOfRedoItems(&numitems);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: GetNumberOfRedoItems() on empty redo stack. (%d)\n",
           result);
    return result;
  }

  if (numitems != 0) {
    printf("ERROR: GetNumberOfRedoItems() expected 0 got %d. (%d)\n",
           numitems, result);
    return result;
  }

  printf("passed\n");

  /*******************************************************************
   *
   * Test transaction Undo() error:
   *
   *******************************************************************/

  printf("Test transaction Undo() error ... ");

  UndoErrorTransaction *ue = new UndoErrorTransaction();
  tx     = 0;

  result = ue->QueryInterface(kITransactionIID, (void **)&tx);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: QueryInterface() failed for transaction. (%d)\n", result);
    return result;
  }

  result = mgr->Do(tx);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: Do() returned unexpected error. (%d)\n", result);
    return result;
  }

  tx->Release();

  u1 = u2 = r1 = r2 = 0;

  result = mgr->PeekUndoStack(&u1);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: Initial PeekUndoStack() failed. (%d)\n", result);
    return result;
  }

  result = mgr->PeekRedoStack(&r1);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: Initial PeekRedoStack() failed. (%d)\n", result);
    return result;
  }

  result = mgr->Undo();

  if (!NS_SUCCEEDED(result) && result != NS_ERROR_FAILURE) {
    printf("ERROR: Undo() returned unexpected error. (%d)\n", result);
    return result;
  }

  result = mgr->PeekUndoStack(&u2);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: Initial PeekUndoStack() failed. (%d)\n", result);
    return result;
  }

  if (u1 != u2) {
    printf("ERROR: Top of undo stack changed. (%d)\n", result);
    return result;
  }

  result = mgr->PeekRedoStack(&r2);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: Initial PeekRedoStack() failed. (%d)\n", result);
    return result;
  }

  if (r1 != r2) {
    printf("ERROR: Top of redo stack changed. (%d)\n", result);
    return result;
  }

  result = mgr->GetNumberOfUndoItems(&numitems);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: GetNumberOfUndoItems() on undo stack with 6 items failed. (%d)\n",
           result);
    return result;
  }

  if (numitems != 6) {
    printf("ERROR: GetNumberOfUndoItems() expected 6 got %d. (%d)\n",
           numitems, result);
    return result;
  }

  result = mgr->GetNumberOfRedoItems(&numitems);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: GetNumberOfRedoItems() on empty redo stack. (%d)\n",
           result);
    return result;
  }

  if (numitems != 0) {
    printf("ERROR: GetNumberOfRedoItems() expected 0 got %d. (%d)\n",
           numitems, result);
    return result;
  }

  printf("passed\n");

  /*******************************************************************
   *
   * XXX Test transaction Redo() error:
   *
   *******************************************************************/

  /*******************************************************************
   *
   * XXX Test coalescing:
   *
   *******************************************************************/

  /*******************************************************************
   *
   * XXX Test aggregation:
   *
   *******************************************************************/

  /*******************************************************************
   *
   * XXX Test coalescing of aggregated transactions:
   *
   *******************************************************************/

  /*******************************************************************
   *
   * Release the transaction manager. Any transactions on the undo
   * and redo stack should automatically be released:
   *
   *******************************************************************/

  // mgr->Write(&console);

  printf("Release the transaction manager ... ");

  result = mgr->Release();

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: nsITransactionManager Release() failed. (%d)\n", result);
    return result;
  }

  printf("passed\n");

  /*******************************************************************
   *
   * Make sure number of transactions created matches number of
   * transactions destroyed!
   *
   *******************************************************************/

  printf("Number of transactions created and destroyed match ... ");

  if (SimpleTransaction::sConstructorCount != SimpleTransaction::sDestructorCount) {
    printf("ERROR: Transaction constructor count (%d) != destructor count (%d).\n",
           SimpleTransaction::sConstructorCount,
           SimpleTransaction::sDestructorCount);
    return result;
  }

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: nsITransactionManager Release() failed. (%d)\n", result);
    return result;
  }

  printf("passed\n");

  return NS_OK;
}

int
main (int argc, char *argv[])
{
  nsresult result;

  result = simple_test();

  if (!NS_SUCCEEDED(result))
    return result;

  fflush(stdout);

  return NS_OK;
}
