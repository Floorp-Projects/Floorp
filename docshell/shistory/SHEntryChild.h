/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SHEntryChild_h
#define mozilla_dom_SHEntryChild_h

#include "mozilla/dom/PSHEntryChild.h"
#include "nsContentUtils.h"
#include "nsExpirationTracker.h"
#include "nsIBFCacheEntry.h"
#include "nsISHEntry.h"
#include "nsRect.h"
#include "nsSHEntryShared.h"
#include "nsStubMutationObserver.h"

class nsDocShellEditorData;
class nsIContentViewer;
class nsILayoutHistoryState;
class nsIMutableArray;

namespace mozilla {
namespace dom {

class SHistoryChild;

/**
 * Implementation of the shared state for session history entries in the child
 * process.
 */
class SHEntryChildShared final : public nsIBFCacheEntry,
                                 public nsStubMutationObserver,
                                 public SHEntrySharedChildState {
 public:
  static void Init();

  static SHEntryChildShared* GetOrCreate(uint64_t aSharedID);
  static void Remove(uint64_t aSharedID);

  static uint64_t CreateSharedID() {
    return nsContentUtils::GenerateProcessSpecificId(++sNextSharedID);
  }
  static void EvictContentViewers(
      const nsTArray<uint64_t>& aToEvictSharedStateIDs);

  void NotifyListenersContentViewerEvicted(uint32_t aNumEvicted = 1);

  NS_DECL_ISUPPORTS
  NS_DECL_NSIBFCACHEENTRY

  // The nsIMutationObserver bits we actually care about.
  NS_DECL_NSIMUTATIONOBSERVER_CHARACTERDATACHANGED
  NS_DECL_NSIMUTATIONOBSERVER_ATTRIBUTECHANGED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTAPPENDED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTINSERTED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTREMOVED

  nsExpirationState* GetExpirationState() { return &mExpirationState; }

  uint64_t GetID() { return mID; }

 private:
  static uint64_t sNextSharedID;

  explicit SHEntryChildShared(uint64_t aID);
  ~SHEntryChildShared();

  friend class SHEntryChild;

  already_AddRefed<SHEntryChildShared> Duplicate();

  void RemoveFromExpirationTracker();
  void SyncPresentationState();
  void DropPresentationState();

  nsresult SetContentViewer(nsIContentViewer* aViewer);

  uint64_t mID;
  RefPtr<SHistoryChild> mSHistory;
};

/**
 * Session history entry actor for the child process.
 */
class SHEntryChild final : public PSHEntryChild, public nsISHEntry {
  friend class PSHEntryChild;

 public:
  explicit SHEntryChild(const SHEntryChild* aClone)
      : mShared(aClone->mShared.get()), mIPCActorDeleted(false) {}
  explicit SHEntryChild(uint64_t aSharedID)
      : mShared(SHEntryChildShared::GetOrCreate(aSharedID)),
        mIPCActorDeleted(false) {}

  NS_DECL_ISUPPORTS
  NS_DECL_NSISHENTRY

  void EvictContentViewer();

  static already_AddRefed<SHEntryChild> GetOrCreate(MaybeNewPSHEntry& aEntry);

 protected:
  void ActorDestroy(ActorDestroyReason aWhy) override {
    mIPCActorDeleted = true;
  }

 private:
  ~SHEntryChild() = default;

  RefPtr<SHEntryChildShared> mShared;
  bool mIPCActorDeleted;
};

}  // namespace dom
}  // namespace mozilla

#endif /* mozilla_dom_SHEntryChild_h */
