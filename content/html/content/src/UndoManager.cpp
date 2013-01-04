/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/UndoManager.h"

#include "nsDOMClassInfoID.h"
#include "nsIClassInfo.h"
#include "nsIXPCScriptable.h"
#include "nsIVariant.h"
#include "nsVariant.h"
#include "nsINode.h"
#include "nsIDOMDOMTransactionEvent.h"
#include "nsEventDispatcher.h"
#include "nsContentUtils.h"
#include "jsapi.h"

#include "mozilla/Preferences.h"
#include "mozilla/ErrorResult.h"

#include "nsIUndoManagerTransaction.h"

// Includes for mutation observer.
#include "nsIDOMHTMLElement.h"
#include "nsStubMutationObserver.h"
#include "nsAutoPtr.h"
#include "nsTransactionManager.h"

// Includes for attribute changed transaction.
#include "nsITransaction.h"
#include "nsIContent.h"
#include "nsIDOMMutationEvent.h"
#include "mozilla/dom/Element.h"

// Includes for text content changed.
#include "nsTextFragment.h"

using namespace mozilla;
using namespace mozilla::dom;

/////////////////////////////////////////////////
// UndoTxn
/////////////////////////////////////////////////

/**
 * A base class to implement methods that behave the same for all
 * UndoManager transactions.
 */
class UndoTxn : public nsITransaction {
  NS_DECL_NSITRANSACTION
protected:
  virtual ~UndoTxn() {}
};

/* void doTransaction (); */
NS_IMETHODIMP
UndoTxn::DoTransaction()
{
  // Do not do anything the first time we apply this transaction,
  // changes should already have been applied.
  return NS_OK;
}

/* void doTransaction (); */
NS_IMETHODIMP
UndoTxn::RedoTransaction()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void doTransaction (); */
NS_IMETHODIMP
UndoTxn::UndoTransaction()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute boolean isTransient; */
NS_IMETHODIMP
UndoTxn::GetIsTransient(bool* aIsTransient)
{
  *aIsTransient = false;
  return NS_OK;
}

/* boolean merge (in nsITransaction aTransaction); */
NS_IMETHODIMP
UndoTxn::Merge(nsITransaction* aTransaction, bool* aResult)
{
  *aResult = false;
  return NS_OK;
}

/////////////////////////////////////////////////
// UndoAttrChanged
/////////////////////////////////////////////////

/**
 * Transaction to handle an attribute change to a nsIContent.
 */
class UndoAttrChanged : public UndoTxn {
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(UndoAttrChanged)
  NS_IMETHOD RedoTransaction();
  NS_IMETHOD UndoTransaction();
  nsresult Init();
  UndoAttrChanged(mozilla::dom::Element* aElement, int32_t aNameSpaceID,
                  nsIAtom* aAttribute, int32_t aModType);
protected:
  nsresult SaveRedoState();
  nsCOMPtr<nsIContent> mElement;
  int32_t mNameSpaceID;
  nsCOMPtr<nsIAtom> mAttrAtom;
  int32_t mModType;
  nsString mRedoValue;
  nsString mUndoValue;
};

NS_IMPL_CYCLE_COLLECTION_2(UndoAttrChanged, mElement, mAttrAtom)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(UndoAttrChanged)
  NS_INTERFACE_MAP_ENTRY(nsITransaction)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(UndoAttrChanged)
NS_IMPL_CYCLE_COLLECTING_RELEASE(UndoAttrChanged)

UndoAttrChanged::UndoAttrChanged(mozilla::dom::Element* aElement,
                                 int32_t aNameSpaceID, nsIAtom* aAttribute,
                                 int32_t aModType)
  : mElement(aElement), mNameSpaceID(aNameSpaceID), mAttrAtom(aAttribute),
    mModType(aModType) {}

nsresult
UndoAttrChanged::SaveRedoState()
{
   mElement->GetAttr(mNameSpaceID, mAttrAtom, mRedoValue);
   return NS_OK;
}

nsresult
UndoAttrChanged::Init()
{
  mElement->GetAttr(mNameSpaceID, mAttrAtom, mUndoValue);
  return NS_OK;
}

NS_IMETHODIMP
UndoAttrChanged::UndoTransaction()
{
  nsresult rv = SaveRedoState();
  NS_ENSURE_SUCCESS(rv, rv);

  switch (mModType) {
  case nsIDOMMutationEvent::MODIFICATION:
    mElement->SetAttr(mNameSpaceID, mAttrAtom, mUndoValue, true);
    return NS_OK;
  case nsIDOMMutationEvent::ADDITION:
    mElement->UnsetAttr(mNameSpaceID, mAttrAtom, true);
    return NS_OK;
  case nsIDOMMutationEvent::REMOVAL:
    if (!mElement->HasAttr(mNameSpaceID, mAttrAtom)) {
      mElement->SetAttr(mNameSpaceID, mAttrAtom, mUndoValue, true);
    }
    return NS_OK;
  }

  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
UndoAttrChanged::RedoTransaction()
{
  switch (mModType) {
  case nsIDOMMutationEvent::MODIFICATION:
    mElement->SetAttr(mNameSpaceID, mAttrAtom, mRedoValue, true);
    return NS_OK;
  case nsIDOMMutationEvent::ADDITION:
    if (!mElement->HasAttr(mNameSpaceID, mAttrAtom)) {
      mElement->SetAttr(mNameSpaceID, mAttrAtom, mRedoValue, true);
    }
    return NS_OK;
  case nsIDOMMutationEvent::REMOVAL:
    mElement->UnsetAttr(mNameSpaceID, mAttrAtom, true);
    return NS_OK;
  }

  return NS_ERROR_UNEXPECTED;
}

/////////////////////////////////////////////////
// UndoTextChanged
/////////////////////////////////////////////////

struct UndoCharacterChangedData {
  bool mAppend;
  uint32_t mChangeStart;
  uint32_t mChangeEnd;
  uint32_t mReplaceLength;
  explicit UndoCharacterChangedData(CharacterDataChangeInfo* aChange)
    : mAppend(aChange->mAppend), mChangeStart(aChange->mChangeStart),
      mChangeEnd(aChange->mChangeEnd),
      mReplaceLength(aChange->mReplaceLength) {}
};

/**
 * Transaction to handle a text change to a nsIContent.
 */
class UndoTextChanged : public UndoTxn {
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(UndoTextChanged)
  NS_IMETHOD RedoTransaction();
  NS_IMETHOD UndoTransaction();
  UndoTextChanged(nsIContent* aContent,
                  CharacterDataChangeInfo* aChange);
protected:
  void SaveRedoState();
  nsCOMPtr<nsIContent> mContent;
  UndoCharacterChangedData mChange;
  nsString mRedoValue;
  nsString mUndoValue;
};

NS_IMPL_CYCLE_COLLECTION_1(UndoTextChanged, mContent)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(UndoTextChanged)
  NS_INTERFACE_MAP_ENTRY(nsITransaction)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(UndoTextChanged)
NS_IMPL_CYCLE_COLLECTING_RELEASE(UndoTextChanged)

UndoTextChanged::UndoTextChanged(nsIContent* aContent,
                                 CharacterDataChangeInfo* aChange)
  : mContent(aContent), mChange(aChange)
{
  const nsTextFragment* text = mContent->GetText();
  int32_t numReplaced = mChange.mChangeEnd - mChange.mChangeStart;
  text->AppendTo(mUndoValue, mChange.mChangeStart, numReplaced);
}

nsresult
UndoTextChanged::RedoTransaction()
{
  nsAutoString text;
  mContent->AppendTextTo(text);

  if (text.Length() < mChange.mChangeStart) {
    return NS_OK;
  }

  if (mChange.mAppend) {
    mContent->AppendText(mRedoValue.get(), mRedoValue.Length(), true);
  } else {
    int32_t numReplaced = mChange.mChangeEnd - mChange.mChangeStart;
    text.Replace(mChange.mChangeStart, numReplaced, mRedoValue);
    mContent->SetText(text, true);
  }

  return NS_OK;
}

nsresult
UndoTextChanged::UndoTransaction()
{
  SaveRedoState();

  nsAutoString text;
  mContent->AppendTextTo(text);

  if (text.Length() < mChange.mChangeStart) {
    return NS_OK;
  }

  if (mChange.mAppend) {
    text.Truncate(text.Length() - mRedoValue.Length());
  } else {
    text.Replace(mChange.mChangeStart, mRedoValue.Length(), mUndoValue);
  }
  mContent->SetText(text, true);

  return NS_OK;
}

void
UndoTextChanged::SaveRedoState()
{
  const nsTextFragment* text = mContent->GetText();
  text->AppendTo(mRedoValue, mChange.mChangeStart, mChange.mReplaceLength);
}

/////////////////////////////////////////////////
// UndoContentAppend
/////////////////////////////////////////////////

/**
 * Transaction to handle appending content to a nsIContent.
 */
class UndoContentAppend : public UndoTxn {
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(UndoContentAppend)
  nsresult Init(int32_t aFirstIndex);
  NS_IMETHOD RedoTransaction();
  NS_IMETHOD UndoTransaction();
  UndoContentAppend(nsIContent* aContent);
protected:
  nsCOMPtr<nsIContent> mContent;
  nsCOMArray<nsIContent> mChildren;
};

NS_IMPL_CYCLE_COLLECTION_2(UndoContentAppend, mContent, mChildren)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(UndoContentAppend)
  NS_INTERFACE_MAP_ENTRY(nsITransaction)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(UndoContentAppend)
NS_IMPL_CYCLE_COLLECTING_RELEASE(UndoContentAppend)

UndoContentAppend::UndoContentAppend(nsIContent* aContent)
{
  mContent = aContent;
}

nsresult
UndoContentAppend::Init(int32_t aFirstIndex)
{
  for (uint32_t i = aFirstIndex; i < mContent->GetChildCount(); i++) {
    NS_ENSURE_TRUE(mChildren.AppendObject(mContent->GetChildAt(i)),
                   NS_ERROR_OUT_OF_MEMORY);
  }

  return NS_OK;
}

nsresult
UndoContentAppend::RedoTransaction()
{
  for (int32_t i = 0; i < mChildren.Count(); i++) {
    if (!mChildren[i]->GetParent()) {
      mContent->AppendChildTo(mChildren[i], true);
    }
  }

  return NS_OK;
}

nsresult
UndoContentAppend::UndoTransaction()
{
  for (int32_t i = mChildren.Count() - 1; i >= 0; i--) {
    if (mChildren[i]->GetParent() == mContent) {
      ErrorResult error;
      mContent->RemoveChild(*mChildren[i], error);
    }
  }

  return NS_OK;
}

/////////////////////////////////////////////////
// UndoContentInsert
/////////////////////////////////////////////////

/**
 * Transaction to handle inserting content into a nsIContent.
 */
class UndoContentInsert : public UndoTxn {
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(UndoContentInsert)
  NS_IMETHOD UndoTransaction();
  NS_IMETHOD RedoTransaction();
  UndoContentInsert(nsIContent* aContent, nsIContent* aChild,
                    int32_t aInsertIndex);
protected:
  nsCOMPtr<nsIContent> mContent;
  nsCOMPtr<nsIContent> mChild;
  nsCOMPtr<nsIContent> mNextNode;
};

NS_IMPL_CYCLE_COLLECTION_3(UndoContentInsert, mContent, mChild, mNextNode)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(UndoContentInsert)
  NS_INTERFACE_MAP_ENTRY(nsITransaction)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(UndoContentInsert)
NS_IMPL_CYCLE_COLLECTING_RELEASE(UndoContentInsert)

UndoContentInsert::UndoContentInsert(nsIContent* aContent,
                                     nsIContent* aChild,
                                     int32_t aInsertIndex)
  : mContent(aContent), mChild(aChild)
{
  mNextNode = mContent->GetChildAt(aInsertIndex + 1);
}

nsresult
UndoContentInsert::RedoTransaction()
{
  if (!mChild) {
    return NS_ERROR_UNEXPECTED;
  }

  // Check if node already has parent.
  if (mChild->GetParent()) {
    return NS_OK;
  }

  // Check to see if next sibling has same parent.
  if (mNextNode && mNextNode->GetParent() != mContent) {
    return NS_OK;
  }

  ErrorResult error;
  mContent->InsertBefore(*mChild, mNextNode, error);
  return NS_OK;
}

nsresult
UndoContentInsert::UndoTransaction()
{
  if (!mChild) {
    return NS_ERROR_UNEXPECTED;
  }

  // Check if the parent is the same.
  if (mChild->GetParent() != mContent) {
    return NS_OK;
  }

  // Check of the parent of the next node is the same.
  if (mNextNode && mNextNode->GetParent() != mContent) {
    return NS_OK;
  }

  // Check that the next node has not changed.
  if (mChild->GetNextSibling() != mNextNode) {
    return NS_OK;
  }

  ErrorResult error;
  mContent->RemoveChild(*mChild, error);
  return NS_OK;
}

/////////////////////////////////////////////////
// UndoContentRemove
/////////////////////////////////////////////////

/**
 * Transaction to handle removing content from an nsIContent.
 */
class UndoContentRemove : public UndoTxn {
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(UndoContentRemove)
  NS_IMETHOD UndoTransaction();
  NS_IMETHOD RedoTransaction();
  nsresult Init(int32_t aInsertIndex);
  UndoContentRemove(nsIContent* aContent, nsIContent* aChild,
                    int32_t aInsertIndex);
protected:
  nsCOMPtr<nsIContent> mContent;
  nsCOMPtr<nsIContent> mChild;
  nsCOMPtr<nsIContent> mNextNode;
};

NS_IMPL_CYCLE_COLLECTION_3(UndoContentRemove, mContent, mChild, mNextNode)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(UndoContentRemove)
  NS_INTERFACE_MAP_ENTRY(nsITransaction)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(UndoContentRemove)
NS_IMPL_CYCLE_COLLECTING_RELEASE(UndoContentRemove)

nsresult
UndoContentRemove::Init(int32_t aInsertIndex)
{
  return NS_OK;
}

UndoContentRemove::UndoContentRemove(nsIContent* aContent, nsIContent* aChild,
                                     int32_t aInsertIndex)
  : mContent(aContent), mChild(aChild)
{
  mNextNode = mContent->GetChildAt(aInsertIndex);
}

nsresult
UndoContentRemove::UndoTransaction()
{
  if (!mChild) {
    return NS_ERROR_UNEXPECTED;
  }

  // Check if child has a parent.
  if (mChild->GetParent()) {
    return NS_OK;
  }

  // Make sure next sibling is still under same parent.
  if (mNextNode && mNextNode->GetParent() != mContent) {
    return NS_OK;
  }

  ErrorResult error;
  mContent->InsertBefore(*mChild, mNextNode, error);
  return NS_OK;
}

nsresult
UndoContentRemove::RedoTransaction()
{
  if (!mChild) {
    return NS_ERROR_UNEXPECTED;
  }

  // Check that the parent has not changed.
  if (mChild->GetParent() != mContent) {
    return NS_OK;
  }

  // Check that the next node still has the same parent.
  if (mNextNode && mNextNode->GetParent() != mContent) {
    return NS_OK;
  }

  // Check that the next sibling has not changed.
  if (mChild->GetNextSibling() != mNextNode) {
    return NS_OK;
  }

  ErrorResult error;
  mContent->RemoveChild(*mChild, error);
  return NS_OK;
}

/////////////////////////////////////////////////
// UndoMutationObserver
/////////////////////////////////////////////////

/**
 * Watches for DOM mutations in a particular element and its
 * descendants to create transactions that undo and redo
 * the mutations. Each UndoMutationObserver corresponds
 * to an undo scope.
 */
class UndoMutationObserver : public nsStubMutationObserver {
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIMUTATIONOBSERVER_ATTRIBUTEWILLCHANGE
  NS_DECL_NSIMUTATIONOBSERVER_CHARACTERDATAWILLCHANGE
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTAPPENDED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTINSERTED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTREMOVED
  explicit UndoMutationObserver(nsITransactionManager* aTxnManager);

protected:
  /**
   * Checks if |aContent| is within the undo scope of this
   * UndoMutationObserver.
   */
  bool IsManagerForMutation(nsIContent* aContent);
  virtual ~UndoMutationObserver() {}
  nsITransactionManager* mTxnManager; // [RawPtr] UndoManager holds strong
                                      // reference.
};

NS_IMPL_ISUPPORTS1(UndoMutationObserver, nsIMutationObserver)

bool
UndoMutationObserver::IsManagerForMutation(nsIContent* aContent)
{
  nsCOMPtr<nsIContent> content = aContent;
  nsRefPtr<UndoManager> undoManager;

  // Get the UndoManager of nearest ancestor with an UndoManager.
  while (content && !undoManager) {
    nsCOMPtr<Element> htmlElem = do_QueryInterface(content);
    if (htmlElem) {
      undoManager = htmlElem->GetUndoManager();
    }

    content = content->GetParent();
  }

  if (!undoManager) {
    // Check against document UndoManager if we were unable to find an
    // UndoManager in an ancestor element.
    nsIDocument* doc = aContent->OwnerDoc();
    NS_ENSURE_TRUE(doc, false);
    undoManager = doc->GetUndoManager();
  }

  // Check if the nsITransactionManager is the same for both the
  // mutation observer and the nsIContent.
  return undoManager->GetTransactionManager() == mTxnManager;
}

UndoMutationObserver::UndoMutationObserver(nsITransactionManager* aTxnManager)
  : mTxnManager(aTxnManager) {}

void
UndoMutationObserver::AttributeWillChange(nsIDocument* aDocument,
                                          mozilla::dom::Element* aElement,
                                          int32_t aNameSpaceID,
                                          nsIAtom* aAttribute,
                                          int32_t aModType)
{
  if (!IsManagerForMutation(aElement)) {
    return;
  }

  nsRefPtr<UndoAttrChanged> undoTxn = new UndoAttrChanged(aElement,
                                                          aNameSpaceID,
                                                          aAttribute,
                                                          aModType);
  if (NS_SUCCEEDED(undoTxn->Init())) {
    mTxnManager->DoTransaction(undoTxn);
  }
}

void
UndoMutationObserver::CharacterDataWillChange(nsIDocument* aDocument,
                                              nsIContent* aContent,
                                              CharacterDataChangeInfo* aInfo)
{
  if (!IsManagerForMutation(aContent)) {
    return;
  }

  nsRefPtr<UndoTextChanged> undoTxn = new UndoTextChanged(aContent, aInfo);
  mTxnManager->DoTransaction(undoTxn);
}

void
UndoMutationObserver::ContentAppended(nsIDocument* aDocument,
                                      nsIContent* aContainer,
                                      nsIContent* aFirstNewContent,
                                      int32_t aNewIndexInContainer)
{
  if (!IsManagerForMutation(aContainer)) {
    return;
  }

  nsRefPtr<UndoContentAppend> txn = new UndoContentAppend(aContainer);
  if (NS_SUCCEEDED(txn->Init(aNewIndexInContainer))) {
    mTxnManager->DoTransaction(txn);
  }
}

void
UndoMutationObserver::ContentInserted(nsIDocument* aDocument,
                                      nsIContent* aContainer,
                                      nsIContent* aChild,
                                      int32_t aIndexInContainer)
{
  if (!IsManagerForMutation(aContainer)) {
    return;
  }

  nsRefPtr<UndoContentInsert> txn = new UndoContentInsert(aContainer, aChild,
                                                          aIndexInContainer);
  mTxnManager->DoTransaction(txn);
}

void
UndoMutationObserver::ContentRemoved(nsIDocument *aDocument,
                                     nsIContent* aContainer,
                                     nsIContent* aChild,
                                     int32_t aIndexInContainer,
                                     nsIContent* aPreviousSibling)
{
  if (!IsManagerForMutation(aContainer)) {
    return;
  }

  nsRefPtr<UndoContentRemove> txn = new UndoContentRemove(aContainer, aChild,
                                                          aIndexInContainer);
  mTxnManager->DoTransaction(txn);
}

/////////////////////////////////////////////////
// FunctionCallTxn
/////////////////////////////////////////////////

/**
 * A transaction that calls members on the transaction
 * object.
 */
class FunctionCallTxn : public UndoTxn {
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(FunctionCallTxn)

  // Flags
  static const uint32_t CALL_ON_REDO = 1;
  static const uint32_t CALL_ON_UNDO = 2;

  NS_IMETHOD RedoTransaction();
  NS_IMETHOD UndoTransaction();
  FunctionCallTxn(nsIUndoManagerTransaction* aTransaction, uint32_t aFlags);
protected:
  /**
   * Call a function member on the transaction object with the
   * specified function name.
   */
  nsresult CallTransactionMember(const char* aFunctionName);
  nsCOMPtr<nsIUndoManagerTransaction> mTransaction;
  uint32_t mFlags;
};

NS_IMPL_CYCLE_COLLECTION_1(FunctionCallTxn, mTransaction)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(FunctionCallTxn)
  NS_INTERFACE_MAP_ENTRY(nsITransaction)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(FunctionCallTxn)
NS_IMPL_CYCLE_COLLECTING_RELEASE(FunctionCallTxn)

FunctionCallTxn::FunctionCallTxn(nsIUndoManagerTransaction* aTransaction,
                                 uint32_t aFlags)
  : mTransaction(aTransaction), mFlags(aFlags) {}

nsresult
FunctionCallTxn::RedoTransaction()
{
  if (!(mFlags & CALL_ON_REDO)) {
    return NS_OK;
  }

  mTransaction->Redo();

  return NS_OK;
}

nsresult
FunctionCallTxn::UndoTransaction()
{
  if (!(mFlags & CALL_ON_UNDO)) {
    return NS_OK;
  }

  mTransaction->Undo();

  return NS_OK;
}

/////////////////////////////////////////////////
// TxnScopeGuard
/////////////////////////////////////////////////
namespace mozilla {
namespace dom {

class TxnScopeGuard {
public:
  explicit TxnScopeGuard(UndoManager* aUndoManager)
    : mUndoManager(aUndoManager)
  {
    mUndoManager->mInTransaction = true;
  }

  ~TxnScopeGuard()
  {
    mUndoManager->mInTransaction = false;
  }
protected:
  UndoManager* mUndoManager;
};

} // namespace dom
} // namespace mozilla

/////////////////////////////////////////////////
// UndoManager
/////////////////////////////////////////////////

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_2(UndoManager, mTxnManager, mHostNode)
NS_IMPL_CYCLE_COLLECTING_ADDREF(UndoManager)
NS_IMPL_CYCLE_COLLECTING_RELEASE(UndoManager)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(UndoManager)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

UndoManager::UndoManager(nsIContent* aNode)
  : mHostNode(aNode), mInTransaction(false), mIsDisconnected(false)
{
  SetIsDOMBinding();
  mTxnManager = new nsTransactionManager();
}

UndoManager::~UndoManager() {}

void
UndoManager::Transact(JSContext* aCx, nsIUndoManagerTransaction& aTransaction,
                      bool aMerge, ErrorResult& aRv)
{
  if (mIsDisconnected || mInTransaction) {
    aRv.Throw(NS_ERROR_DOM_INVALID_ACCESS_ERR);
    return;
  }

  TxnScopeGuard guard(this);

  // First try executing an automatic transaction.
  AutomaticTransact(&aTransaction, aRv);

  if (aRv.ErrorCode() == NS_ERROR_XPC_JSOBJECT_HAS_NO_FUNCTION_NAMED) {
    // If the automatic transaction didn't work due to the function being
    // undefined, then try a manual transaction.
    aRv = NS_OK;
    ManualTransact(&aTransaction, aRv);
  }

  if (aRv.Failed()) {
    return;
  }

  if (aMerge) {
    nsresult rv = mTxnManager->BatchTopUndo();
    if (NS_FAILED(rv)) {
      aRv.Throw(rv);
      return;
    }
  }

  DispatchTransactionEvent(aCx, NS_LITERAL_STRING("DOMTransaction"), 0, aRv);
  if (aRv.Failed()) {
    return;
  }
}

void
UndoManager::AutomaticTransact(nsIUndoManagerTransaction* aTransaction,
                               ErrorResult& aRv)
{
  nsCOMPtr<nsIMutationObserver> mutationObserver =
    new UndoMutationObserver(mTxnManager);

  // Transaction to call the "undo" method after the automatic transaction
  // has been undone.
  nsRefPtr<FunctionCallTxn> undoTxn = new FunctionCallTxn(aTransaction,
      FunctionCallTxn::CALL_ON_UNDO);

  // Transaction to call the "redo" method after the automatic transaction
  // has been redone.
  nsRefPtr<FunctionCallTxn> redoTxn = new FunctionCallTxn(aTransaction,
      FunctionCallTxn::CALL_ON_REDO);

  mTxnManager->BeginBatch(aTransaction);
  mTxnManager->DoTransaction(undoTxn);
  mHostNode->AddMutationObserver(mutationObserver);

  nsresult rv = aTransaction->ExecuteAutomatic();

  mHostNode->RemoveMutationObserver(mutationObserver);
  mTxnManager->DoTransaction(redoTxn);
  mTxnManager->EndBatch(true);

  if (NS_FAILED(rv)) {
    mTxnManager->RemoveTopUndo();
    aRv.Throw(rv);
    return;
  }
}

void
UndoManager::ManualTransact(nsIUndoManagerTransaction* aTransaction,
                            ErrorResult& aRv)
{
  nsRefPtr<FunctionCallTxn> txn = new FunctionCallTxn(aTransaction,
      FunctionCallTxn::CALL_ON_REDO | FunctionCallTxn::CALL_ON_UNDO);

  nsresult rv = aTransaction->Execute();
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return;
  }

  mTxnManager->BeginBatch(aTransaction);
  mTxnManager->DoTransaction(txn);
  mTxnManager->EndBatch(true);
}

uint32_t
UndoManager::GetPosition(ErrorResult& aRv)
{
  int32_t numRedo;
  nsresult rv = mTxnManager->GetNumberOfRedoItems(&numRedo);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return 0;
  }

  MOZ_ASSERT(numRedo >= 0, "Number of redo items should not be negative");
  return numRedo;
}

uint32_t
UndoManager::GetLength(ErrorResult& aRv)
{
  int32_t numRedo;
  nsresult rv = mTxnManager->GetNumberOfRedoItems(&numRedo);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return 0;
  }

  int32_t numUndo;
  rv = mTxnManager->GetNumberOfUndoItems(&numUndo);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return 0;
  }

  return numRedo + numUndo;
}

void
UndoManager::ItemInternal(uint32_t aIndex,
                          nsTArray<nsIUndoManagerTransaction*>& aItems,
                          ErrorResult& aRv)
{
  int32_t numRedo;
  nsresult rv = mTxnManager->GetNumberOfRedoItems(&numRedo);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return;
  }
  MOZ_ASSERT(numRedo >= 0, "Number of redo items should not be negative");

  int32_t numUndo;
  rv = mTxnManager->GetNumberOfUndoItems(&numUndo);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return;
  }
  MOZ_ASSERT(numUndo >= 0, "Number of undo items should not be negative");

  MOZ_ASSERT(aIndex < (uint32_t) numRedo + numUndo,
             "Index should be within bounds.");

  nsCOMPtr<nsITransactionList> txnList;
  int32_t listIndex = aIndex;
  if (aIndex < (uint32_t) numRedo) {
    // Index is an redo.
    mTxnManager->GetRedoList(getter_AddRefs(txnList));
  } else {
    // Index is a undo.
    mTxnManager->GetUndoList(getter_AddRefs(txnList));
    // We need to adjust the index because the undo list indices will
    // be in the reverse order.
    listIndex = numRedo + numUndo - aIndex - 1;
  }

  // Obtain data from transaction list and convert to list of
  // nsIUndoManagerTransaction.
  nsISupports** listData;
  uint32_t listDataLength;
  rv = txnList->GetData(listIndex, &listDataLength, &listData);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return;
  }

  for (uint32_t i = 0; i < listDataLength; i++) {
    nsCOMPtr<nsIUndoManagerTransaction> transaction =
        do_QueryInterface(listData[i]);
    MOZ_ASSERT(transaction,
               "Only nsIUndoManagerTransaction should be stored as data.");
    aItems.AppendElement(transaction);
    NS_RELEASE(listData[i]);
  }
  NS_Free(listData);
}

void
UndoManager::Item(uint32_t aIndex,
                  Nullable<nsTArray<nsRefPtr<nsIUndoManagerTransaction>>>& aItems,
                  ErrorResult& aRv)
{
  int32_t numRedo;
  nsresult rv = mTxnManager->GetNumberOfRedoItems(&numRedo);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return;
  }
  MOZ_ASSERT(numRedo >= 0, "Number of redo items should not be negative");

  int32_t numUndo;
  rv = mTxnManager->GetNumberOfUndoItems(&numUndo);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return;
  }
  MOZ_ASSERT(numUndo >= 0, "Number of undo items should not be negative");

  if (aIndex >= (uint32_t) numRedo + numUndo) {
    // If the index is out of bounds, then return null.
    aItems.SetNull();
    return;
  }

  nsTArray<nsIUndoManagerTransaction*> transactions;
  ItemInternal(aIndex, transactions, aRv);
  if (aRv.Failed()) {
    return;
  }

  nsTArray<nsRefPtr<nsIUndoManagerTransaction>>& items = aItems.SetValue();
  for (uint32_t i = 0; i < transactions.Length(); i++) {
    items.AppendElement(transactions[i]);
  }
}

void
UndoManager::Undo(JSContext* aCx, ErrorResult& aRv)
{
  if (mIsDisconnected || mInTransaction) {
    aRv.Throw(NS_ERROR_DOM_INVALID_ACCESS_ERR);
    return;
  }

  uint32_t position = GetPosition(aRv);
  if (aRv.Failed()) {
    return;
  }

  uint32_t length = GetLength(aRv);
  if (aRv.Failed()) {
    return;
  }

  // Stop if there are no transactions left to undo.
  if (position >= length) {
    return;
  }

  TxnScopeGuard guard(this);

  nsresult rv = mTxnManager->UndoTransaction();
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return;
  }

  DispatchTransactionEvent(aCx, NS_LITERAL_STRING("undo"), position, aRv);
  if (aRv.Failed()) {
    return;
  }
}

void
UndoManager::Redo(JSContext* aCx, ErrorResult& aRv)
{
  if (mIsDisconnected || mInTransaction) {
    aRv.Throw(NS_ERROR_DOM_INVALID_ACCESS_ERR);
    return;
  }

  uint32_t position = GetPosition(aRv);
  if (aRv.Failed()) {
    return;
  }

  // Stop if there are no transactions left to redo.
  if (position <= 0) {
    return;
  }

  TxnScopeGuard guard(this);

  nsresult rv = mTxnManager->RedoTransaction();
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return;
  }

  DispatchTransactionEvent(aCx, NS_LITERAL_STRING("redo"), position - 1, aRv);
  if (aRv.Failed()) {
    return;
  }
}

void
UndoManager::DispatchTransactionEvent(JSContext* aCx, const nsAString& aType,
                                      uint32_t aPreviousPosition,
                                      ErrorResult& aRv)
{
  nsTArray<nsIUndoManagerTransaction*> items;
  ItemInternal(aPreviousPosition, items, aRv);
  if (aRv.Failed()) {
    return;
  }

  nsIDocument* ownerDoc = mHostNode->OwnerDoc();
  if (!ownerDoc) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return;
  }

  nsCOMPtr<nsIDOMDocument> domDoc = do_QueryInterface(ownerDoc);
  if (!domDoc) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return;
  }

  nsCOMPtr<nsIDOMEvent> event;
  nsresult rv = domDoc->CreateEvent(NS_LITERAL_STRING("domtransaction"),
                                    getter_AddRefs(event));
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return;
  }

  nsCOMPtr<nsIWritableVariant> transactions = new nsVariant();

  // Unwrap the nsIUndoManagerTransactions into jsvals, then convert
  // to nsIVariant then put into a nsIVariant array. Arrays in XPIDL suck.
  JSObject* obj;
  nsCOMArray<nsIVariant> keepAlive;
  nsTArray<nsIVariant*> transactionItems;
  for (uint32_t i = 0; i < items.Length(); i++) {
    nsCOMPtr<nsIXPConnectWrappedJS> wrappedJS = do_QueryInterface(items[i]);
    MOZ_ASSERT(wrappedJS, "All transactions should be WrappedJS.");
    wrappedJS->GetJSObject(&obj);
    jsval txVal = JS::ObjectValue(*obj);
    if (!JS_WrapValue(aCx, &txVal)) {
      aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
      return;
    }
    nsCOMPtr<nsIVariant> txVariant;
    rv = nsContentUtils::XPConnect()->JSToVariant(aCx, txVal,
                                                  getter_AddRefs(txVariant));
    if (NS_SUCCEEDED(rv)) {
      keepAlive.AppendObject(txVariant);
      transactionItems.AppendElement(txVariant.get());
    }
  }

  transactions->SetAsArray(nsIDataType::VTYPE_INTERFACE_IS,
                           &NS_GET_IID(nsIVariant),
                           transactionItems.Length(),
                           transactionItems.Elements());

  nsCOMPtr<nsIDOMDOMTransactionEvent> ptEvent = do_QueryInterface(event);
  if (ptEvent &&
      NS_SUCCEEDED(ptEvent->InitDOMTransactionEvent(aType, true, false,
                                                    transactions))) {
    event->SetTrusted(true);
    event->SetTarget(mHostNode);
    nsEventDispatcher::DispatchDOMEvent(mHostNode, nullptr, event,
                                        nullptr, nullptr);
  }
}

void
UndoManager::ClearUndo(ErrorResult& aRv)
{
  if (mIsDisconnected || mInTransaction) {
    aRv.Throw(NS_ERROR_DOM_INVALID_ACCESS_ERR);
    return;
  }

  nsresult rv = mTxnManager->ClearUndoStack();
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return;
  }
}

void
UndoManager::ClearRedo(ErrorResult& aRv)
{
  if (mIsDisconnected || mInTransaction) {
    aRv.Throw(NS_ERROR_DOM_INVALID_ACCESS_ERR);
    return;
  }

  nsresult rv = mTxnManager->ClearRedoStack();
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return;
  }
}

nsITransactionManager*
UndoManager::GetTransactionManager()
{
  return mTxnManager;
}

void
UndoManager::Disconnect()
{
  mIsDisconnected = true;
}

bool
UndoManager::PrefEnabled()
{
  static bool sPrefValue = Preferences::GetBool("dom.undo_manager.enabled", false);
  return sPrefValue;
}

