/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * nsStubMutationObserver is an implementation of the nsIMutationObserver
 * interface (except for the methods on nsISupports) that is intended to be
 * used as a base class within the content/layout library.  All methods do
 * nothing.
 */

#include "nsStubMutationObserver.h"
#include "mozilla/RefCountType.h"
#include "nsISupports.h"
#include "nsINode.h"

/******************************************************************************
 * nsStubMutationObserver
 *****************************************************************************/

NS_IMPL_NSIMUTATIONOBSERVER_CORE_STUB(nsStubMutationObserver)
NS_IMPL_NSIMUTATIONOBSERVER_CONTENT(nsStubMutationObserver)

/******************************************************************************
 * MutationObserverWrapper
 *****************************************************************************/

/**
 * @brief Wrapper class for a mutation observer that observes multiple nodes.
 *
 * This wrapper implements all methods of the nsIMutationObserver interface
 * and forwards all calls to its owner, which is an instance of
 * nsMultiMutationObserver.
 *
 * This class holds a reference to the owner and AddRefs/Releases its owner
 * as well to ensure lifetime.
 */
class MutationObserverWrapper final : public nsIMutationObserver {
 public:
  NS_DECL_ISUPPORTS

  explicit MutationObserverWrapper(nsMultiMutationObserver* aOwner)
      : mOwner(aOwner) {}

  void CharacterDataWillChange(nsIContent* aContent,
                               const CharacterDataChangeInfo& aInfo) override {
    MOZ_ASSERT(mOwner);
    mOwner->CharacterDataWillChange(aContent, aInfo);
  }

  void CharacterDataChanged(nsIContent* aContent,
                            const CharacterDataChangeInfo& aInfo) override {
    MOZ_ASSERT(mOwner);
    mOwner->CharacterDataChanged(aContent, aInfo);
  }

  void AttributeWillChange(mozilla::dom::Element* aElement,
                           int32_t aNameSpaceID, nsAtom* aAttribute,
                           int32_t aModType) override {
    MOZ_ASSERT(mOwner);
    mOwner->AttributeWillChange(aElement, aNameSpaceID, aAttribute, aModType);
  }

  void AttributeChanged(mozilla::dom::Element* aElement, int32_t aNameSpaceID,
                        nsAtom* aAttribute, int32_t aModType,
                        const nsAttrValue* aOldValue) override {
    MOZ_ASSERT(mOwner);
    mOwner->AttributeChanged(aElement, aNameSpaceID, aAttribute, aModType,
                             aOldValue);
  }

  void NativeAnonymousChildListChange(nsIContent* aContent,
                                      bool aIsRemove) override {
    MOZ_ASSERT(mOwner);
    mOwner->NativeAnonymousChildListChange(aContent, aIsRemove);
  }

  void AttributeSetToCurrentValue(mozilla::dom::Element* aElement,
                                  int32_t aNameSpaceID,
                                  nsAtom* aAttribute) override {
    MOZ_ASSERT(mOwner);
    mOwner->AttributeSetToCurrentValue(aElement, aNameSpaceID, aAttribute);
  }

  void ContentAppended(nsIContent* aFirstNewContent) override {
    MOZ_ASSERT(mOwner);
    mOwner->ContentAppended(aFirstNewContent);
  }

  void ContentInserted(nsIContent* aChild) override {
    MOZ_ASSERT(mOwner);
    mOwner->ContentInserted(aChild);
  }

  void ContentRemoved(nsIContent* aChild,
                      nsIContent* aPreviousSibling) override {
    MOZ_ASSERT(mOwner);
    mOwner->ContentRemoved(aChild, aPreviousSibling);
  }

  void NodeWillBeDestroyed(const nsINode* aNode) override {
    MOZ_ASSERT(mOwner);
    RefPtr<nsMultiMutationObserver> owner = mOwner;
    owner->NodeWillBeDestroyed(aNode);
    owner->mWrapperForNode.Remove(const_cast<nsINode*>(aNode));
    mOwner = nullptr;
    ReleaseWrapper();
  }

  void ParentChainChanged(nsIContent* aContent) override {
    MOZ_ASSERT(mOwner);
    mOwner->ParentChainChanged(aContent);
  }

  MozExternalRefCountType AddRefWrapper() {
    NS_LOG_ADDREF(this, mRefCnt, "MutationObserverWrapper", sizeof(*this));
    return ++mRefCnt;
  }

  MozExternalRefCountType ReleaseWrapper() {
    --mRefCnt;
    NS_LOG_RELEASE(this, mRefCnt, "MutationObserverWrapper");
    if (mRefCnt == 0) {
      mRefCnt = 1;
      auto refCnt = MozExternalRefCountType(mRefCnt);
      delete this;
      return refCnt;
    }
    return mRefCnt;
  }

 private:
  ~MutationObserverWrapper() = default;
  nsMultiMutationObserver* mOwner{nullptr};
};

NS_IMPL_QUERY_INTERFACE(MutationObserverWrapper, nsIMutationObserver);

MozExternalRefCountType MutationObserverWrapper::AddRef() {
  MOZ_ASSERT(mOwner);
  AddRefWrapper();
  mOwner->AddRef();
  return mRefCnt;
}

MozExternalRefCountType MutationObserverWrapper::Release() {
  MOZ_ASSERT(mOwner);
  mOwner->Release();
  return ReleaseWrapper();
}

/******************************************************************************
 * nsMultiMutationObserver
 *****************************************************************************/

void nsMultiMutationObserver::AddMutationObserverToNode(nsINode* aNode) {
  if (!aNode) {
    return;
  }
  if (mWrapperForNode.Contains(aNode)) {
    return;
  }
  auto* newWrapper = new MutationObserverWrapper{this};
  newWrapper->AddRefWrapper();
  mWrapperForNode.InsertOrUpdate(aNode, newWrapper);
  aNode->AddMutationObserver(newWrapper);
}

void nsMultiMutationObserver::RemoveMutationObserverFromNode(nsINode* aNode) {
  if (!aNode) {
    return;
  }

  if (auto obs = mWrapperForNode.MaybeGet(aNode); obs.isSome()) {
    aNode->RemoveMutationObserver(*obs);
    mWrapperForNode.Remove(aNode);
    (*obs)->ReleaseWrapper();
  }
}

bool nsMultiMutationObserver::ContainsNode(const nsINode* aNode) const {
  return mWrapperForNode.Contains(const_cast<nsINode*>(aNode));
}

/******************************************************************************
 * nsStubMultiMutationObserver
 *****************************************************************************/

NS_IMPL_NSIMUTATIONOBSERVER_CORE_STUB(nsStubMultiMutationObserver)
NS_IMPL_NSIMUTATIONOBSERVER_CONTENT(nsStubMultiMutationObserver)
