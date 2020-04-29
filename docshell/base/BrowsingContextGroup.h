/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_BrowsingContextGroup_h
#define mozilla_dom_BrowsingContextGroup_h

#include "mozilla/dom/BrowsingContext.h"
#include "nsRefPtrHashtable.h"
#include "nsHashKeys.h"
#include "nsTArray.h"
#include "nsTHashtable.h"
#include "nsWrapperCache.h"
#include "nsXULAppAPI.h"

namespace mozilla {
class ThrottledEventQueue;

namespace dom {

class BrowsingContext;
class WindowContext;
class ContentParent;

// A BrowsingContextGroup represents the Unit of Related Browsing Contexts in
// the standard.
//
// A BrowsingContext may not hold references to other BrowsingContext objects
// which are not in the same BrowsingContextGroup.
class BrowsingContextGroup final : public nsWrapperCache {
 public:
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(BrowsingContextGroup)
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(BrowsingContextGroup)

  typedef nsTHashtable<nsRefPtrHashKey<ContentParent>> ContentParents;

  // Interact with the list of synced contexts. This controls the lifecycle of
  // the BrowsingContextGroup and contexts loaded within them.
  void Register(nsISupports* aContext);
  void Unregister(nsISupports* aContext);

  // Interact with the list of ContentParents
  void Subscribe(ContentParent* aOriginProcess);
  void Unsubscribe(ContentParent* aOriginProcess);

  // Force the given ContentParent to subscribe to our BrowsingContextGroup.
  void EnsureSubscribed(ContentParent* aProcess);

  // Get a reference to the list of toplevel contexts in this
  // BrowsingContextGroup.
  nsTArray<RefPtr<BrowsingContext>>& Toplevels() { return mToplevels; }
  void GetToplevels(nsTArray<RefPtr<BrowsingContext>>& aToplevels) {
    aToplevels.AppendElements(mToplevels);
  }

  uint64_t Id() { return mId; }

  nsISupports* GetParentObject() const;
  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  // Get or create a BrowsingContextGroup with the given ID.
  static already_AddRefed<BrowsingContextGroup> GetOrCreate(uint64_t aId);
  static already_AddRefed<BrowsingContextGroup> Create();
  static already_AddRefed<BrowsingContextGroup> Select(
      WindowContext* aParent, BrowsingContext* aOpener);

  // For each 'ContentParent', except for 'aExcludedParent',
  // associated with this group call 'aCallback'.
  template <typename Func>
  void EachOtherParent(ContentParent* aExcludedParent, Func&& aCallback) {
    MOZ_DIAGNOSTIC_ASSERT(XRE_IsParentProcess());
    for (auto iter = mSubscribers.Iter(); !iter.Done(); iter.Next()) {
      if (iter.Get()->GetKey() != aExcludedParent) {
        aCallback(iter.Get()->GetKey());
      }
    }
  }

  // For each 'ContentParent' associated with
  // this group call 'aCallback'.
  template <typename Func>
  void EachParent(Func&& aCallback) {
    MOZ_DIAGNOSTIC_ASSERT(XRE_IsParentProcess());
    for (auto iter = mSubscribers.Iter(); !iter.Done(); iter.Next()) {
      aCallback(iter.Get()->GetKey());
    }
  }

  nsresult QueuePostMessageEvent(already_AddRefed<nsIRunnable>&& aRunnable);

  void FlushPostMessageEvents();

  static BrowsingContextGroup* GetChromeGroup();

  void GetDocGroups(nsTArray<DocGroup*>& aDocGroups);

  // Called by Document when a Document needs to be added to a DocGroup.
  already_AddRefed<DocGroup> AddDocument(const nsACString& aKey,
                                         Document* aDocument);

  // Called by Document when a Document needs to be removed to a DocGroup.
  void RemoveDocument(const nsACString& aKey, Document* aDocument);

  auto DocGroups() const { return mDocGroups.ConstIter(); }

  mozilla::ThrottledEventQueue* GetTimerEventQueue() const {
    return mTimerEventQueue;
  }

  mozilla::ThrottledEventQueue* GetWorkerEventQueue() const {
    return mWorkerEventQueue;
  }

  static void GetAllGroups(nsTArray<RefPtr<BrowsingContextGroup>>& aGroups);

 private:
  friend class CanonicalBrowsingContext;

  explicit BrowsingContextGroup(uint64_t aId);
  ~BrowsingContextGroup();

  void UnsubscribeAllContentParents();

  uint64_t mId;

  // A BrowsingContextGroup contains a series of {Browsing,Window}Context
  // objects. They are addressed using a hashtable to avoid linear lookup when
  // adding or removing elements from the set.
  //
  // FIXME: This list is only required over a counter to keep nested
  // non-discarded contexts within discarded contexts alive. It should be
  // removed in the future.
  // FIXME: Consider introducing a better common base than `nsISupports`?
  nsTHashtable<nsRefPtrHashKey<nsISupports>> mContexts;

  // The set of toplevel browsing contexts in the current BrowsingContextGroup.
  nsTArray<RefPtr<BrowsingContext>> mToplevels;

  // DocGroups are thread-safe, and not able to be cycle collected,
  // but we still keep strong pointers. When all Documents are removed
  // from DocGroup (by the BrowsingContextGroup), the DocGroup is
  // removed from here.
  nsRefPtrHashtable<nsCStringHashKey, DocGroup> mDocGroups;

  ContentParents mSubscribers;

  // A queue to store postMessage events during page load, the queue will be
  // flushed once the page is loaded
  RefPtr<mozilla::ThrottledEventQueue> mPostMessageEventQueue;

  RefPtr<mozilla::ThrottledEventQueue> mTimerEventQueue;
  RefPtr<mozilla::ThrottledEventQueue> mWorkerEventQueue;
};

}  // namespace dom
}  // namespace mozilla

#endif  // !defined(mozilla_dom_BrowsingContextGroup_h)
