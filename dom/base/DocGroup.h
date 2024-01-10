/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DocGroup_h
#define DocGroup_h

#include "nsISupportsImpl.h"
#include "nsIPrincipal.h"
#include "nsThreadUtils.h"
#include "nsTHashSet.h"
#include "nsString.h"
#include "mozilla/RefPtr.h"
#include "mozilla/dom/BrowsingContextGroup.h"
#include "mozilla/dom/HTMLSlotElement.h"

namespace mozilla {
class AbstractThread;
namespace dom {

class CustomElementReactionsStack;
class JSExecutionManager;

// Two browsing contexts are considered "related" if they are reachable from one
// another through window.opener, window.parent, or window.frames. This is the
// spec concept of a browsing context group.
//
// Two browsing contexts are considered "similar-origin" if they can be made to
// have the same origin by setting document.domain. This is the spec concept of
// a "unit of similar-origin related browsing contexts"
//
// A BrowsingContextGroup is a set of browsing contexts which are all
// "related".  Within a BrowsingContextGroup, browsing contexts are
// broken into "similar-origin" DocGroups.  A DocGroup is a member
// of exactly one BrowsingContextGroup.
class DocGroup final {
 public:
  typedef nsTArray<Document*>::iterator Iterator;

  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(DocGroup)
  NS_DECL_CYCLE_COLLECTION_NATIVE_CLASS(DocGroup)

  static already_AddRefed<DocGroup> Create(
      BrowsingContextGroup* aBrowsingContextGroup, const nsACString& aKey);

  // Returns NS_ERROR_FAILURE and sets |aString| to an empty string if the TLD
  // service isn't available. Returns NS_OK on success, but may still set
  // |aString| may still be set to an empty string.
  [[nodiscard]] static nsresult GetKey(nsIPrincipal* aPrincipal,
                                       bool aCrossOriginIsolated,
                                       nsACString& aKey);

  bool MatchesKey(const nsACString& aKey) { return aKey == mKey; }

  const nsACString& GetKey() const { return mKey; }

  JSExecutionManager* GetExecutionManager() const { return mExecutionManager; }
  void SetExecutionManager(JSExecutionManager*);

  BrowsingContextGroup* GetBrowsingContextGroup() const {
    return mBrowsingContextGroup;
  }

  mozilla::dom::DOMArena* ArenaAllocator() { return mArena; }

  mozilla::dom::CustomElementReactionsStack* CustomElementReactionsStack();

  // Adding documents to a DocGroup should be done through
  // BrowsingContextGroup::AddDocument (which in turn calls
  // DocGroup::AddDocument).
  void AddDocument(Document* aDocument);

  // Removing documents from a DocGroup should be done through
  // BrowsingContextGroup::RemoveDocument(which in turn calls
  // DocGroup::RemoveDocument).
  void RemoveDocument(Document* aDocument);

  // Iterators for iterating over every document within the DocGroup
  Iterator begin() {
    MOZ_ASSERT(NS_IsMainThread());
    return mDocuments.begin();
  }
  Iterator end() {
    MOZ_ASSERT(NS_IsMainThread());
    return mDocuments.end();
  }

  // Return a pointer that can be continually checked to see if access to this
  // DocGroup is valid. This pointer should live at least as long as the
  // DocGroup.
  bool* GetValidAccessPtr();

  // Append aSlot to the list of signal slot list, and queue a mutation observer
  // microtask.
  void SignalSlotChange(HTMLSlotElement& aSlot);

  nsTArray<RefPtr<HTMLSlotElement>> MoveSignalSlotList();

  // List of DocGroups that has non-empty signal slot list.
  static AutoTArray<RefPtr<DocGroup>, 2>* sPendingDocGroups;

  // Returns true if any of its documents are active but not in the bfcache.
  bool IsActive() const;

  const nsID& AgentClusterId() const { return mAgentClusterId; }

  bool IsEmpty() const { return mDocuments.IsEmpty(); }

 private:
  DocGroup(BrowsingContextGroup* aBrowsingContextGroup, const nsACString& aKey);

  ~DocGroup();

  nsCString mKey;
  nsTArray<Document*> mDocuments;
  RefPtr<mozilla::dom::CustomElementReactionsStack> mReactionsStack;
  nsTArray<RefPtr<HTMLSlotElement>> mSignalSlotList;
  RefPtr<BrowsingContextGroup> mBrowsingContextGroup;

  // non-null if the JS execution for this docgroup is regulated with regards
  // to worker threads. This should only be used when we are forcing serialized
  // SAB access.
  RefPtr<JSExecutionManager> mExecutionManager;

  // Each DocGroup has a persisted agent cluster ID.
  const nsID mAgentClusterId;

  RefPtr<mozilla::dom::DOMArena> mArena;
};

}  // namespace dom
}  // namespace mozilla

#endif  // defined(DocGroup_h)
