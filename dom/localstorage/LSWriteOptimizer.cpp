/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "LSWriteOptimizer.h"

namespace mozilla {
namespace dom {

class LSWriteOptimizerBase::WriteInfoComparator {
 public:
  bool Equals(const WriteInfo* a, const WriteInfo* b) const {
    return a && b ? a->SerialNumber() == b->SerialNumber()
                  : !a && !b ? true : false;
  }

  bool LessThan(const WriteInfo* a, const WriteInfo* b) const {
    return a && b ? a->SerialNumber() < b->SerialNumber() : b ? true : false;
  }
};

void LSWriteOptimizerBase::DeleteItem(const nsAString& aKey, int64_t aDelta) {
  AssertIsOnOwningThread();

  WriteInfo* existingWriteInfo;
  if (mWriteInfos.Get(aKey, &existingWriteInfo) &&
      existingWriteInfo->GetType() == WriteInfo::InsertItem) {
    mWriteInfos.Remove(aKey);
  } else {
    nsAutoPtr<WriteInfo> newWriteInfo(
        new DeleteItemInfo(NextSerialNumber(), aKey));
    mWriteInfos.Put(aKey, newWriteInfo.forget());
  }

  mTotalDelta += aDelta;
}

void LSWriteOptimizerBase::Truncate(int64_t aDelta) {
  AssertIsOnOwningThread();

  mWriteInfos.Clear();

  if (!mTruncateInfo) {
    mTruncateInfo = new TruncateInfo(NextSerialNumber());
  }

  mTotalDelta += aDelta;
}

void LSWriteOptimizerBase::GetSortedWriteInfos(
    nsTArray<WriteInfo*>& aWriteInfos) {
  AssertIsOnOwningThread();

  if (mTruncateInfo) {
    aWriteInfos.InsertElementSorted(mTruncateInfo, WriteInfoComparator());
  }

  for (auto iter = mWriteInfos.ConstIter(); !iter.Done(); iter.Next()) {
    WriteInfo* writeInfo = iter.Data();

    aWriteInfos.InsertElementSorted(writeInfo, WriteInfoComparator());
  }
}

template <typename T, typename U>
void LSWriteOptimizer<T, U>::InsertItem(const nsAString& aKey, const T& aValue,
                                        int64_t aDelta) {
  AssertIsOnOwningThread();

  WriteInfo* existingWriteInfo;
  nsAutoPtr<WriteInfo> newWriteInfo;
  if (mWriteInfos.Get(aKey, &existingWriteInfo) &&
      existingWriteInfo->GetType() == WriteInfo::DeleteItem) {
    // We could just simply replace the deletion with ordinary update, but that
    // would preserve item's original position/index. Imagine a case when we
    // have only one existing key k1. Now let's create a new optimizer and
    // remove k1, add k2 and add k1 back. The final order should be k2, k1
    // (ordinary update would produce k1, k2). So we need to differentiate
    // between normal update and "optimized" update which resulted from a
    // deletion followed by an insertion. We use the UpdateWithMove flag for
    // this.

    newWriteInfo = new UpdateItemInfo(NextSerialNumber(), aKey, aValue,
                                      /* aUpdateWithMove */ true);
  } else {
    newWriteInfo = new InsertItemInfo(NextSerialNumber(), aKey, aValue);
  }
  mWriteInfos.Put(aKey, newWriteInfo.forget());

  mTotalDelta += aDelta;
}

template <typename T, typename U>
void LSWriteOptimizer<T, U>::UpdateItem(const nsAString& aKey, const T& aValue,
                                        int64_t aDelta) {
  AssertIsOnOwningThread();

  WriteInfo* existingWriteInfo;
  nsAutoPtr<WriteInfo> newWriteInfo;
  if (mWriteInfos.Get(aKey, &existingWriteInfo) &&
      existingWriteInfo->GetType() == WriteInfo::InsertItem) {
    newWriteInfo = new InsertItemInfo(NextSerialNumber(), aKey, aValue);
  } else {
    newWriteInfo = new UpdateItemInfo(NextSerialNumber(), aKey, aValue,
                                      /* aUpdateWithMove */ false);
  }
  mWriteInfos.Put(aKey, newWriteInfo.forget());

  mTotalDelta += aDelta;
}

}  // namespace dom
}  // namespace mozilla
