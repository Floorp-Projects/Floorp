#include "nsTransactionManager.h"

class TestTransaction : public nsITransaction
{
public:

  TestTransaction()  {}
  ~TestTransaction()
  {
    printf("~TestTransaction: 0x%.8x\n", this);
  }

  NS_DECL_ISUPPORTS

  virtual nsresult Do()
  {
    printf("0x%.8x: Do()\n", this);
    return NS_OK;
  }

  virtual nsresult Undo()
  {
    printf("0x%.8x: Undo()\n", this);
    return NS_OK;
  }

  virtual nsresult Redo()
  {
    printf("0x%.8x: Redo()\n", this);
    return NS_OK;
  }

  virtual nsresult Write(nsIOutputStream *aOutputStream)
  {
    return NS_OK;
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

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kITransactionIID, NS_ITRANSACTION_IID);

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

class TestDoErrorTransaction : public TestTransaction
{
public:

  virtual nsresult Do()
  {
    TestTransaction::Do();
    return NS_ERROR_FAILURE;
  }
};

class TestUndoErrorTransaction : public TestTransaction
{
public:

  virtual nsresult Undo()
  {
    TestTransaction::Undo();
    return NS_ERROR_FAILURE;
  }
};

class TestRedoErrorTransaction : public TestTransaction
{
public:

  virtual nsresult Redo()
  {
    TestTransaction::Redo();
    return NS_ERROR_FAILURE;
  }
};

int
main (int argc, char *argv[])
{
  nsTransactionManager     *mgr = new nsTransactionManager();
  TestTransaction          *tt  = new TestTransaction();
  TestDoErrorTransaction   *det = new TestDoErrorTransaction();
  TestUndoErrorTransaction *uet = new TestUndoErrorTransaction();
  TestRedoErrorTransaction *ret = new TestRedoErrorTransaction();

  mgr->AddRef();

  printf("-- TestTransaction: 0x%.8x\n", (nsITransaction *)tt);
  mgr->Do(tt);
  mgr->Undo();
  mgr->Redo();
  mgr->Undo();

  printf("-- TestDoErrorTransaction: 0x%.8x\n", (nsITransaction *)det);
  mgr->Do(det);
  mgr->Undo();
  mgr->Redo();
  mgr->Undo();

  printf("-- TestUndoErrorTransaction: 0x%.8x\n", (nsITransaction *)uet);
  mgr->Do(uet);
  mgr->Undo();
  mgr->Redo();
  mgr->Undo();

  printf("-- TestRedoErrorTransaction: 0x%.8x\n", (nsITransaction *)ret);
  mgr->Do(ret);
  mgr->Undo();
  mgr->Redo();
  mgr->Undo();

  mgr->Release();

  return 0;
}

