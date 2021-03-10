/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_localstorage_LSSnapshot_h
#define mozilla_dom_localstorage_LSSnapshot_h

#include <cstdint>
#include <cstdlib>
#include "ErrorList.h"
#include "mozilla/Assertions.h"
#include "mozilla/RefPtr.h"
#include "mozilla/UniquePtr.h"
#include "nsCOMPtr.h"
#include "nsTHashMap.h"
#include "nsHashKeys.h"
#include "nsIRunnable.h"
#include "nsISupports.h"
#include "nsStringFwd.h"
#include "nsTArrayForwardDeclare.h"
#include "nsTHashtable.h"

class nsITimer;

namespace mozilla {
namespace dom {

class LSDatabase;
class LSNotifyInfo;
class LSSnapshotChild;
class LSSnapshotInitInfo;
class LSWriteAndNotifyInfo;
class SnapshotWriteOptimizer;

template <typename>
class Optional;

class LSSnapshot final : public nsIRunnable {
 public:
  /**
   * The LoadState expresses what subset of information a snapshot has from the
   * authoritative Datastore in the parent process.  The initial snapshot is
   * populated heuristically based on the size of the keys and size of the items
   * (inclusive of the key value; item is key+value, not just value) of the
   * entire datastore relative to the configured prefill limit (via pref
   * "dom.storage.snapshot_prefill" exposed as gSnapshotPrefill in bytes).
   *
   * If there's less data than the limit, we send both keys and values and end
   * up as AllOrderedItems.  If there's enough room for all the keys but not
   * all the values, we end up as AllOrderedKeys with as many values present as
   * would fit.  If there's not enough room for all the keys, then we end up as
   * Partial with as many key-value pairs as will fit.
   *
   * The state AllUnorderedItems can only be reached by code getting items one
   * by one.
   */
  enum class LoadState {
    /**
     * Class constructed, Init(LSSnapshotInitInfo) has not been invoked yet.
     */
    Initial,
    /**
     * Some keys and their values are known.
     */
    Partial,
    /**
     * All the keys are known in order, but some values are unknown.
     */
    AllOrderedKeys,
    /**
     * All keys and their values are known, but in an arbitrary order.
     */
    AllUnorderedItems,
    /**
     * All keys and their values are known and are present in their canonical
     * order.  This is everything, and is the preferred case.  The initial
     * population will send this info when the size of all items is less than
     * the prefill threshold.
     *
     * mValues will contain all keys and values, mLoadedItems and mUnknownItems
     * are unused.
     */
    AllOrderedItems,
    EndGuard
  };

 private:
  RefPtr<LSSnapshot> mSelfRef;

  RefPtr<LSDatabase> mDatabase;

  nsCOMPtr<nsITimer> mTimer;

  LSSnapshotChild* mActor;

  nsTHashtable<nsStringHashKey> mLoadedItems;
  nsTHashtable<nsStringHashKey> mUnknownItems;
  nsTHashMap<nsStringHashKey, nsString> mValues;
  UniquePtr<SnapshotWriteOptimizer> mWriteOptimizer;
  UniquePtr<nsTArray<LSWriteAndNotifyInfo>> mWriteAndNotifyInfos;

  uint32_t mInitLength;
  uint32_t mLength;
  int64_t mExactUsage;
  int64_t mPeakUsage;

  LoadState mLoadState;

  bool mHasOtherProcessObservers;
  bool mExplicit;
  bool mHasPendingStableStateCallback;
  bool mHasPendingTimerCallback;
  bool mDirty;

#ifdef DEBUG
  bool mInitialized;
  bool mSentFinish;
#endif

 public:
  explicit LSSnapshot(LSDatabase* aDatabase);

  void AssertIsOnOwningThread() const { NS_ASSERT_OWNINGTHREAD(LSSnapshot); }

  void SetActor(LSSnapshotChild* aActor);

  void ClearActor() {
    AssertIsOnOwningThread();
    MOZ_ASSERT(mActor);

    mActor = nullptr;
  }

  bool Explicit() const { return mExplicit; }

  nsresult Init(const nsAString& aKey, const LSSnapshotInitInfo& aInitInfo,
                bool aExplicit);

  nsresult GetLength(uint32_t* aResult);

  nsresult GetKey(uint32_t aIndex, nsAString& aResult);

  nsresult GetItem(const nsAString& aKey, nsAString& aResult);

  nsresult GetKeys(nsTArray<nsString>& aKeys);

  nsresult SetItem(const nsAString& aKey, const nsAString& aValue,
                   LSNotifyInfo& aNotifyInfo);

  nsresult RemoveItem(const nsAString& aKey, LSNotifyInfo& aNotifyInfo);

  nsresult Clear(LSNotifyInfo& aNotifyInfo);

  void MarkDirty();

  nsresult End();

 private:
  ~LSSnapshot();

  void ScheduleStableStateCallback();

  void MaybeScheduleStableStateCallback();

  nsresult GetItemInternal(const nsAString& aKey,
                           const Optional<nsString>& aValue,
                           nsAString& aResult);

  nsresult EnsureAllKeys();

  nsresult UpdateUsage(int64_t aDelta);

  nsresult Checkpoint();

  nsresult Finish();

  void CancelTimer();

  static void TimerCallback(nsITimer* aTimer, void* aClosure);

  NS_DECL_ISUPPORTS
  NS_DECL_NSIRUNNABLE
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_localstorage_LSSnapshot_h
