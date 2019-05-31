/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_localstorage_LSWriteOptimizer_h
#define mozilla_dom_localstorage_LSWriteOptimizer_h

#include "mozilla/CheckedInt.h"

namespace mozilla {
namespace dom {

/**
 * Base class for coalescing manipulation queue.
 */
class LSWriteOptimizerBase {
  class WriteInfoComparator;

 protected:
  class WriteInfo;
  class DeleteItemInfo;
  class TruncateInfo;

  nsAutoPtr<WriteInfo> mTruncateInfo;
  nsClassHashtable<nsStringHashKey, WriteInfo> mWriteInfos;
  CheckedUint64 mLastSerialNumber;
  int64_t mTotalDelta;

  NS_DECL_OWNINGTHREAD

 public:
  LSWriteOptimizerBase() : mLastSerialNumber(0), mTotalDelta(0) {}

  LSWriteOptimizerBase(LSWriteOptimizerBase&& aWriteOptimizer)
      : mTruncateInfo(std::move(aWriteOptimizer.mTruncateInfo)) {
    AssertIsOnOwningThread();
    MOZ_ASSERT(&aWriteOptimizer != this);

    mWriteInfos.SwapElements(aWriteOptimizer.mWriteInfos);
    mTotalDelta = aWriteOptimizer.mTotalDelta;
    aWriteOptimizer.mTotalDelta = 0;
  }

  void AssertIsOnOwningThread() const {
    NS_ASSERT_OWNINGTHREAD(LSWriteOptimizerBase);
  }

  void DeleteItem(const nsAString& aKey, int64_t aDelta = 0);

  void Truncate(int64_t aDelta = 0);

  bool HasWrites() const {
    AssertIsOnOwningThread();

    return mTruncateInfo || !mWriteInfos.IsEmpty();
  }

  void Reset() {
    AssertIsOnOwningThread();

    mTruncateInfo = nullptr;
    mWriteInfos.Clear();
  }

 protected:
  uint64_t NextSerialNumber() {
    AssertIsOnOwningThread();

    mLastSerialNumber++;

    MOZ_ASSERT(mLastSerialNumber.isValid());

    return mLastSerialNumber.value();
  }

  /**
   * This method can be used by derived classes to get a sorted list of write
   * infos. Write infos are sorted by the serial number.
   */
  void GetSortedWriteInfos(nsTArray<WriteInfo*>& aWriteInfos);
};

/**
 * Base class for specific mutations.
 */
class LSWriteOptimizerBase::WriteInfo {
  uint64_t mSerialNumber;

 public:
  WriteInfo(uint64_t aSerialNumber) : mSerialNumber(aSerialNumber) {}

  virtual ~WriteInfo() = default;

  uint64_t SerialNumber() const { return mSerialNumber; }

  enum Type { InsertItem = 0, UpdateItem, DeleteItem, Truncate };

  virtual Type GetType() = 0;
};

class LSWriteOptimizerBase::DeleteItemInfo final : public WriteInfo {
  nsString mKey;

 public:
  DeleteItemInfo(uint64_t aSerialNumber, const nsAString& aKey)
      : WriteInfo(aSerialNumber), mKey(aKey) {}

  const nsAString& GetKey() const { return mKey; }

 private:
  Type GetType() override { return DeleteItem; }
};

/**
 * Truncate mutation.
 */
class LSWriteOptimizerBase::TruncateInfo final : public WriteInfo {
 public:
  explicit TruncateInfo(uint64_t aSerialNumber) : WriteInfo(aSerialNumber) {}

 private:
  Type GetType() override { return Truncate; }
};

/**
 * Coalescing manipulation queue.
 */
template <typename T, typename U = T>
class LSWriteOptimizer;

template <typename T, typename U>
class LSWriteOptimizer : public LSWriteOptimizerBase {
 protected:
  class InsertItemInfo;
  class UpdateItemInfo;

 public:
  void InsertItem(const nsAString& aKey, const T& aValue, int64_t aDelta = 0);

  void UpdateItem(const nsAString& aKey, const T& aValue, int64_t aDelta = 0);
};

/**
 * Insert mutation (the key did not previously exist).
 */
template <typename T, typename U>
class LSWriteOptimizer<T, U>::InsertItemInfo : public WriteInfo {
  nsString mKey;
  U mValue;

 public:
  InsertItemInfo(uint64_t aSerialNumber, const nsAString& aKey, const T& aValue)
      : WriteInfo(aSerialNumber), mKey(aKey), mValue(aValue) {}

  const nsAString& GetKey() const { return mKey; }

  const T& GetValue() const { return mValue; }

 private:
  WriteInfo::Type GetType() override { return InsertItem; }
};

/**
 * Update mutation (the key already existed).
 */
template <typename T, typename U>
class LSWriteOptimizer<T, U>::UpdateItemInfo final : public InsertItemInfo {
  bool mUpdateWithMove;

 public:
  UpdateItemInfo(uint64_t aSerialNumber, const nsAString& aKey, const T& aValue,
                 bool aUpdateWithMove)
      : InsertItemInfo(aSerialNumber, aKey, aValue),
        mUpdateWithMove(aUpdateWithMove) {}

  bool UpdateWithMove() const { return mUpdateWithMove; }

 private:
  WriteInfo::Type GetType() override { return WriteInfo::UpdateItem; }
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_localstorage_LSWriteOptimizer_h
