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
  ~ConsoleOutput() {}

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
  char *mName;

  TestTransaction(const char *aName="")
  {
    PRInt32 len = strlen(aName) + 1;
    mName = new char[len];
    strncpy(mName, aName, len);
  }

  ~TestTransaction()
  {
    printf("TestTransaction: %s - 0x%.8x\n", mName, this);
  }

  NS_DECL_ISUPPORTS

  virtual nsresult Do()
  {
    // printf("Do() ");
    // this->Write(&console);
    return NS_OK;
  }

  virtual nsresult Undo()
  {
    // printf("Undo() ");
    //  this->Write(&console);
    return NS_OK;
  }

  virtual nsresult Redo()
  {
    // printf("Redo() ");
    // this->Write(&console);
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

    sprintf(buf, "Transaction: %s - 0x%.8x\n", mName, this);
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

NS_IMPL_ADDREF(TestTransaction)
NS_IMPL_RELEASE(TestTransaction)

nsresult
TestTransaction::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (nsnull == aInstancePtr) {
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

class TestDoErrorTransaction : public TestTransaction
{
public:

  TestDoErrorTransaction(const char *aName) : TestTransaction(aName) {}

  virtual nsresult Do()
  {
    TestTransaction::Do();
    return NS_ERROR_FAILURE;
  }
};

class TestUndoErrorTransaction : public TestTransaction
{
public:
  TestUndoErrorTransaction(const char *aName) : TestTransaction(aName) {}

  virtual nsresult Undo()
  {
    TestTransaction::Undo();
    return NS_ERROR_FAILURE;
  }
};

class TestRedoErrorTransaction : public TestTransaction
{
public:
  TestRedoErrorTransaction(const char *aName) : TestTransaction(aName) {}

  virtual nsresult Redo()
  {
    TestTransaction::Redo();
    return NS_ERROR_FAILURE;
  }
};

class TestNestedTransaction : public TestTransaction
{
  nsITransactionManager *mTXMgr;

public:
  TestNestedTransaction(nsITransactionManager *txmgr, const char *aName)
     : mTXMgr(txmgr), TestTransaction(aName) {}

  virtual nsresult Do()
  {
    PRInt32 i;
    char buf[256];

    for (i = 1; i <= 5; i++) {
      sprintf(buf, "%s-%d", mName, i);
      mTXMgr->Do(new TestTransaction(buf));
    }
    return NS_OK;;
  }
};

class TestNestedTransaction2 : public TestTransaction
{
  nsITransactionManager *mTXMgr;

public:
  TestNestedTransaction2(nsITransactionManager *txmgr, const char *aName)
     : mTXMgr(txmgr), TestTransaction(aName) {}

  virtual nsresult Do()
  {
    PRInt32 i;
    char buf[256];

    for (i = 1; i <= 5; i++) {
      sprintf(buf, "%s-%d", mName, i);
      mTXMgr->Do(new TestNestedTransaction(mTXMgr, buf));
    }
    return NS_OK;;
  }
};

int
main (int argc, char *argv[])
{
  nsTransactionManager  *mgrimpl = new nsTransactionManager();
  nsITransactionManager  *mgr    = 0;

  nsresult result = mgrimpl->QueryInterface(kITransactionManagerIID,
                                            (void **)&mgr);

  if (!NS_SUCCEEDED(result)) {
    printf("Failed to get TransactionManager interface. (%d)\n", result);
    return result;
  }

  if (!mgr) {
    printf("QueryInterface() returned NULL pointer.\n");
    return NS_ERROR_NULL_POINTER;
  }

  printf("Call Do() with null transaction ... ");
  result = mgr->Do(0);

  if (!NS_SUCCEEDED(result)
      && result != NS_ERROR_NULL_POINTER) {
    printf("Failed to get TransactionManager interface. (%d)\n", result);
    return result;
  }

  printf("passed\n");

  printf("Call Undo() with empty undo stack ... ");
  result = mgr->Undo();

  if (!NS_SUCCEEDED(result)) {
    printf("Undo on empty undo stack failed. (%d)\n", result);
    return result;
  }

  printf("passed\n");

  printf("Call Redo() with empty undo stack ... ");
  result = mgr->Redo();

  if (!NS_SUCCEEDED(result)) {
    printf("Redo on empty redo stack failed. (%d)\n", result);
    return result;
  }

  printf("passed\n");

  PRInt32 numitems = 0;

  printf("Call GetNumberOfUndoItems() with empty undo stack ... ");
  result = mgr->GetNumberOfUndoItems(&numitems);

  if (!NS_SUCCEEDED(result)) {
    printf("GetNumberOfUndoItems() on empty undo stack failed. (%d)\n", result);
    return result;
  }

  if (numitems != 0) {
    printf("GetNumberOfUndoItems() on empty undo stack failed. (%d)\n", result);
    return result;
  }

  printf("passed\n");

  printf("Call GetNumberOfRedoItems() with empty undo stack ... ");
  result = mgr->GetNumberOfRedoItems(&numitems);

  if (!NS_SUCCEEDED(result)) {
    printf("GetNumberOfRedoItems() on empty redo stack failed. (%d)\n", result);
    return result;
  }

  if (numitems != 0) {
    printf("GetNumberOfRedoItems() on empty redo stack failed. (%d)\n", result);
    return result;
  }

  printf("passed\n");

  int i = 0;

  printf("Execute 10 simple transactions ... ");

  for (i = 1; i <= 10; i++) {
    char buf[256];
    sprintf(buf, "%d", i);

    TestTransaction *tximpl = new TestTransaction(buf);
    nsITransaction *tx      = 0;

    result = tximpl->QueryInterface(kITransactionIID, (void **)&tx);
    if (!NS_SUCCEEDED(result)) {
      printf("QueryInterface() failed for transaction %d. (%d)\n", i, result);
      return result;
    }

    result = mgr->Do(tx);
    if (!NS_SUCCEEDED(result)) {
      printf("Failed to execute transaction %d. (%d)\n", i, result);
      return result;
    }

    tx->Release();
  }

  result = mgr->GetNumberOfUndoItems(&numitems);

  if (!NS_SUCCEEDED(result)) {
    printf("GetNumberOfUndoItems() on undo stack with 10 items failed. (%d)\n",
           result);
    return result;
  }

  if (numitems != 10) {
    printf("GetNumberOfUndoItems() on undo stack failed. (%d)\n", result);
    return result;
  }

  result = mgr->GetNumberOfRedoItems(&numitems);

  if (!NS_SUCCEEDED(result)) {
    printf("GetNumberOfRedoItems() on empty redo stack failed. (%d)\n", result);
    return result;
  }

  if (numitems != 0) {
    printf("GetNumberOfRedoItems() on empty redo stack failed. (%d)\n", result);
    return result;
  }

  printf("passed\n");

  printf("Undo 4 transactions ... ");

  for (i = 10; i > 6; i--) {
    result = mgr->Undo();
    if (!NS_SUCCEEDED(result)) {
      printf("Failed to undo transaction %d. (%d)\n", i, result);
      return result;
    }
  }

  result = mgr->GetNumberOfUndoItems(&numitems);

  if (!NS_SUCCEEDED(result)) {
    printf("GetNumberOfUndoItems() on undo stack with 10 items failed. (%d)\n",
           result);
    return result;
  }

  if (numitems != 6) {
    printf("GetNumberOfUndoItems() on undo stack failed. (%d)\n", result);
    return result;
  }

  result = mgr->GetNumberOfRedoItems(&numitems);

  if (!NS_SUCCEEDED(result)) {
    printf("GetNumberOfRedoItems() on empty redo stack failed. (%d)\n", result);
    return result;
  }

  if (numitems != 4) {
    printf("GetNumberOfRedoItems() on empty redo stack failed. (%d)\n", result);
    return result;
  }

  printf("passed\n");

  mgr->Release();

  return 0;
}

