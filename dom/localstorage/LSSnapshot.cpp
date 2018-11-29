/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "LSSnapshot.h"

#include "nsContentUtils.h"

namespace mozilla {
namespace dom {

LSSnapshot::LSSnapshot(LSDatabase* aDatabase)
  : mDatabase(aDatabase)
  , mActor(nullptr)
  , mInitLength(0)
  , mLength(0)
  , mExactUsage(0)
  , mPeakUsage(0)
  , mLoadState(LoadState::Initial)
  , mExplicit(false)
#ifdef DEBUG
  , mInitialized(false)
  , mSentFinish(false)
#endif
{
  AssertIsOnOwningThread();
}

LSSnapshot::~LSSnapshot()
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mDatabase);
  MOZ_ASSERT_IF(mInitialized, mSentFinish);

  if (mActor) {
    mActor->SendDeleteMeInternal();
    MOZ_ASSERT(!mActor, "SendDeleteMeInternal should have cleared!");
  }
}

void
LSSnapshot::SetActor(LSSnapshotChild* aActor)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(aActor);
  MOZ_ASSERT(!mActor);

  mActor = aActor;
}

nsresult
LSSnapshot::Init(const LSSnapshotInitInfo& aInitInfo,
                 bool aExplicit)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mActor);
  MOZ_ASSERT(mLoadState == LoadState::Initial);
  MOZ_ASSERT(!mInitialized);
  MOZ_ASSERT(!mSentFinish);

  if (aExplicit) {
    mSelfRef = this;
  } else {
    nsCOMPtr<nsIRunnable> runnable = this;
    nsContentUtils::RunInStableState(runnable.forget());
  }

  LoadState loadState = aInitInfo.loadState();

  const nsTArray<LSItemInfo>& itemInfos = aInitInfo.itemInfos();
  for (uint32_t i = 0; i < itemInfos.Length(); i++) {
    const LSItemInfo& itemInfo = itemInfos[i];

    const nsString& value = itemInfo.value();

    if (loadState != LoadState::AllOrderedItems && !value.IsVoid()) {
      mLoadedItems.PutEntry(itemInfo.key());
    }

    mValues.Put(itemInfo.key(), value);
  }

  if (loadState == LoadState::Partial) {
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

  mExplicit = aExplicit;

#ifdef DEBUG
  mInitialized = true;
#endif

  return NS_OK;
}

nsresult
LSSnapshot::GetLength(uint32_t* aResult)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mActor);
  MOZ_ASSERT(mInitialized);
  MOZ_ASSERT(!mSentFinish);

  if (mLoadState == LoadState::Partial) {
    *aResult = mLength;
  } else {
    *aResult = mValues.Count();
  }

  return NS_OK;
}

nsresult
LSSnapshot::GetKey(uint32_t aIndex,
                   nsAString& aResult)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mActor);
  MOZ_ASSERT(mInitialized);
  MOZ_ASSERT(!mSentFinish);

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

nsresult
LSSnapshot::GetItem(const nsAString& aKey,
                    nsAString& aResult)
{
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
        if (NS_WARN_IF(!mActor->SendLoadItem(nsString(aKey), &result))) {
          return NS_ERROR_FAILURE;
        }

        if (result.IsVoid()) {
          mUnknownItems.PutEntry(aKey);
        } else {
          mLoadedItems.PutEntry(aKey);
          mValues.Put(aKey, result);

          if (mLoadedItems.Count() == mInitLength) {
            mLoadedItems.Clear();
            mUnknownItems.Clear();
            mLength = 0;
            mLoadState = LoadState::AllUnorderedItems;
          }
        }
      }

      break;
    }

    case LoadState::AllOrderedKeys: {
      if (mValues.Get(aKey, &result)) {
        if (result.IsVoid()) {
          if (NS_WARN_IF(!mActor->SendLoadItem(nsString(aKey), &result))) {
            return NS_ERROR_FAILURE;
          }

          MOZ_ASSERT(!result.IsVoid());

          mLoadedItems.PutEntry(aKey);
          mValues.Put(aKey, result);

          if (mLoadedItems.Count() == mInitLength) {
            mLoadedItems.Clear();
            MOZ_ASSERT(mLength == 0);
            mLoadState = LoadState::AllOrderedItems;
          }
        }
      } else {
        result.SetIsVoid(true);
      }

      break;
    }

    case LoadState::AllUnorderedItems:
    case LoadState::AllOrderedItems: {
      if (mValues.Get(aKey, &result)) {
        MOZ_ASSERT(!result.IsVoid());
      } else {
        result.SetIsVoid(true);
      }

      break;
    }

    default:
      MOZ_CRASH("Bad state!");
  }

  aResult = result;
  return NS_OK;
}

nsresult
LSSnapshot::GetKeys(nsTArray<nsString>& aKeys)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mActor);
  MOZ_ASSERT(mInitialized);
  MOZ_ASSERT(!mSentFinish);

  nsresult rv = EnsureAllKeys();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  for (auto iter = mValues.ConstIter(); !iter.Done(); iter.Next()) {
    aKeys.AppendElement(iter.Key());
  }

  return NS_OK;
}

nsresult
LSSnapshot::SetItem(const nsAString& aKey,
                    const nsAString& aValue,
                    LSNotifyInfo& aNotifyInfo)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mActor);
  MOZ_ASSERT(mInitialized);
  MOZ_ASSERT(!mSentFinish);

  nsString oldValue;
  nsresult rv = GetItem(aKey, oldValue);
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
      return rv;
    }

    mValues.Put(aKey, nsString(aValue));

    if (oldValue.IsVoid() && mLoadState == LoadState::Partial) {
      mLength++;
    }

    LSSetItemInfo setItemInfo;
    setItemInfo.key() = aKey;
    setItemInfo.oldValue() = oldValue;
    setItemInfo.value() = aValue;

    mWriteInfos.AppendElement(std::move(setItemInfo));
  }

  aNotifyInfo.changed() = changed;
  aNotifyInfo.oldValue() = oldValue;

  return NS_OK;
}

nsresult
LSSnapshot::RemoveItem(const nsAString& aKey,
                       LSNotifyInfo& aNotifyInfo)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mActor);
  MOZ_ASSERT(mInitialized);
  MOZ_ASSERT(!mSentFinish);

  nsString oldValue;
  nsresult rv = GetItem(aKey, oldValue);
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

    mValues.Remove(aKey);

    if (mLoadState == LoadState::Partial) {
      mLength--;
    }

    LSRemoveItemInfo removeItemInfo;
    removeItemInfo.key() = aKey;
    removeItemInfo.oldValue() = oldValue;

    mWriteInfos.AppendElement(std::move(removeItemInfo));
  }

  aNotifyInfo.changed() = changed;
  aNotifyInfo.oldValue() = oldValue;

  return NS_OK;
}

nsresult
LSSnapshot::Clear(LSNotifyInfo& aNotifyInfo)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mActor);
  MOZ_ASSERT(mInitialized);
  MOZ_ASSERT(!mSentFinish);

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

    LSClearInfo clearInfo;

    mWriteInfos.AppendElement(std::move(clearInfo));
  }

  aNotifyInfo.changed() = changed;

  return NS_OK;
}

nsresult
LSSnapshot::Finish()
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mDatabase);
  MOZ_ASSERT(mActor);
  MOZ_ASSERT(mInitialized);
  MOZ_ASSERT(!mSentFinish);

  MOZ_ALWAYS_TRUE(mActor->SendFinish(mWriteInfos));

#ifdef DEBUG
  mSentFinish = true;
#endif

  if (mExplicit) {
    if (NS_WARN_IF(!mActor->SendPing())) {
      return NS_ERROR_FAILURE;
    }
  }

  mDatabase->NoteFinishedSnapshot(this);

  if (mExplicit) {
    mSelfRef = nullptr;
  } else {
    MOZ_ASSERT(!mSelfRef);
  }

  return NS_OK;
}

nsresult
LSSnapshot::EnsureAllKeys()
{
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

  for (uint32_t index = 0; index < mWriteInfos.Length(); index++) {
    const LSWriteInfo& writeInfo = mWriteInfos[index];

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

nsresult
LSSnapshot::UpdateUsage(int64_t aDelta)
{
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
    if (NS_WARN_IF(!mActor->SendIncreasePeakUsage(requestedSize,
                                                  minSize,
                                                  &size))) {
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

NS_IMPL_ISUPPORTS(LSSnapshot, nsIRunnable)

NS_IMETHODIMP
LSSnapshot::Run()
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(!mExplicit);

  MOZ_ALWAYS_SUCCEEDS(Finish());

  return NS_OK;
}

} // namespace dom
} // namespace mozilla
