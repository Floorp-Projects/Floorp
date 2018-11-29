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
  , mExactUsage(0)
  , mPeakUsage(0)
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
LSSnapshot::Init(const LSSnapshotInitInfo& aInitInfo)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mActor);
  MOZ_ASSERT(!mInitialized);
  MOZ_ASSERT(!mSentFinish);

  const nsTArray<LSItemInfo>& itemInfos = aInitInfo.itemInfos();
  for (uint32_t i = 0; i < itemInfos.Length(); i++) {
    const LSItemInfo& itemInfo = itemInfos[i];
    mValues.Put(itemInfo.key(), itemInfo.value());
  }

  mExactUsage = aInitInfo.initialUsage();
  mPeakUsage = aInitInfo.peakUsage();

  nsCOMPtr<nsIRunnable> runnable = this;
  nsContentUtils::RunInStableState(runnable.forget());

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

  *aResult = mValues.Count();

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
  if (!mValues.Get(aKey, &result)) {
    result.SetIsVoid(true);
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

  bool changed;
  if (!mValues.Count()) {
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
  MOZ_ASSERT(mDatabase);
  MOZ_ASSERT(mActor);
  MOZ_ASSERT(!mSentFinish);

  MOZ_ALWAYS_TRUE(mActor->SendFinish(mWriteInfos));

#ifdef DEBUG
  mSentFinish = true;
#endif

  mDatabase->NoteFinishedSnapshot(this);

  return NS_OK;
}

} // namespace dom
} // namespace mozilla
