/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/LSSnapshot.h"

// Local includes
#include "ActorsChild.h"
#include "LSDatabase.h"
#include "LSWriteOptimizer.h"
#include "LSWriteOptimizerImpl.h"
#include "LocalStorageCommon.h"

// Global includes
#include <cstdint>
#include <cstdlib>
#include <new>
#include <type_traits>
#include <utility>
#include "ErrorList.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/MacroForEach.h"
#include "mozilla/Maybe.h"
#include "mozilla/Preferences.h"
#include "mozilla/RefPtr.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/StaticPrefs_dom.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/LSValue.h"
#include "mozilla/dom/quota/QuotaCommon.h"
#include "mozilla/dom/quota/ResultExtensions.h"
#include "mozilla/dom/quota/ScopedLogExtraInfo.h"
#include "mozilla/dom/PBackgroundLSDatabase.h"
#include "mozilla/dom/PBackgroundLSSharedTypes.h"
#include "mozilla/dom/PBackgroundLSSnapshot.h"
#include "nsBaseHashtable.h"
#include "nsCOMPtr.h"
#include "nsContentUtils.h"
#include "nsDebug.h"
#include "nsError.h"
#include "nsITimer.h"
#include "nsString.h"
#include "nsStringFlags.h"
#include "nsStringFwd.h"
#include "nsTArray.h"
#include "nsTStringRepr.h"
#include "nscore.h"

namespace mozilla::dom {

/**
 * Coalescing manipulation queue used by `LSSnapshot`.  Used by `LSSnapshot` to
 * buffer and coalesce manipulations before they are sent to the parent process,
 * when a Snapshot Checkpoints. (This can only be done when there are no
 * observers for other content processes.)
 */
class SnapshotWriteOptimizer final : public LSWriteOptimizer<LSValue> {
 public:
  void Enumerate(nsTArray<LSWriteInfo>& aWriteInfos);
};

void SnapshotWriteOptimizer::Enumerate(nsTArray<LSWriteInfo>& aWriteInfos) {
  AssertIsOnOwningThread();

  // The mWriteInfos hash table contains all write infos, but it keeps them in
  // an arbitrary order, which means write infos need to be sorted before being
  // processed.

  nsTArray<NotNull<WriteInfo*>> writeInfos;
  GetSortedWriteInfos(writeInfos);

  for (WriteInfo* writeInfo : writeInfos) {
    switch (writeInfo->GetType()) {
      case WriteInfo::InsertItem: {
        auto insertItemInfo = static_cast<InsertItemInfo*>(writeInfo);

        LSSetItemInfo setItemInfo;
        setItemInfo.key() = insertItemInfo->GetKey();
        setItemInfo.value() = insertItemInfo->GetValue();

        aWriteInfos.AppendElement(std::move(setItemInfo));

        break;
      }

      case WriteInfo::UpdateItem: {
        auto updateItemInfo = static_cast<UpdateItemInfo*>(writeInfo);

        if (updateItemInfo->UpdateWithMove()) {
          // See the comment in LSWriteOptimizer::InsertItem for more details
          // about the UpdateWithMove flag.

          LSRemoveItemInfo removeItemInfo;
          removeItemInfo.key() = updateItemInfo->GetKey();

          aWriteInfos.AppendElement(std::move(removeItemInfo));
        }

        LSSetItemInfo setItemInfo;
        setItemInfo.key() = updateItemInfo->GetKey();
        setItemInfo.value() = updateItemInfo->GetValue();

        aWriteInfos.AppendElement(std::move(setItemInfo));

        break;
      }

      case WriteInfo::DeleteItem: {
        auto deleteItemInfo = static_cast<DeleteItemInfo*>(writeInfo);

        LSRemoveItemInfo removeItemInfo;
        removeItemInfo.key() = deleteItemInfo->GetKey();

        aWriteInfos.AppendElement(std::move(removeItemInfo));

        break;
      }

      case WriteInfo::Truncate: {
        LSClearInfo clearInfo;

        aWriteInfos.AppendElement(std::move(clearInfo));

        break;
      }

      default:
        MOZ_CRASH("Bad type!");
    }
  }
}

LSSnapshot::LSSnapshot(LSDatabase* aDatabase)
    : mDatabase(aDatabase),
      mActor(nullptr),
      mInitLength(0),
      mLength(0),
      mUsage(0),
      mPeakUsage(0),
      mLoadState(LoadState::Initial),
      mHasOtherProcessDatabases(false),
      mHasOtherProcessObservers(false),
      mExplicit(false),
      mHasPendingStableStateCallback(false),
      mHasPendingIdleTimerCallback(false),
      mDirty(false)
#ifdef DEBUG
      ,
      mInitialized(false),
      mSentFinish(false)
#endif
{
  AssertIsOnOwningThread();
}

LSSnapshot::~LSSnapshot() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mDatabase);
  MOZ_ASSERT(!mHasPendingStableStateCallback);
  MOZ_ASSERT(!mHasPendingIdleTimerCallback);
  MOZ_ASSERT_IF(mInitialized, mSentFinish);

  if (mActor) {
    mActor->SendDeleteMeInternal();
    MOZ_ASSERT(!mActor, "SendDeleteMeInternal should have cleared!");
  }
}

void LSSnapshot::SetActor(LSSnapshotChild* aActor) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aActor);
  MOZ_ASSERT(!mActor);

  mActor = aActor;
}

nsresult LSSnapshot::Init(const nsAString& aKey,
                          const LSSnapshotInitInfo& aInitInfo, bool aExplicit) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(!mSelfRef);
  MOZ_ASSERT(mActor);
  MOZ_ASSERT(mLoadState == LoadState::Initial);
  MOZ_ASSERT(!mInitialized);
  MOZ_ASSERT(!mSentFinish);

  mSelfRef = this;

  LoadState loadState = aInitInfo.loadState();

  const nsTArray<LSItemInfo>& itemInfos = aInitInfo.itemInfos();
  for (uint32_t i = 0; i < itemInfos.Length(); i++) {
    const LSItemInfo& itemInfo = itemInfos[i];

    const LSValue& value = itemInfo.value();

    if (loadState != LoadState::AllOrderedItems && !value.IsVoid()) {
      mLoadedItems.Insert(itemInfo.key());
    }

    mValues.InsertOrUpdate(itemInfo.key(), value.AsString());
  }

  if (loadState == LoadState::Partial) {
    if (aInitInfo.addKeyToUnknownItems()) {
      mUnknownItems.Insert(aKey);
    }
    mInitLength = aInitInfo.totalLength();
    mLength = mInitLength;
  } else if (loadState == LoadState::AllOrderedKeys) {
    mInitLength = aInitInfo.totalLength();
  } else {
    MOZ_ASSERT(loadState == LoadState::AllOrderedItems);
  }

  mUsage = aInitInfo.usage();
  mPeakUsage = aInitInfo.peakUsage();

  mLoadState = aInitInfo.loadState();

  mHasOtherProcessDatabases = aInitInfo.hasOtherProcessDatabases();
  mHasOtherProcessObservers = aInitInfo.hasOtherProcessObservers();

  mExplicit = aExplicit;

#ifdef DEBUG
  mInitialized = true;
#endif

  if (mHasOtherProcessObservers) {
    mWriteAndNotifyInfos = MakeUnique<nsTArray<LSWriteAndNotifyInfo>>();
  } else {
    mWriteOptimizer = MakeUnique<SnapshotWriteOptimizer>();
  }

  if (!mExplicit) {
    mIdleTimer = NS_NewTimer();
    MOZ_ASSERT(mIdleTimer);

    ScheduleStableStateCallback();
  }

  return NS_OK;
}

nsresult LSSnapshot::GetLength(uint32_t* aResult) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mActor);
  MOZ_ASSERT(mInitialized);
  MOZ_ASSERT(!mSentFinish);

  MaybeScheduleStableStateCallback();

  if (mLoadState == LoadState::Partial) {
    *aResult = mLength;
  } else {
    *aResult = mValues.Count();
  }

  return NS_OK;
}

nsresult LSSnapshot::GetKey(uint32_t aIndex, nsAString& aResult) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mActor);
  MOZ_ASSERT(mInitialized);
  MOZ_ASSERT(!mSentFinish);

  MaybeScheduleStableStateCallback();

  nsresult rv = EnsureAllKeys();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  aResult.SetIsVoid(true);
  for (auto iter = mValues.ConstIter(); !iter.Done(); iter.Next()) {
    if (aIndex == 0) {
      aResult = iter.Key();
      return NS_OK;
    }
    aIndex--;
  }

  return NS_OK;
}

nsresult LSSnapshot::GetItem(const nsAString& aKey, nsAString& aResult) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mActor);
  MOZ_ASSERT(mInitialized);
  MOZ_ASSERT(!mSentFinish);

  MaybeScheduleStableStateCallback();

  nsString result;
  nsresult rv = GetItemInternal(aKey, Optional<nsString>(), result);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  aResult = result;
  return NS_OK;
}

nsresult LSSnapshot::GetKeys(nsTArray<nsString>& aKeys) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mActor);
  MOZ_ASSERT(mInitialized);
  MOZ_ASSERT(!mSentFinish);

  MaybeScheduleStableStateCallback();

  nsresult rv = EnsureAllKeys();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  AppendToArray(aKeys, mValues.Keys());

  return NS_OK;
}

nsresult LSSnapshot::SetItem(const nsAString& aKey, const nsAString& aValue,
                             LSNotifyInfo& aNotifyInfo) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mActor);
  MOZ_ASSERT(mInitialized);
  MOZ_ASSERT(!mSentFinish);

  MaybeScheduleStableStateCallback();

  nsString oldValue;
  nsresult rv =
      GetItemInternal(aKey, Optional<nsString>(nsString(aValue)), oldValue);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  bool changed;
  if (oldValue == aValue && oldValue.IsVoid() == aValue.IsVoid()) {
    changed = false;
  } else {
    changed = true;

    auto autoRevertValue = MakeScopeExit([&] {
      if (oldValue.IsVoid()) {
        mValues.Remove(aKey);
      } else {
        mValues.InsertOrUpdate(aKey, oldValue);
      }
    });

    // Anything that can fail must be done early before we start modifying the
    // state.

    Maybe<LSValue> oldValueFromString;
    if (mHasOtherProcessObservers) {
      oldValueFromString.emplace();
      if (NS_WARN_IF(!oldValueFromString->InitFromString(oldValue))) {
        return NS_ERROR_FAILURE;
      }
    }

    LSValue valueFromString;
    if (NS_WARN_IF(!valueFromString.InitFromString(aValue))) {
      return NS_ERROR_FAILURE;
    }

    int64_t delta = static_cast<int64_t>(aValue.Length()) -
                    static_cast<int64_t>(oldValue.Length());

    if (oldValue.IsVoid()) {
      delta += static_cast<int64_t>(aKey.Length());
    }

    {
      quota::ScopedLogExtraInfo scope{
          quota::ScopedLogExtraInfo::kTagContextTainted,
          "dom::localstorage::LSSnapshot::SetItem::UpdateUsage"_ns};
      QM_TRY(MOZ_TO_RESULT(UpdateUsage(delta)), QM_PROPAGATE, QM_NO_CLEANUP,
             ([]() {
               static uint32_t counter = 0u;
               const bool result = 0u != (counter & (1u + counter));
               ++counter;
               return result;
             }));
    }

    if (oldValue.IsVoid() && mLoadState == LoadState::Partial) {
      mLength++;
    }

    if (mHasOtherProcessObservers) {
      MOZ_ASSERT(mWriteAndNotifyInfos);
      MOZ_ASSERT(oldValueFromString.isSome());

      LSSetItemAndNotifyInfo setItemAndNotifyInfo;
      setItemAndNotifyInfo.key() = aKey;
      setItemAndNotifyInfo.oldValue() = oldValueFromString.value();
      setItemAndNotifyInfo.value() = valueFromString;

      mWriteAndNotifyInfos->AppendElement(std::move(setItemAndNotifyInfo));
    } else {
      MOZ_ASSERT(mWriteOptimizer);

      if (oldValue.IsVoid()) {
        mWriteOptimizer->InsertItem(aKey, valueFromString);
      } else {
        mWriteOptimizer->UpdateItem(aKey, valueFromString);
      }
    }

    autoRevertValue.release();
  }

  aNotifyInfo.changed() = changed;
  aNotifyInfo.oldValue() = oldValue;

  return NS_OK;
}

nsresult LSSnapshot::RemoveItem(const nsAString& aKey,
                                LSNotifyInfo& aNotifyInfo) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mActor);
  MOZ_ASSERT(mInitialized);
  MOZ_ASSERT(!mSentFinish);

  MaybeScheduleStableStateCallback();

  nsString oldValue;
  nsresult rv =
      GetItemInternal(aKey, Optional<nsString>(VoidString()), oldValue);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  bool changed;
  if (oldValue.IsVoid()) {
    changed = false;
  } else {
    changed = true;

    auto autoRevertValue = MakeScopeExit([&] {
      MOZ_ASSERT(!oldValue.IsVoid());
      mValues.InsertOrUpdate(aKey, oldValue);
    });

    // Anything that can fail must be done early before we start modifying the
    // state.

    Maybe<LSValue> oldValueFromString;
    if (mHasOtherProcessObservers) {
      oldValueFromString.emplace();
      if (NS_WARN_IF(!oldValueFromString->InitFromString(oldValue))) {
        return NS_ERROR_FAILURE;
      }
    }

    int64_t delta = -(static_cast<int64_t>(aKey.Length()) +
                      static_cast<int64_t>(oldValue.Length()));

    DebugOnly<nsresult> rv = UpdateUsage(delta);
    MOZ_ASSERT(NS_SUCCEEDED(rv));

    if (mLoadState == LoadState::Partial) {
      mLength--;
    }

    if (mHasOtherProcessObservers) {
      MOZ_ASSERT(mWriteAndNotifyInfos);
      MOZ_ASSERT(oldValueFromString.isSome());

      LSRemoveItemAndNotifyInfo removeItemAndNotifyInfo;
      removeItemAndNotifyInfo.key() = aKey;
      removeItemAndNotifyInfo.oldValue() = oldValueFromString.value();

      mWriteAndNotifyInfos->AppendElement(std::move(removeItemAndNotifyInfo));
    } else {
      MOZ_ASSERT(mWriteOptimizer);

      mWriteOptimizer->DeleteItem(aKey);
    }

    autoRevertValue.release();
  }

  aNotifyInfo.changed() = changed;
  aNotifyInfo.oldValue() = oldValue;

  return NS_OK;
}

nsresult LSSnapshot::Clear(LSNotifyInfo& aNotifyInfo) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mActor);
  MOZ_ASSERT(mInitialized);
  MOZ_ASSERT(!mSentFinish);

  MaybeScheduleStableStateCallback();

  uint32_t length;
  if (mLoadState == LoadState::Partial) {
    length = mLength;
    MOZ_ASSERT(length);

    MOZ_ALWAYS_TRUE(mActor->SendLoaded());

    mLoadedItems.Clear();
    mUnknownItems.Clear();
    mLength = 0;
    mLoadState = LoadState::AllOrderedItems;
  } else {
    length = mValues.Count();
  }

  bool changed;
  if (!length) {
    changed = false;
  } else {
    changed = true;

    int64_t delta = 0;
    for (const auto& entry : mValues) {
      const nsAString& key = entry.GetKey();
      const nsString& value = entry.GetData();

      delta += -static_cast<int64_t>(key.Length()) -
               static_cast<int64_t>(value.Length());
    }

    DebugOnly<nsresult> rv = UpdateUsage(delta);
    MOZ_ASSERT(NS_SUCCEEDED(rv));

    mValues.Clear();

    if (mHasOtherProcessObservers) {
      MOZ_ASSERT(mWriteAndNotifyInfos);

      LSClearInfo clearInfo;

      mWriteAndNotifyInfos->AppendElement(std::move(clearInfo));
    } else {
      MOZ_ASSERT(mWriteOptimizer);

      mWriteOptimizer->Truncate();
    }
  }

  aNotifyInfo.changed() = changed;

  return NS_OK;
}

void LSSnapshot::MarkDirty() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mActor);
  MOZ_ASSERT(mInitialized);
  MOZ_ASSERT(!mSentFinish);

  if (mDirty) {
    return;
  }

  mDirty = true;

  if (!mExplicit && !mHasPendingStableStateCallback) {
    CancelIdleTimer();

    MOZ_ALWAYS_SUCCEEDS(Checkpoint());

    MOZ_ALWAYS_SUCCEEDS(Finish());
  } else {
    MOZ_ASSERT(!mHasPendingIdleTimerCallback);
  }
}

nsresult LSSnapshot::ExplicitCheckpoint() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mActor);
  MOZ_ASSERT(mExplicit);
  MOZ_ASSERT(!mHasPendingStableStateCallback);
  MOZ_ASSERT(!mHasPendingIdleTimerCallback);
  MOZ_ASSERT(mInitialized);
  MOZ_ASSERT(!mSentFinish);

  nsresult rv = Checkpoint(/* aSync */ true);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult LSSnapshot::ExplicitEnd() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mActor);
  MOZ_ASSERT(mExplicit);
  MOZ_ASSERT(!mHasPendingStableStateCallback);
  MOZ_ASSERT(!mHasPendingIdleTimerCallback);
  MOZ_ASSERT(mInitialized);
  MOZ_ASSERT(!mSentFinish);

  nsresult rv = Checkpoint();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  RefPtr<LSSnapshot> kungFuDeathGrip = this;

  rv = Finish(/* aSync */ true);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

int64_t LSSnapshot::GetUsage() const {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mActor);
  MOZ_ASSERT(mInitialized);
  MOZ_ASSERT(!mSentFinish);

  return mUsage;
}

void LSSnapshot::ScheduleStableStateCallback() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mIdleTimer);
  MOZ_ASSERT(!mExplicit);
  MOZ_ASSERT(!mHasPendingStableStateCallback);

  CancelIdleTimer();

  nsCOMPtr<nsIRunnable> runnable = this;
  nsContentUtils::RunInStableState(runnable.forget());

  mHasPendingStableStateCallback = true;
}

void LSSnapshot::MaybeScheduleStableStateCallback() {
  AssertIsOnOwningThread();

  if (!mExplicit && !mHasPendingStableStateCallback) {
    ScheduleStableStateCallback();
  } else {
    MOZ_ASSERT(!mHasPendingIdleTimerCallback);
  }
}

nsresult LSSnapshot::GetItemInternal(const nsAString& aKey,
                                     const Optional<nsString>& aValue,
                                     nsAString& aResult) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mActor);
  MOZ_ASSERT(mInitialized);
  MOZ_ASSERT(!mSentFinish);

  nsString result;

  switch (mLoadState) {
    case LoadState::Partial: {
      if (mValues.Get(aKey, &result)) {
        MOZ_ASSERT(!result.IsVoid());
      } else if (mLoadedItems.Contains(aKey) || mUnknownItems.Contains(aKey)) {
        result.SetIsVoid(true);
      } else {
        LSValue value;
        nsTArray<LSItemInfo> itemInfos;
        if (NS_WARN_IF(!mActor->SendLoadValueAndMoreItems(
                nsString(aKey), &value, &itemInfos))) {
          return NS_ERROR_FAILURE;
        }

        result = value.AsString();

        if (result.IsVoid()) {
          mUnknownItems.Insert(aKey);
        } else {
          mLoadedItems.Insert(aKey);
          mValues.InsertOrUpdate(aKey, result);

          // mLoadedItems.Count()==mInitLength is checked below.
        }

        for (uint32_t i = 0; i < itemInfos.Length(); i++) {
          const LSItemInfo& itemInfo = itemInfos[i];

          mLoadedItems.Insert(itemInfo.key());
          mValues.InsertOrUpdate(itemInfo.key(), itemInfo.value().AsString());
        }

        if (mLoadedItems.Count() == mInitLength) {
          mLoadedItems.Clear();
          mUnknownItems.Clear();
          mLength = 0;
          mLoadState = LoadState::AllUnorderedItems;
        }
      }

      if (aValue.WasPassed()) {
        const nsString& value = aValue.Value();
        if (!value.IsVoid()) {
          mValues.InsertOrUpdate(aKey, value);
        } else if (!result.IsVoid()) {
          mValues.Remove(aKey);
        }
      }

      break;
    }

    case LoadState::AllOrderedKeys: {
      if (mValues.Get(aKey, &result)) {
        if (result.IsVoid()) {
          LSValue value;
          nsTArray<LSItemInfo> itemInfos;
          if (NS_WARN_IF(!mActor->SendLoadValueAndMoreItems(
                  nsString(aKey), &value, &itemInfos))) {
            return NS_ERROR_FAILURE;
          }

          result = value.AsString();

          MOZ_ASSERT(!result.IsVoid());

          mLoadedItems.Insert(aKey);
          mValues.InsertOrUpdate(aKey, result);

          // mLoadedItems.Count()==mInitLength is checked below.

          for (uint32_t i = 0; i < itemInfos.Length(); i++) {
            const LSItemInfo& itemInfo = itemInfos[i];

            mLoadedItems.Insert(itemInfo.key());
            mValues.InsertOrUpdate(itemInfo.key(), itemInfo.value().AsString());
          }

          if (mLoadedItems.Count() == mInitLength) {
            mLoadedItems.Clear();
            MOZ_ASSERT(mLength == 0);
            mLoadState = LoadState::AllOrderedItems;
          }
        }
      } else {
        result.SetIsVoid(true);
      }

      if (aValue.WasPassed()) {
        const nsString& value = aValue.Value();
        if (!value.IsVoid()) {
          mValues.InsertOrUpdate(aKey, value);
        } else if (!result.IsVoid()) {
          mValues.Remove(aKey);
        }
      }

      break;
    }

    case LoadState::AllUnorderedItems:
    case LoadState::AllOrderedItems: {
      if (aValue.WasPassed()) {
        const nsString& value = aValue.Value();
        if (!value.IsVoid()) {
          mValues.WithEntryHandle(aKey, [&](auto&& entry) {
            if (entry) {
              result = std::exchange(entry.Data(), value);
            } else {
              result.SetIsVoid(true);
              entry.Insert(value);
            }
          });
        } else {
          if (auto entry = mValues.Lookup(aKey)) {
            result = entry.Data();
            MOZ_ASSERT(!result.IsVoid());
            entry.Remove();
          } else {
            result.SetIsVoid(true);
          }
        }
      } else {
        if (mValues.Get(aKey, &result)) {
          MOZ_ASSERT(!result.IsVoid());
        } else {
          result.SetIsVoid(true);
        }
      }

      break;
    }

    default:
      MOZ_CRASH("Bad state!");
  }

  aResult = result;
  return NS_OK;
}

nsresult LSSnapshot::EnsureAllKeys() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mActor);
  MOZ_ASSERT(mInitialized);
  MOZ_ASSERT(!mSentFinish);
  MOZ_ASSERT(mLoadState != LoadState::Initial);

  if (mLoadState == LoadState::AllOrderedKeys ||
      mLoadState == LoadState::AllOrderedItems) {
    return NS_OK;
  }

  nsTArray<nsString> keys;
  if (NS_WARN_IF(!mActor->SendLoadKeys(&keys))) {
    return NS_ERROR_FAILURE;
  }

  nsTHashMap<nsStringHashKey, nsString> newValues;

  for (auto key : keys) {
    newValues.InsertOrUpdate(key, VoidString());
  }

  if (mHasOtherProcessObservers) {
    MOZ_ASSERT(mWriteAndNotifyInfos);

    if (!mWriteAndNotifyInfos->IsEmpty()) {
      for (uint32_t index = 0; index < mWriteAndNotifyInfos->Length();
           index++) {
        const LSWriteAndNotifyInfo& writeAndNotifyInfo =
            mWriteAndNotifyInfos->ElementAt(index);

        switch (writeAndNotifyInfo.type()) {
          case LSWriteAndNotifyInfo::TLSSetItemAndNotifyInfo: {
            newValues.InsertOrUpdate(
                writeAndNotifyInfo.get_LSSetItemAndNotifyInfo().key(),
                VoidString());
            break;
          }
          case LSWriteAndNotifyInfo::TLSRemoveItemAndNotifyInfo: {
            newValues.Remove(
                writeAndNotifyInfo.get_LSRemoveItemAndNotifyInfo().key());
            break;
          }
          case LSWriteAndNotifyInfo::TLSClearInfo: {
            newValues.Clear();
            break;
          }

          default:
            MOZ_CRASH("Should never get here!");
        }
      }
    }
  } else {
    MOZ_ASSERT(mWriteOptimizer);

    if (mWriteOptimizer->HasWrites()) {
      nsTArray<LSWriteInfo> writeInfos;
      mWriteOptimizer->Enumerate(writeInfos);

      MOZ_ASSERT(!writeInfos.IsEmpty());

      for (uint32_t index = 0; index < writeInfos.Length(); index++) {
        const LSWriteInfo& writeInfo = writeInfos[index];

        switch (writeInfo.type()) {
          case LSWriteInfo::TLSSetItemInfo: {
            newValues.InsertOrUpdate(writeInfo.get_LSSetItemInfo().key(),
                                     VoidString());
            break;
          }
          case LSWriteInfo::TLSRemoveItemInfo: {
            newValues.Remove(writeInfo.get_LSRemoveItemInfo().key());
            break;
          }
          case LSWriteInfo::TLSClearInfo: {
            newValues.Clear();
            break;
          }

          default:
            MOZ_CRASH("Should never get here!");
        }
      }
    }
  }

  MOZ_ASSERT_IF(mLoadState == LoadState::AllUnorderedItems,
                newValues.Count() == mValues.Count());

  for (auto iter = newValues.Iter(); !iter.Done(); iter.Next()) {
    nsString value;
    if (mValues.Get(iter.Key(), &value)) {
      iter.Data() = value;
    }
  }

  mValues.SwapElements(newValues);

  if (mLoadState == LoadState::Partial) {
    mUnknownItems.Clear();
    mLength = 0;
    mLoadState = LoadState::AllOrderedKeys;
  } else {
    MOZ_ASSERT(mLoadState == LoadState::AllUnorderedItems);

    MOZ_ASSERT(mUnknownItems.Count() == 0);
    MOZ_ASSERT(mLength == 0);
    mLoadState = LoadState::AllOrderedItems;
  }

  return NS_OK;
}

nsresult LSSnapshot::UpdateUsage(int64_t aDelta) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mDatabase);
  MOZ_ASSERT(mActor);
  MOZ_ASSERT(mPeakUsage >= mUsage);
  MOZ_ASSERT(mInitialized);
  MOZ_ASSERT(!mSentFinish);

  int64_t newUsage = mUsage + aDelta;
  if (newUsage > mPeakUsage) {
    const int64_t minSize = newUsage - mPeakUsage;

    int64_t size;
    if (NS_WARN_IF(!mActor->SendIncreasePeakUsage(minSize, &size))) {
      return NS_ERROR_FAILURE;
    }

    MOZ_ASSERT(size >= 0);

    if (size == 0) {
      return NS_ERROR_FILE_NO_DEVICE_SPACE;
    }

    mPeakUsage += size;
  }

  mUsage = newUsage;
  return NS_OK;
}

nsresult LSSnapshot::Checkpoint(bool aSync) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mActor);
  MOZ_ASSERT(mInitialized);
  MOZ_ASSERT(!mSentFinish);

  if (mHasOtherProcessObservers) {
    MOZ_ASSERT(mWriteAndNotifyInfos);

    if (!mWriteAndNotifyInfos->IsEmpty()) {
      if (aSync) {
        MOZ_ALWAYS_TRUE(
            mActor->SendSyncCheckpointAndNotify(*mWriteAndNotifyInfos));
      } else {
        MOZ_ALWAYS_TRUE(
            mActor->SendAsyncCheckpointAndNotify(*mWriteAndNotifyInfos));
      }

      mWriteAndNotifyInfos->Clear();
    }
  } else {
    MOZ_ASSERT(mWriteOptimizer);

    if (mWriteOptimizer->HasWrites()) {
      nsTArray<LSWriteInfo> writeInfos;
      mWriteOptimizer->Enumerate(writeInfos);

      MOZ_ASSERT(!writeInfos.IsEmpty());

      if (aSync) {
        MOZ_ALWAYS_TRUE(mActor->SendSyncCheckpoint(writeInfos));
      } else {
        MOZ_ALWAYS_TRUE(mActor->SendAsyncCheckpoint(writeInfos));
      }

      mWriteOptimizer->Reset();
    }
  }

  return NS_OK;
}

nsresult LSSnapshot::Finish(bool aSync) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mDatabase);
  MOZ_ASSERT(mActor);
  MOZ_ASSERT(mInitialized);
  MOZ_ASSERT(!mSentFinish);

  if (aSync) {
    MOZ_ALWAYS_TRUE(mActor->SendSyncFinish());
  } else {
    MOZ_ALWAYS_TRUE(mActor->SendAsyncFinish());
  }

  mDatabase->NoteFinishedSnapshot(this);

#ifdef DEBUG
  mSentFinish = true;
#endif

  // Clear the self reference added in Init method.
  MOZ_ASSERT(mSelfRef);
  mSelfRef = nullptr;

  return NS_OK;
}

void LSSnapshot::CancelIdleTimer() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mIdleTimer);

  if (mHasPendingIdleTimerCallback) {
    MOZ_ALWAYS_SUCCEEDS(mIdleTimer->Cancel());
    mHasPendingIdleTimerCallback = false;
  }
}

// static
void LSSnapshot::IdleTimerCallback(nsITimer* aTimer, void* aClosure) {
  MOZ_ASSERT(aTimer);

  auto* self = static_cast<LSSnapshot*>(aClosure);
  MOZ_ASSERT(self);
  MOZ_ASSERT(self->mIdleTimer);
  MOZ_ASSERT(SameCOMIdentity(self->mIdleTimer, aTimer));
  MOZ_ASSERT(!self->mHasPendingStableStateCallback);
  MOZ_ASSERT(self->mHasPendingIdleTimerCallback);

  self->mHasPendingIdleTimerCallback = false;

  MOZ_ALWAYS_SUCCEEDS(self->Finish());
}

NS_IMPL_ISUPPORTS(LSSnapshot, nsIRunnable)

NS_IMETHODIMP
LSSnapshot::Run() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(!mExplicit);
  MOZ_ASSERT(mHasPendingStableStateCallback);
  MOZ_ASSERT(!mHasPendingIdleTimerCallback);

  mHasPendingStableStateCallback = false;

  MOZ_ALWAYS_SUCCEEDS(Checkpoint());

  // 1. The unused pre-incremented snapshot peak usage can't be undone when
  //    there are other snapshots for the same database. We only add a pending
  //    usage delta when a snapshot finishes and usage deltas are then applied
  //    when the last database becomes inactive.
  // 2. If there's a snapshot with pre-incremented peak usage, the next
  //    snapshot will use that as a base for its usage.
  // 3. When a task for given snapshot finishes, we try to reuse the snapshot
  //    by only checkpointing the snapshot and delaying the finish by a timer.
  // 4. If two or more tabs for the same origin use localStorage periodically
  //    at the same time the usage gradually grows until it hits the quota
  //    limit.
  // 5. We prevent that from happening by finishing the snapshot immediatelly
  //    if there are databases in other processess.

  if (mDirty || mHasOtherProcessDatabases ||
      !Preferences::GetBool("dom.storage.snapshot_reusing")) {
    MOZ_ALWAYS_SUCCEEDS(Finish());
  } else {
    MOZ_ASSERT(mIdleTimer);

    MOZ_ALWAYS_SUCCEEDS(mIdleTimer->InitWithNamedFuncCallback(
        IdleTimerCallback, this,
        StaticPrefs::dom_storage_snapshot_idle_timeout_ms(),
        nsITimer::TYPE_ONE_SHOT, "LSSnapshot::IdleTimerCallback"));

    mHasPendingIdleTimerCallback = true;
  }

  return NS_OK;
}

}  // namespace mozilla::dom
