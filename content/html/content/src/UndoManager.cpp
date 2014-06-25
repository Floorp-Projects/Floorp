/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/UndoManager.h"
#include "mozilla/dom/DOMTransactionBinding.h"

#include "mozilla/dom/Event.h"
#include "nsDOMClassInfoID.h"
#include "nsIClassInfo.h"
#include "nsIDOMDocument.h"
#include "nsIXPCScriptable.h"
#include "nsIVariant.h"
#include "nsVariant.h"
#include "nsINode.h"
#include "nsIDOMDOMTransactionEvent.h"
#include "nsContentUtils.h"
#include "jsapi.h"
#include "nsIDocument.h"

#include "mozilla/ErrorResult.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/Preferences.h"

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
  ~UndoAttrChanged() {}

  nsresult SaveRedoState();
  nsCOMPtr<nsIContent> mElement;
  int32_t mNameSpaceID;
  nsCOMPtr<nsIAtom> mAttrAtom;
  int32_t mModType;
  nsString mRedoValue;
  nsString mUndoValue;
};

NS_IMPL_CYCLE_COLLECTION(UndoAttrChanged, mElement, mAttrAtom)

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
  ~UndoTextChanged() {}

  void SaveRedoState();
  nsCOMPtr<nsIContent> mContent;
  UndoCharacterChangedData mChange;
  nsString mRedoValue;
  nsString mUndoValue;
};

NS_IMPL_CYCLE_COLLECTION(UndoTextChanged, mContent)

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
    // Text length should match the change start unless there was a
    // mutation exterior to the UndoManager in which case we do nothing.
    if (text.Length() == mChange.mChangeStart) {
      mContent->AppendText(mRedoValue.get(), mRedoValue.Length(), true);
    }
  } else {
    int32_t numReplaced = mChange.mChangeEnd - mChange.mChangeStart;
    // The length of the text should be at least as long as the replacement
    // offset + replaced length, otherwise there was an external mutation.
    if (mChange.mChangeStart + numReplaced <= text.Length()) {
      text.Replace(mChange.mChangeStart, numReplaced, mRedoValue);
      mContent->SetText(text, true);
    }
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
    // The text should at least as long as the redo value in the case
    // of an append, otherwise there was an external mutation.
    if (mRedoValue.Length() <= text.Length()) {
      text.Truncate(text.Length() - mRedoValue.Length());
    }
  } else {
    // The length of the text should be at least as long as the replacement
    // offset + replacement length, otherwise there was an external mutation.
    if (mChange.mChangeStart + mChange.mReplaceLength <= text.Length()) {
      text.Replace(mChange.mChangeStart, mChange.mReplaceLength, mUndoValue);
    }
  }
  mContent->SetText(text, true);

  return NS_OK;
}

void
UndoTextChanged::SaveRedoState()
{
  const nsTextFragment* text = mContent->GetText();
  mRedoValue.Truncate();
  // The length of the text should be at least as long as the replacement
  // offset + replacement length, otherwise there was an external mutation.
  if (mChange.mChangeStart + mChange.mReplaceLength <= text->GetLength()) {
    text->AppendTo(mRedoValue, mChange.mChangeStart, mChange.mReplaceLength);
  }
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
  ~UndoContentAppend() {}
  nsCOMPtr<nsIContent> mContent;
  nsCOMArray<nsIContent> mChildren;
};

NS_IMPL_CYCLE_COLLECTION(UndoContentAppend, mContent, mChildren)

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
    if (!mChildren[i]->GetParentNode()) {
      mContent->AppendChildTo(mChildren[i], true);
    }
  }

  return NS_OK;
}

nsresult
UndoContentAppend::UndoTransaction()
{
  for (int32_t i = mChildren.Count() - 1; i >= 0; i--) {
    if (mChildren[i]->GetParentNode() == mContent) {
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
  ~UndoContentInsert() {}
  nsCOMPtr<nsIContent> mContent;
  nsCOMPtr<nsIContent> mChild;
  nsCOMPtr<nsIContent> mNextNode;
};

NS_IMPL_CYCLE_COLLECTION(UndoContentInsert, mContent, mChild, mNextNode)

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
  if (mChild->GetParentNode()) {
    return NS_OK;
  }

  // Check to see if next sibling has same parent.
  if (mNextNode && mNextNode->GetParentNode() != mContent) {
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
  if (mChild->GetParentNode() != mContent) {
    return NS_OK;
  }

  // Check of the parent of the next node is the same.
  if (mNextNode && mNextNode->GetParentNode() != mContent) {
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
  ~UndoContentRemove() {}
  nsCOMPtr<nsIContent> mContent;
  nsCOMPtr<nsIContent> mChild;
  nsCOMPtr<nsIContent> mNextNode;
};

NS_IMPL_CYCLE_COLLECTION(UndoContentRemove, mContent, mChild, mNextNode)

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
  if (mChild->GetParentNode()) {
    return NS_OK;
  }

  // Make sure next sibling is still under same parent.
  if (mNextNode && mNextNode->GetParentNode() != mContent) {
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
  if (mChild->GetParentNode() != mContent) {
    return NS_OK;
  }

  // Check that the next node still has the same parent.
  if (mNextNode && mNextNode->GetParentNode() != mContent) {
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

NS_IMPL_ISUPPORTS(UndoMutationObserver, nsIMutationObserver)

bool
UndoMutationObserver::IsManagerForMutation(nsIContent* aContent)
{
  nsCOMPtr<nsINode> currentNode = aContent;
  nsRefPtr<UndoManager> undoManager;

  // Get the UndoManager of nearest ancestor with an UndoManager.
  while (currentNode && !undoManager) {
    nsCOMPtr<Element> htmlElem = do_QueryInterface(currentNode);
    if (htmlElem) {
      undoManager = htmlElem->GetUndoManager();
    }

    currentNode = currentNode->GetParentNode();
  }

  if (!undoManager) {
    // Check against document UndoManager if we were unable to find an
    // UndoManager in an ancestor element.
    nsIDocument* doc = aContent->OwnerDoc();
    NS_ENSURE_TRUE(doc, false);
    undoManager = doc->GetUndoManager();
    // The document will not have an undoManager if the
    // documentElement is removed.
    NS_ENSURE_TRUE(undoManager, false);
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
  FunctionCallTxn(DOMTransaction* aTransaction, uint32_t aFlags);
protected:
  ~FunctionCallTxn() {}
  /**
   * Call a function member on the transaction object with the
   * specified function name.
   */
  nsRefPtr<DOMTransaction> mTransaction;
  uint32_t mFlags;
};

NS_IMPL_CYCLE_COLLECTION(FunctionCallTxn, mTransaction)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(FunctionCallTxn)
  NS_INTERFACE_MAP_ENTRY(nsITransaction)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(FunctionCallTxn)
NS_IMPL_CYCLE_COLLECTING_RELEASE(FunctionCallTxn)

FunctionCallTxn::FunctionCallTxn(DOMTransaction* aTransaction,
                                 uint32_t aFlags)
  : mTransaction(aTransaction), mFlags(aFlags) {}

nsresult
FunctionCallTxn::RedoTransaction()
{
  if (!(mFlags & CALL_ON_REDO)) {
    return NS_OK;
  }

  ErrorResult rv;
  nsRefPtr<DOMTransactionCallback> redo = mTransaction->GetRedo(rv);
  if (!rv.Failed() && redo) {
    redo->Call(mTransaction.get(), rv);
  }
  // We ignore rv because we want to avoid the rollback behavior of the
  // nsITransactionManager.

  return NS_OK;
}

nsresult
FunctionCallTxn::UndoTransaction()
{
  if (!(mFlags & CALL_ON_UNDO)) {
    return NS_OK;
  }

  ErrorResult rv;
  nsRefPtr<DOMTransactionCallback> undo = mTransaction->GetUndo(rv);
  if (!rv.Failed() && undo) {
    undo->Call(mTransaction.get(), rv);
  }
  // We ignore rv because we want to avoid the rollback behavior of the
  // nsITransactionManager.

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

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(UndoManager, mTxnManager, mHostNode)
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
UndoManager::Transact(JSContext* aCx, DOMTransaction& aTransaction,
                      bool aMerge, ErrorResult& aRv)
{
  if (mIsDisconnected || mInTransaction) {
    aRv.Throw(NS_ERROR_DOM_INVALID_ACCESS_ERR);
    return;
  }

  TxnScopeGuard guard(this);

  // First try executing an automatic transaction.

  nsRefPtr<DOMTransactionCallback> executeAutomatic =
    aTransaction.GetExecuteAutomatic(aRv);
  if (aRv.Failed()) {
    return;
  }
  if (executeAutomatic) {
    AutomaticTransact(&aTransaction, executeAutomatic, aRv);
  } else {
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
UndoManager::AutomaticTransact(DOMTransaction* aTransaction,
                               DOMTransactionCallback* aCallback,
                               ErrorResult& aRv)
{
  MOZ_ASSERT(aCallback);

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

  aCallback->Call(aTransaction, aRv);

  mHostNode->RemoveMutationObserver(mutationObserver);
  mTxnManager->DoTransaction(redoTxn);
  mTxnManager->EndBatch(true);

  if (aRv.Failed()) {
    mTxnManager->RemoveTopUndo();
  }
}

void
UndoManager::ManualTransact(DOMTransaction* aTransaction,
                            ErrorResult& aRv)
{
  nsRefPtr<FunctionCallTxn> txn = new FunctionCallTxn(aTransaction,
      FunctionCallTxn::CALL_ON_REDO | FunctionCallTxn::CALL_ON_UNDO);

  nsRefPtr<DOMTransactionCallback> execute = aTransaction->GetExecute(aRv);
  if (!aRv.Failed() && execute) {
    execute->Call(aTransaction, aRv);
  }
  if (aRv.Failed()) {
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
                          nsTArray<DOMTransaction*>& aItems,
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
  // DOMTransaction*.
  nsISupports** listData;
  uint32_t listDataLength;
  rv = txnList->GetData(listIndex, &listDataLength, &listData);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return;
  }

  for (uint32_t i = 0; i < listDataLength; i++) {
    aItems.AppendElement(static_cast<DOMTransaction*>(listData[i]));
    NS_RELEASE(listData[i]);
  }
  NS_Free(listData);
}

void
UndoManager::Item(uint32_t aIndex,
                  Nullable<nsTArray<nsRefPtr<DOMTransaction> > >& aItems,
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

  nsTArray<DOMTransaction*> transactions;
  ItemInternal(aIndex, transactions, aRv);
  if (aRv.Failed()) {
    return;
  }

  nsTArray<nsRefPtr<DOMTransaction> >& items = aItems.SetValue();
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
  nsTArray<DOMTransaction*> items;
  ItemInternal(aPreviousPosition, items, aRv);
  if (aRv.Failed()) {
    return;
  }

  nsRefPtr<Event> event = mHostNode->OwnerDoc()->CreateEvent(
    NS_LITERAL_STRING("domtransaction"), aRv);
  if (aRv.Failed()) {
    return;
  }

  nsCOMPtr<nsIWritableVariant> transactions = new nsVariant();

  // Unwrap the DOMTransactions into jsvals, then convert
  // to nsIVariant then put into a nsIVariant array. Arrays in XPIDL suck.
  nsCOMArray<nsIVariant> keepAlive;
  nsTArray<nsIVariant*> transactionItems;
  for (uint32_t i = 0; i < items.Length(); i++) {
    JS::Rooted<JS::Value> txVal(aCx, JS::ObjectValue(*items[i]->Callback()));
    if (!JS_WrapValue(aCx, &txVal)) {
      aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
      return;
    }
    nsCOMPtr<nsIVariant> txVariant;
    nsresult rv =
      nsContentUtils::XPConnect()->JSToVariant(aCx, txVal,
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
    EventDispatcher::DispatchDOMEvent(mHostNode, nullptr, event,
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
