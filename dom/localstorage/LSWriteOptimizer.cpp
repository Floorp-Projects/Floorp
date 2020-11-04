/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "LSWriteOptimizer.h"

#include <new>
#include "nsBaseHashtable.h"
#include "nsTArray.h"

namespace mozilla {
namespace dom {

class LSWriteOptimizerBase::WriteInfoComparator {
 public:
  bool Equals(const WriteInfo* a, const WriteInfo* b) const {
    MOZ_ASSERT(a && b);
    return a->SerialNumber() == b->SerialNumber();
  }

  bool LessThan(const WriteInfo* a, const WriteInfo* b) const {
    MOZ_ASSERT(a && b);
    return a->SerialNumber() < b->SerialNumber();
  }
};

void LSWriteOptimizerBase::DeleteItem(const nsAString& aKey, int64_t aDelta) {
  AssertIsOnOwningThread();

  WriteInfo* existingWriteInfo;
  if (mWriteInfos.Get(aKey, &existingWriteInfo) &&
      existingWriteInfo->GetType() == WriteInfo::InsertItem) {
    mWriteInfos.Remove(aKey);
  } else {
    mWriteInfos.Put(aKey, MakeUnique<DeleteItemInfo>(NextSerialNumber(), aKey));
  }

  mTotalDelta += aDelta;
}

void LSWriteOptimizerBase::Truncate(int64_t aDelta) {
  AssertIsOnOwningThread();

  mWriteInfos.Clear();

  if (!mTruncateInfo) {
    mTruncateInfo = MakeUnique<TruncateInfo>(NextSerialNumber());
  }

  mTotalDelta += aDelta;
}

void LSWriteOptimizerBase::GetSortedWriteInfos(
    nsTArray<WriteInfo*>& aWriteInfos) {
  AssertIsOnOwningThread();

  if (mTruncateInfo) {
    aWriteInfos.InsertElementSorted(mTruncateInfo.get(), WriteInfoComparator());
  }

  for (auto iter = mWriteInfos.ConstIter(); !iter.Done(); iter.Next()) {
    WriteInfo* writeInfo = iter.UserData();

    aWriteInfos.InsertElementSorted(writeInfo, WriteInfoComparator());
  }
}

}  // namespace dom
}  // namespace mozilla
