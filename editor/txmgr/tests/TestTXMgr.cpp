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

#if 0

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
  static PRInt32 sDestructorOrderArr[];

#define THROWS_NO_ERROR_FLAG    0
#define THROWS_DO_ERROR_FLAG    1
#define THROWS_UNDO_ERROR_FLAG  2
#define THROWS_REDO_ERROR_FLAG  4
#define MERGE_FLAG              8
#define TRANSIENT_FLAG         16

  PRInt32 mVal;
  PRInt32 mFlags;

  SimpleTransaction(PRInt32 aFlags=THROWS_NO_ERROR_FLAG)
                    : mVal(++sConstructorCount), mFlags(aFlags)
  {}

  virtual ~SimpleTransaction()
  {
    //
    // Make sure transactions are being destroyed in the order we expect!
    // Notice that we don't check to see if we go past the end of the array.
    // This is done on purpose since we want to crash if the order array is out
    // of date.
    //
    if (mVal != sDestructorOrderArr[sDestructorCount]) {
      printf("ERROR: ~SimpleTransaction expected %d got %d.\n",
             mVal, sDestructorOrderArr[sDestructorCount]);
      exit(NS_ERROR_FAILURE);
    }

    ++sDestructorCount;

    // printf("\n~SimpleTransaction: %d - 0x%.8x\n", mVal, this);

    mVal = -1;
  }

  virtual nsresult Do()
  {
    return (mFlags & THROWS_DO_ERROR_FLAG) ? NS_ERROR_FAILURE : NS_OK;
  }

  virtual nsresult Undo()
  {
    return (mFlags & THROWS_UNDO_ERROR_FLAG) ? NS_ERROR_FAILURE : NS_OK;
  }

  virtual nsresult Redo()
  {
    return (mFlags & THROWS_REDO_ERROR_FLAG) ? NS_ERROR_FAILURE : NS_OK;
  }

  virtual nsresult GetIsTransient(PRBool *aIsTransient)
  {
    if (aIsTransient)
      *aIsTransient = (mFlags & TRANSIENT_FLAG) ? PR_TRUE : PR_FALSE;

    return NS_OK;
  }

  virtual nsresult Merge(PRBool *aDidMerge, nsITransaction *aTransaction)
  {
    if (aDidMerge)
      *aDidMerge = (mFlags & MERGE_FLAG) ? PR_TRUE : PR_FALSE;

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

PRInt32 SimpleTransaction::sConstructorCount     = 0;
PRInt32 SimpleTransaction::sDestructorCount      = 0;
PRInt32 SimpleTransaction::sDestructorOrderArr[] = {
         2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 16,
        17, 18, 19, 20, 21,  1, 22, 23, 24, 25, 26, 27, 28, 29, 30,
        31, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55,
        56, 57, 58, 59, 60, 61, 41, 40, 62, 39, 38, 37, 36, 35, 34,
        33, 32, 68, 71, 70, 69, 67, 66, 65, 64, 63 };


nsresult
simple_test ()
{
  /*******************************************************************
   *
   * Create a transaction manager implementation:
   *
   *******************************************************************/

  printf("Create a transaction manager with 10 levels of undo ... ");

  int i;
  nsTransactionManager  *mgrimpl = new nsTransactionManager(10);
  nsITransactionManager  *mgr    = 0;
  nsITransaction *tx             = 0;
  SimpleTransaction *tximpl      = 0;
  nsITransaction *u1 = 0, *u2 = 0;
  nsITransaction *r1 = 0, *r2 = 0;


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
   * Test coalescing by executing a transaction that can merge any
   * command into itself. Then execute 20 transaction. Afterwards,
   * we should still have the first transaction sitting on the undo
   * stack.
   *
   *******************************************************************/

  printf("Test coalescing of transactions ... ");

  tximpl = new SimpleTransaction(MERGE_FLAG);
  tx = 0;

  result = tximpl->QueryInterface(kITransactionIID, (void **)&tx);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: QueryInterface() failed for initial transaction. (%d)\n",
           result);
    return result;
  }

  result = mgr->Do(tx);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: Failed to execute initial transaction. (%d)\n", result);
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

  result = mgr->PeekUndoStack(&u2);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: Second PeekUndoStack() failed. (%d)\n", result);
    return result;
  }

  if (u1 != u2) {
    printf("ERROR: Top of undo stack changed. (%d)\n", result);
    return result;
  }

  result = mgr->PeekRedoStack(&r2);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: Second PeekRedoStack() failed. (%d)\n", result);
    return result;
  }

  if (r1 != r2) {
    printf("ERROR: Top of redo stack changed. (%d)\n", result);
    return result;
  }

  result = mgr->GetNumberOfUndoItems(&numitems);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: GetNumberOfUndoItems() on undo stack with 1 item failed. (%d)\n",
           result);
    return result;
  }

  if (numitems != 1) {
    printf("ERROR: GetNumberOfUndoItems() expected 1 got %d. (%d)\n",
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

  result = mgr->Clear();

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: Clear() failed. (%d)\n", result);
    return result;
  }

  printf("passed\n");

  /*******************************************************************
   *
   * Execute 20 transactions. Afterwards, we should have 10
   * transactions on the undo stack:
   *
   *******************************************************************/

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
   * Execute 20 transient transactions. Afterwards, we should still
   * have the same 10 transactions on the undo stack:
   *
   *******************************************************************/

  printf("Execute 20 transient transactions ... ");

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

  for (i = 1; i <= 20; i++) {
    tximpl = new SimpleTransaction(TRANSIENT_FLAG);

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

  result = mgr->PeekUndoStack(&u2);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: Second PeekUndoStack() failed. (%d)\n", result);
    return result;
  }

  if (u1 != u2) {
    printf("ERROR: Top of undo stack changed. (%d)\n", result);
    return result;
  }

  result = mgr->PeekRedoStack(&r2);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: Second PeekRedoStack() failed. (%d)\n", result);
    return result;
  }

  if (r1 != r2) {
    printf("ERROR: Top of redo stack changed. (%d)\n", result);
    return result;
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

  tximpl = new SimpleTransaction(THROWS_DO_ERROR_FLAG);
  tx     = 0;

  result = tximpl->QueryInterface(kITransactionIID, (void **)&tx);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: QueryInterface() failed for transaction. (%d)\n", result);
    return result;
  }

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

  result = mgr->Do(tx);

  if (!NS_SUCCEEDED(result) && result != NS_ERROR_FAILURE) {
    printf("ERROR: Do() returned unexpected error. (%d)\n", result);
    return result;
  }

  tx->Release();

  result = mgr->PeekUndoStack(&u2);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: Second PeekUndoStack() failed. (%d)\n", result);
    return result;
  }

  if (u1 != u2) {
    printf("ERROR: Top of undo stack changed. (%d)\n", result);
    return result;
  }

  result = mgr->PeekRedoStack(&r2);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: Second PeekRedoStack() failed. (%d)\n", result);
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

  tximpl = new SimpleTransaction(THROWS_UNDO_ERROR_FLAG);
  tx     = 0;

  result = tximpl->QueryInterface(kITransactionIID, (void **)&tx);

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
    printf("ERROR: Second PeekUndoStack() failed. (%d)\n", result);
    return result;
  }

  if (u1 != u2) {
    printf("ERROR: Top of undo stack changed. (%d)\n", result);
    return result;
  }

  result = mgr->PeekRedoStack(&r2);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: Second PeekRedoStack() failed. (%d)\n", result);
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
   * Test transaction Redo() error:
   *
   *******************************************************************/

  printf("Test transaction Redo() error ... ");

  tximpl = new SimpleTransaction(THROWS_REDO_ERROR_FLAG);
  tx     = 0;

  result = tximpl->QueryInterface(kITransactionIID, (void **)&tx);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: QueryInterface() failed for RedoErrorTransaction. (%d)\n",
           result);
    return result;
  }

  result = mgr->Do(tx);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: Do() returned unexpected error. (%d)\n", result);
    return result;
  }

  tx->Release();

  //
  // Execute a normal transaction to be used in a later test:
  //

  tximpl = new SimpleTransaction();
  tx     = 0;

  result = tximpl->QueryInterface(kITransactionIID, (void **)&tx);

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

  //
  // Undo the 2 transactions just executed.
  //

  for (i = 1; i <= 2; ++i) {
    result = mgr->Undo();
    if (!NS_SUCCEEDED(result)) {
      printf("ERROR: Failed to undo transaction %d. (%d)\n", i, result);
      return result;
    }
  }

  //
  // The RedoErrorTransaction should now be at the top of the redo stack!
  //

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

  result = mgr->Redo();

  if (!NS_SUCCEEDED(result) && result != NS_ERROR_FAILURE) {
    printf("ERROR: Redo() returned unexpected error. (%d)\n", result);
    return result;
  }

  result = mgr->PeekUndoStack(&u2);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: Second PeekUndoStack() failed. (%d)\n", result);
    return result;
  }

  if (u1 != u2) {
    printf("ERROR: Top of undo stack changed. (%d)\n", result);
    return result;
  }

  result = mgr->PeekRedoStack(&r2);

  if (!NS_SUCCEEDED(result)) {
    printf("ERROR: Second PeekRedoStack() failed. (%d)\n", result);
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

  if (numitems != 2) {
    printf("ERROR: GetNumberOfRedoItems() expected 2 got %d. (%d)\n",
           numitems, result);
    return result;
  }

  printf("passed\n");

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

  return NS_OK;
}
