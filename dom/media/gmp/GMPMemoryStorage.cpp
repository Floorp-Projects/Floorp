/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GMPStorage.h"
#include "nsClassHashtable.h"

namespace mozilla {
namespace gmp {

class GMPMemoryStorage : public GMPStorage {
 public:
  GMPErr Open(const nsCString& aRecordName) override {
    MOZ_ASSERT(!IsOpen(aRecordName));

    Record* record = nullptr;
    if (!mRecords.Get(aRecordName, &record)) {
      record = new Record();
      mRecords.Put(aRecordName, record);
    }
    record->mIsOpen = true;
    return GMPNoErr;
  }

  bool IsOpen(const nsCString& aRecordName) const override {
    const Record* record = mRecords.Get(aRecordName);
    if (!record) {
      return false;
    }
    return record->mIsOpen;
  }

  GMPErr Read(const nsCString& aRecordName,
              nsTArray<uint8_t>& aOutBytes) override {
    const Record* record = mRecords.Get(aRecordName);
    if (!record) {
      return GMPGenericErr;
    }
    aOutBytes = record->mData.Clone();
    return GMPNoErr;
  }

  GMPErr Write(const nsCString& aRecordName,
               const nsTArray<uint8_t>& aBytes) override {
    Record* record = nullptr;
    if (!mRecords.Get(aRecordName, &record)) {
      return GMPClosedErr;
    }
    record->mData = aBytes.Clone();
    return GMPNoErr;
  }

  void Close(const nsCString& aRecordName) override {
    Record* record = nullptr;
    if (!mRecords.Get(aRecordName, &record)) {
      return;
    }
    if (!record->mData.Length()) {
      // Record is empty, delete.
      mRecords.Remove(aRecordName);
    } else {
      record->mIsOpen = false;
    }
  }

 private:
  struct Record {
    nsTArray<uint8_t> mData;
    bool mIsOpen = false;
  };

  nsClassHashtable<nsCStringHashKey, Record> mRecords;
};

already_AddRefed<GMPStorage> CreateGMPMemoryStorage() {
  return RefPtr<GMPStorage>(new GMPMemoryStorage()).forget();
}

}  // namespace gmp
}  // namespace mozilla
