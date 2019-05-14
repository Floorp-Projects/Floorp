/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_BrowsingContextGroup_h
#define mozilla_dom_BrowsingContextGroup_h

#include "mozilla/dom/BrowsingContext.h"
#include "mozilla/dom/ContentParent.h"
#include "nsHashKeys.h"
#include "nsTArray.h"
#include "nsTHashtable.h"
#include "nsWrapperCache.h"

namespace mozilla {
namespace dom {

class BrowsingContext;

// A BrowsingContextGroup represents the Unit of Related Browsing Contexts in
// the standard. This object currently serves roughly the same purpose as the
// TabGroup class which already exists, and at some point will likely merge with
// it.
//
// A BrowsingContext may not hold references to other BrowsingContext objects
// which are not in the same BrowsingContextGroup.
class BrowsingContextGroup final : public nsWrapperCache {
 public:
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(BrowsingContextGroup)
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(BrowsingContextGroup)

  typedef nsTHashtable<nsRefPtrHashKey<ContentParent>> ContentParents;

  // Interact with the list of BrowsingContexts.
  bool Contains(BrowsingContext* aContext);
  void Register(BrowsingContext* aContext);
  void Unregister(BrowsingContext* aContext);

  // Interact with the list of ContentParents
  void Subscribe(ContentParent* aOriginProcess);
  void Unsubscribe(ContentParent* aOriginProcess);

  // Force the given ContentParent to subscribe to our BrowsingContextGroup.
  void EnsureSubscribed(ContentParent* aProcess);

  // Methods interacting with cached contexts.
  bool IsContextCached(BrowsingContext* aContext) const;
  void CacheContext(BrowsingContext* aContext);
  void CacheContexts(const BrowsingContext::Children& aContexts);
  bool EvictCachedContext(BrowsingContext* aContext);

  // Get a reference to the list of toplevel contexts in this
  // BrowsingContextGroup.
  BrowsingContext::Children& Toplevels() { return mToplevels; }
  void GetToplevels(BrowsingContext::Children& aToplevels) {
    aToplevels.AppendElements(mToplevels);
  }

  nsISupports* GetParentObject() const;
  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  BrowsingContextGroup();

  static already_AddRefed<BrowsingContextGroup> Select(
      BrowsingContext* aParent, BrowsingContext* aOpener) {
    if (aParent) {
      return do_AddRef(aParent->Group());
    }
    if (aOpener) {
      return do_AddRef(aOpener->Group());
    }
    return MakeAndAddRef<BrowsingContextGroup>();
  }

  static already_AddRefed<BrowsingContextGroup> Select(uint64_t aParentId,
                                                       uint64_t aOpenerId) {
    RefPtr<BrowsingContext> parent = BrowsingContext::Get(aParentId);
    MOZ_RELEASE_ASSERT(parent || aParentId == 0);

    RefPtr<BrowsingContext> opener = BrowsingContext::Get(aOpenerId);
    MOZ_RELEASE_ASSERT(opener || aOpenerId == 0);

    return Select(parent, opener);
  }

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

 private:
  friend class CanonicalBrowsingContext;

  ~BrowsingContextGroup();

  // A BrowsingContextGroup contains a series of BrowsingContext objects. They
  // are addressed using a hashtable to avoid linear lookup when adding or
  // removing elements from the set.
  nsTHashtable<nsRefPtrHashKey<BrowsingContext>> mContexts;

  // The set of toplevel browsing contexts in the current BrowsingContextGroup.
  BrowsingContext::Children mToplevels;

  ContentParents mSubscribers;

  // Map of cached contexts that need to stay alive due to bfcache.
  nsTHashtable<nsRefPtrHashKey<BrowsingContext>> mCachedContexts;
};

}  // namespace dom
}  // namespace mozilla

#endif  // !defined(mozilla_dom_BrowsingContextGroup_h)
