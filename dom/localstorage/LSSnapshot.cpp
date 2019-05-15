/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "LSSnapshot.h"

#include "nsContentUtils.h"

namespace mozilla {
namespace dom {

namespace {

const uint32_t kSnapshotTimeoutMs = 20000;

}  // namespace

/**
 * Coalescing manipulation queue used by `LSSnapshot`.  Used by `LSSnapshot` to
 * buffer and coalesce manipulations before they are sent to the parent process,
 * when a Snapshot Checkpoints. (This can only be done when there are no
 * observers for other content processes.)
 */
class SnapshotWriteOptimizer final
    : public LSWriteOptimizer<nsAString, nsString> {
 public:
  void Enumerate(nsTArray<LSWriteInfo>& aWriteInfos);
};

void SnapshotWriteOptimizer::Enumerate(nsTArray<LSWriteInfo>& aWriteInfos) {
  AssertIsOnOwningThread();

  // The mWriteInfos hash table contains all write infos, but it keeps them in
  // an arbitrary order, which means write infos need to be sorted before being
  // processed.

  nsTArray<WriteInfo*> writeInfos;
  GetSortedWriteInfos(writeInfos);

  for (WriteInfo* writeInfo : writeInfos) {
    switch (writeInfo->GetType()) {
      case WriteInfo::InsertItem: {
        auto insertItemInfo = static_cast<InsertItemInfo*>(writeInfo);

        LSSetItemInfo setItemInfo;
        setItemInfo.key() = insertItemInfo->GetKey();
        setItemInfo.value() = LSValue(insertItemInfo->GetValue());

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
        setItemInfo.value() = LSValue(updateItemInfo->GetValue());

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
      mExactUsage(0),
      mPeakUsage(0),
      mLoadState(LoadState::Initial),
      mHasOtherProcessObservers(false),
      mExplicit(false),
      mHasPendingStableStateCallback(false),
      mHasPendingTimerCallback(false),
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
  MOZ_ASSERT(!mHasPendingTimerCallback);
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
      mLoadedItems.PutEntry(itemInfo.key());
    }

    mValues.Put(itemInfo.key(), value.AsString());
  }

  if (loadState == LoadState::Partial) {
    if (aInitInfo.addKeyToUnknownItems()) {
      mUnknownItems.PutEntry(aKey);
    }
    mInitLength = aInitInfo.totalLength();
    mLength = mInitLength;
  } else if (loadState == LoadState::AllOrderedKeys) {
    mInitLength = aInitInfo.totalLength();
  } else {
    MOZ_ASSERT(loadState == LoadState::AllOrderedItems);
  }

  mExactUsage = aInitInfo.initialUsage();
  mPeakUsage = aInitInfo.peakUsage();

  mLoadState = aInitInfo.loadState();

  mHasOtherProcessObservers = aInitInfo.hasOtherProcessObservers();

  mExplicit = aExplicit;

#ifdef DEBUG
  mInitialized = true;
#endif

  if (mHasOtherProcessObservers) {
    mWriteAndNotifyInfos = new nsTArray<LSWriteAndNotifyInfo>();
  } else {
    mWriteOptimizer = new SnapshotWriteOptimizer();
  }

  if (!mExplicit) {
    mTimer = NS_NewTimer();
    MOZ_ASSERT(mTimer);

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

  for (auto iter = mValues.ConstIter(); !iter.Done(); iter.Next()) {
    aKeys.AppendElement(iter.Key());
  }

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

    int64_t delta = static_cast<int64_t>(aValue.Length()) -
                    static_cast<int64_t>(oldValue.Length());

    if (oldValue.IsVoid()) {
      delta += static_cast<int64_t>(aKey.Length());
    }

    rv = UpdateUsage(delta);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      if (oldValue.IsVoid()) {
        mValues.Remove(aKey);
      } else {
        mValues.Put(aKey, oldValue);
      }
      return rv;
    }

    if (oldValue.IsVoid() && mLoadState == LoadState::Partial) {
      mLength++;
    }

    if (mHasOtherProcessObservers) {
      MOZ_ASSERT(mWriteAndNotifyInfos);

      LSSetItemAndNotifyInfo setItemAndNotifyInfo;
      setItemAndNotifyInfo.key() = aKey;
      setItemAndNotifyInfo.oldValue() = LSValue(oldValue);
      setItemAndNotifyInfo.value() = LSValue(aValue);

      mWriteAndNotifyInfos->AppendElement(std::move(setItemAndNotifyInfo));
    } else {
      MOZ_ASSERT(mWriteOptimizer);

      if (oldValue.IsVoid()) {
        mWriteOptimizer->InsertItem(aKey, aValue);
      } else {
        mWriteOptimizer->UpdateItem(aKey, aValue);
      }
    }
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

    int64_t delta = -(static_cast<int64_t>(aKey.Length()) +
                      static_cast<int64_t>(oldValue.Length()));

    DebugOnly<nsresult> rv = UpdateUsage(delta);
    MOZ_ASSERT(NS_SUCCEEDED(rv));

    if (mLoadState == LoadState::Partial) {
      mLength--;
    }

    if (mHasOtherProcessObservers) {
      MOZ_ASSERT(mWriteAndNotifyInfos);

      LSRemoveItemAndNotifyInfo removeItemAndNotifyInfo;
      removeItemAndNotifyInfo.key() = aKey;
      removeItemAndNotifyInfo.oldValue() = LSValue(oldValue);

      mWriteAndNotifyInfos->AppendElement(std::move(removeItemAndNotifyInfo));
    } else {
      MOZ_ASSERT(mWriteOptimizer);

      mWriteOptimizer->DeleteItem(aKey);
    }
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

    DebugOnly<nsresult> rv = UpdateUsage(-mExactUsage);
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
    CancelTimer();

    MOZ_ALWAYS_SUCCEEDS(Checkpoint());

    MOZ_ALWAYS_SUCCEEDS(Finish());
  } else {
    MOZ_ASSERT(!mHasPendingTimerCallback);
  }
}

nsresult LSSnapshot::End() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mActor);
  MOZ_ASSERT(mExplicit);
  MOZ_ASSERT(!mHasPendingStableStateCallback);
  MOZ_ASSERT(!mHasPendingTimerCallback);
  MOZ_ASSERT(mInitialized);
  MOZ_ASSERT(!mSentFinish);

  nsresult rv = Checkpoint();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  RefPtr<LSSnapshot> kungFuDeathGrip = this;

  rv = Finish();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (NS_WARN_IF(!mActor->SendPing())) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

void LSSnapshot::ScheduleStableStateCallback() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mTimer);
  MOZ_ASSERT(!mExplicit);
  MOZ_ASSERT(!mHasPendingStableStateCallback);

  CancelTimer();

  nsCOMPtr<nsIRunnable> runnable = this;
  nsContentUtils::RunInStableState(runnable.forget());

  mHasPendingStableStateCallback = true;
}

void LSSnapshot::MaybeScheduleStableStateCallback() {
  AssertIsOnOwningThread();

  if (!mExplicit && !mHasPendingStableStateCallback) {
    ScheduleStableStateCallback();
  } else {
    MOZ_ASSERT(!mHasPendingTimerCallback);
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
      } else if (mLoadedItems.GetEntry(aKey) || mUnknownItems.GetEntry(aKey)) {
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
          mUnknownItems.PutEntry(aKey);
        } else {
          mLoadedItems.PutEntry(aKey);
          mValues.Put(aKey, result);

          // mLoadedItems.Count()==mInitLength is checked below.
        }

        for (uint32_t i = 0; i < itemInfos.Length(); i++) {
          const LSItemInfo& itemInfo = itemInfos[i];

          mLoadedItems.PutEntry(itemInfo.key());
          mValues.Put(itemInfo.key(), itemInfo.value().AsString());
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
          mValues.Put(aKey, value);
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

          mLoadedItems.PutEntry(aKey);
          mValues.Put(aKey, result);

          // mLoadedItems.Count()==mInitLength is checked below.

          for (uint32_t i = 0; i < itemInfos.Length(); i++) {
            const LSItemInfo& itemInfo = itemInfos[i];

            mLoadedItems.PutEntry(itemInfo.key());
            mValues.Put(itemInfo.key(), itemInfo.value().AsString());
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
          mValues.Put(aKey, value);
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
          auto entry = mValues.LookupForAdd(aKey);
          if (entry) {
            result = entry.Data();
            entry.Data() = value;
          } else {
            result.SetIsVoid(true);
            entry.OrInsert([value]() { return value; });
          }
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

  nsDataHashtable<nsStringHashKey, nsString> newValues;

  for (auto key : keys) {
    newValues.Put(key, VoidString());
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
            newValues.Put(writeAndNotifyInfo.get_LSSetItemAndNotifyInfo().key(),
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
            newValues.Put(writeInfo.get_LSSetItemInfo().key(), VoidString());
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
  MOZ_ASSERT(mPeakUsage >= mExactUsage);
  MOZ_ASSERT(mInitialized);
  MOZ_ASSERT(!mSentFinish);

  int64_t newExactUsage = mExactUsage + aDelta;
  if (newExactUsage > mPeakUsage) {
    int64_t minSize = newExactUsage - mPeakUsage;
    int64_t requestedSize = minSize + 4096;
    int64_t size;
    if (NS_WARN_IF(
            !mActor->SendIncreasePeakUsage(requestedSize, minSize, &size))) {
      return NS_ERROR_FAILURE;
    }

    MOZ_ASSERT(size >= 0);

    if (size == 0) {
      return NS_ERROR_FILE_NO_DEVICE_SPACE;
    }

    mPeakUsage += size;
  }

  mExactUsage = newExactUsage;
  return NS_OK;
}

nsresult LSSnapshot::Checkpoint() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mActor);
  MOZ_ASSERT(mInitialized);
  MOZ_ASSERT(!mSentFinish);

  if (mHasOtherProcessObservers) {
    MOZ_ASSERT(mWriteAndNotifyInfos);

    if (!mWriteAndNotifyInfos->IsEmpty()) {
      MOZ_ALWAYS_TRUE(mActor->SendCheckpointAndNotify(*mWriteAndNotifyInfos));

      mWriteAndNotifyInfos->Clear();
    }
  } else {
    MOZ_ASSERT(mWriteOptimizer);

    if (mWriteOptimizer->HasWrites()) {
      nsTArray<LSWriteInfo> writeInfos;
      mWriteOptimizer->Enumerate(writeInfos);

      MOZ_ASSERT(!writeInfos.IsEmpty());

      MOZ_ALWAYS_TRUE(mActor->SendCheckpoint(writeInfos));

      mWriteOptimizer->Reset();
    }
  }

  return NS_OK;
}

nsresult LSSnapshot::Finish() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mDatabase);
  MOZ_ASSERT(mActor);
  MOZ_ASSERT(mInitialized);
  MOZ_ASSERT(!mSentFinish);

  MOZ_ALWAYS_TRUE(mActor->SendFinish());

  mDatabase->NoteFinishedSnapshot(this);

#ifdef DEBUG
  mSentFinish = true;
#endif

  // Clear the self reference added in Init method.
  MOZ_ASSERT(mSelfRef);
  mSelfRef = nullptr;

  return NS_OK;
}

void LSSnapshot::CancelTimer() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mTimer);

  if (mHasPendingTimerCallback) {
    MOZ_ALWAYS_SUCCEEDS(mTimer->Cancel());
    mHasPendingTimerCallback = false;
  }
}

// static
void LSSnapshot::TimerCallback(nsITimer* aTimer, void* aClosure) {
  MOZ_ASSERT(aTimer);

  auto* self = static_cast<LSSnapshot*>(aClosure);
  MOZ_ASSERT(self);
  MOZ_ASSERT(self->mTimer);
  MOZ_ASSERT(SameCOMIdentity(self->mTimer, aTimer));
  MOZ_ASSERT(!self->mHasPendingStableStateCallback);
  MOZ_ASSERT(self->mHasPendingTimerCallback);

  self->mHasPendingTimerCallback = false;

  MOZ_ALWAYS_SUCCEEDS(self->Finish());
}

NS_IMPL_ISUPPORTS(LSSnapshot, nsIRunnable)

NS_IMETHODIMP
LSSnapshot::Run() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(!mExplicit);
  MOZ_ASSERT(mHasPendingStableStateCallback);
  MOZ_ASSERT(!mHasPendingTimerCallback);

  mHasPendingStableStateCallback = false;

  MOZ_ALWAYS_SUCCEEDS(Checkpoint());

  if (mDirty || !Preferences::GetBool("dom.storage.snapshot_reusing")) {
    MOZ_ALWAYS_SUCCEEDS(Finish());
  } else if (!mExplicit) {
    MOZ_ASSERT(mTimer);

    MOZ_ALWAYS_SUCCEEDS(mTimer->InitWithNamedFuncCallback(
        TimerCallback, this, kSnapshotTimeoutMs, nsITimer::TYPE_ONE_SHOT,
        "LSSnapshot::TimerCallback"));

    mHasPendingTimerCallback = true;
  }

  return NS_OK;
}

}  // namespace dom
}  // namespace mozilla
