/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "LSWriteOptimizer.h"

namespace mozilla {
namespace dom {

void LSWriteOptimizerBase::DeleteItem(const nsAString& aKey, int64_t aDelta) {
  AssertIsOnOwningThread();

  WriteInfo* existingWriteInfo;
  if (mWriteInfos.Get(aKey, &existingWriteInfo) &&
      existingWriteInfo->GetType() == WriteInfo::InsertItem) {
    mWriteInfos.Remove(aKey);
  } else {
    nsAutoPtr<WriteInfo> newWriteInfo(new DeleteItemInfo(aKey));
    mWriteInfos.Put(aKey, newWriteInfo.forget());
  }

  mTotalDelta += aDelta;
}

void LSWriteOptimizerBase::Truncate(int64_t aDelta) {
  AssertIsOnOwningThread();

  mWriteInfos.Clear();

  if (!mTruncateInfo) {
    mTruncateInfo = new TruncateInfo();
  }

  mTotalDelta += aDelta;
}

template <typename T, typename U>
void LSWriteOptimizer<T, U>::InsertItem(const nsAString& aKey, const T& aValue,
                                        int64_t aDelta) {
  AssertIsOnOwningThread();

  WriteInfo* existingWriteInfo;
  nsAutoPtr<WriteInfo> newWriteInfo;
  if (mWriteInfos.Get(aKey, &existingWriteInfo) &&
      existingWriteInfo->GetType() == WriteInfo::DeleteItem) {
    newWriteInfo = new UpdateItemInfo(aKey, aValue);
  } else {
    newWriteInfo = new InsertItemInfo(aKey, aValue);
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
    newWriteInfo = new InsertItemInfo(aKey, aValue);
  } else {
    newWriteInfo = new UpdateItemInfo(aKey, aValue);
  }
  mWriteInfos.Put(aKey, newWriteInfo.forget());

  mTotalDelta += aDelta;
}

}  // namespace dom
}  // namespace mozilla
