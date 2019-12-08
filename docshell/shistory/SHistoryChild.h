/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SHistoryChild_h
#define mozilla_dom_SHistoryChild_h

#include "mozilla/dom/PSHistoryChild.h"
#include "nsExpirationTracker.h"
#include "nsISHistory.h"
#include "nsWeakReference.h"

class nsIDocShell;

namespace mozilla {
namespace dom {

class LoadSHEntryData;
class SHEntryChildShared;

/**
 * Session history actor for the child process.
 */
class SHistoryChild final : public PSHistoryChild,
                            public nsISHistory,
                            public nsSupportsWeakReference {
  friend class PSHistoryChild;

 public:
  explicit SHistoryChild(BrowsingContext* aBrowsingContext);

  NS_DECL_ISUPPORTS
  NS_DECL_NSISHISTORY

 protected:
  void ActorDestroy(ActorDestroyReason aWhy) override {
    mIPCActorDeleted = true;
  }

 private:
  bool RecvReloadCurrentEntryFromChild() {
    ReloadCurrentEntry();
    return true;
  }

  // The timer based history tracker is used to evict bfcache on expiration.
  class HistoryTracker final
      : public nsExpirationTracker<SHEntryChildShared, 3> {
   public:
    explicit HistoryTracker(SHistoryChild* aSHistory, uint32_t aTimeout,
                            nsIEventTarget* aEventTarget)
        : nsExpirationTracker(1000 * aTimeout / 2, "HistoryTracker",
                              aEventTarget),
          mSHistory(aSHistory) {
      MOZ_ASSERT(aSHistory);
      mSHistory = aSHistory;
    }

   protected:
    void NotifyExpired(SHEntryChildShared* aObj) override;

   private:
    // HistoryTracker is owned by SHistoryChild; it always outlives
    // HistoryTracker so it's safe to use raw pointer here.
    SHistoryChild* mSHistory;
  };

  ~SHistoryChild() = default;

  nsresult LoadURI(LoadSHEntryData& aLoadData);

  // Track all bfcache entries and evict on expiration.
  mozilla::UniquePtr<HistoryTracker> mHistoryTracker;

  // Session History listeners
  nsAutoTObserverArray<nsWeakPtr, 2> mListeners;

  WeakPtr<nsDocShell> mRootDocShell;

  bool mIPCActorDeleted;
};

}  // namespace dom
}  // namespace mozilla

#endif /* mozilla_dom_SHistoryChild_h */
