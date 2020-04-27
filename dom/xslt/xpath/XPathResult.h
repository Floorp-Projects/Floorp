/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_XPathResult_h
#define mozilla_dom_XPathResult_h

#include "nsStubMutationObserver.h"
#include "nsCOMPtr.h"
#include "nsCOMArray.h"
#include "nsIWeakReferenceUtils.h"
#include "nsCycleCollectionParticipant.h"
#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "nsString.h"
#include "nsWrapperCache.h"
#include "nsINode.h"

class txAExprResult;

// {662f2c9a-c7cd-4cab-9349-e733df5a838c}
#define NS_IXPATHRESULT_IID                          \
  {                                                  \
    0x662f2c9a, 0xc7cd, 0x4cab, {                    \
      0x93, 0x49, 0xe7, 0x33, 0xdf, 0x5a, 0x83, 0x8c \
    }                                                \
  }

class nsIXPathResult : public nsISupports {
 public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IXPATHRESULT_IID)
  virtual void SetExprResult(txAExprResult* aExprResult, uint16_t aResultType,
                             nsINode* aContextNode,
                             mozilla::ErrorResult& aRv) = 0;
  virtual nsresult GetExprResult(txAExprResult** aExprResult) = 0;
  virtual nsresult Clone(nsIXPathResult** aResult) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIXPathResult, NS_IXPATHRESULT_IID)

namespace mozilla {
namespace dom {

/**
 * A class for evaluating an XPath expression string
 */
class XPathResult final : public nsIXPathResult,
                          public nsStubMutationObserver,
                          public nsWrapperCache {
  ~XPathResult();

 public:
  explicit XPathResult(nsINode* aParent);
  XPathResult(const XPathResult& aResult);

  enum {
    ANY_TYPE = 0U,
    NUMBER_TYPE = 1U,
    STRING_TYPE = 2U,
    BOOLEAN_TYPE = 3U,
    UNORDERED_NODE_ITERATOR_TYPE = 4U,
    ORDERED_NODE_ITERATOR_TYPE = 5U,
    UNORDERED_NODE_SNAPSHOT_TYPE = 6U,
    ORDERED_NODE_SNAPSHOT_TYPE = 7U,
    ANY_UNORDERED_NODE_TYPE = 8U,
    FIRST_ORDERED_NODE_TYPE = 9U
  };

  // nsISupports interface
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_AMBIGUOUS(XPathResult,
                                                         nsIXPathResult)

  static XPathResult* FromSupports(nsISupports* aSupports) {
    return static_cast<XPathResult*>(static_cast<nsIXPathResult*>(aSupports));
  }

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;
  nsINode* GetParentObject() const { return mParent; }
  uint16_t ResultType() const { return mResultType; }
  double GetNumberValue(ErrorResult& aRv) const {
    if (mResultType != NUMBER_TYPE) {
      aRv.ThrowTypeError("Result is not a number");
      return 0;
    }

    return mNumberResult;
  }
  void GetStringValue(nsAString& aStringValue, ErrorResult& aRv) const {
    if (mResultType != STRING_TYPE) {
      aRv.ThrowTypeError("Result is not a string");
      return;
    }

    aStringValue = mStringResult;
  }
  bool GetBooleanValue(ErrorResult& aRv) const {
    if (mResultType != BOOLEAN_TYPE) {
      aRv.ThrowTypeError("Result is not a boolean");
      return false;
    }

    return mBooleanResult;
  }
  nsINode* GetSingleNodeValue(ErrorResult& aRv) const {
    if (!isNode()) {
      aRv.ThrowTypeError("Result is not a node");
      return nullptr;
    }

    return mResultNodes.SafeObjectAt(0);
  }
  bool InvalidIteratorState() const {
    return isIterator() && mInvalidIteratorState;
  }
  uint32_t GetSnapshotLength(ErrorResult& aRv) const {
    if (!isSnapshot()) {
      aRv.ThrowTypeError("Result is not a snapshot");
      return 0;
    }

    return (uint32_t)mResultNodes.Count();
  }
  nsINode* IterateNext(ErrorResult& aRv);
  nsINode* SnapshotItem(uint32_t aIndex, ErrorResult& aRv) const {
    if (!isSnapshot()) {
      aRv.ThrowTypeError("Result is not a snapshot");
      return nullptr;
    }

    return mResultNodes.SafeObjectAt(aIndex);
  }

  // nsIMutationObserver interface
  NS_DECL_NSIMUTATIONOBSERVER_CHARACTERDATACHANGED
  NS_DECL_NSIMUTATIONOBSERVER_ATTRIBUTECHANGED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTAPPENDED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTINSERTED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTREMOVED
  NS_DECL_NSIMUTATIONOBSERVER_NODEWILLBEDESTROYED

  void SetExprResult(txAExprResult* aExprResult, uint16_t aResultType,
                     nsINode* aContextNode, ErrorResult& aRv) override;
  nsresult GetExprResult(txAExprResult** aExprResult) override;
  nsresult Clone(nsIXPathResult** aResult) override;
  void RemoveObserver();

 private:
  static bool isSnapshot(uint16_t aResultType) {
    return aResultType == UNORDERED_NODE_SNAPSHOT_TYPE ||
           aResultType == ORDERED_NODE_SNAPSHOT_TYPE;
  }
  static bool isIterator(uint16_t aResultType) {
    return aResultType == UNORDERED_NODE_ITERATOR_TYPE ||
           aResultType == ORDERED_NODE_ITERATOR_TYPE;
  }
  static bool isNode(uint16_t aResultType) {
    return aResultType == FIRST_ORDERED_NODE_TYPE ||
           aResultType == ANY_UNORDERED_NODE_TYPE;
  }
  bool isSnapshot() const { return isSnapshot(mResultType); }
  bool isIterator() const { return isIterator(mResultType); }
  bool isNode() const { return isNode(mResultType); }

  void Invalidate(const nsIContent* aChangeRoot);

  nsCOMPtr<nsINode> mParent;
  RefPtr<txAExprResult> mResult;
  nsCOMArray<nsINode> mResultNodes;
  RefPtr<Document> mDocument;
  nsWeakPtr mContextNode;
  uint32_t mCurrentPos;
  uint16_t mResultType;
  bool mInvalidIteratorState;
  bool mBooleanResult;
  double mNumberResult;
  nsString mStringResult;
};

}  // namespace dom
}  // namespace mozilla

#endif /* mozilla_dom_XPathResult_h */
