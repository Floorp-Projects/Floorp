/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GMPLog.h"
#include "GMPStorage.h"
#include "nsClassHashtable.h"

namespace mozilla::gmp {

#define LOG(msg, ...)                   \
  MOZ_LOG(GetGMPLog(), LogLevel::Debug, \
          ("GMPMemoryStorage=%p, " msg, this, ##__VA_ARGS__))

class GMPMemoryStorage : public GMPStorage {
 public:
  GMPMemoryStorage(const nsACString& aNodeId, const nsAString& aGMPName) {
    LOG("Created GMPMemoryStorage, nodeId=%s, gmpName=%s",
        aNodeId.BeginReading(), NS_ConvertUTF16toUTF8(aGMPName).get());
  }
  ~GMPMemoryStorage() { LOG("Destroyed GMPMemoryStorage"); }

  GMPErr Open(const nsACString& aRecordName) override {
    MOZ_ASSERT(!IsOpen(aRecordName));

    Record* record = mRecords.GetOrInsertNew(aRecordName);
    record->mIsOpen = true;
    return GMPNoErr;
  }

  bool IsOpen(const nsACString& aRecordName) const override {
    const Record* record = mRecords.Get(aRecordName);
    if (!record) {
      return false;
    }
    return record->mIsOpen;
  }

  GMPErr Read(const nsACString& aRecordName,
              nsTArray<uint8_t>& aOutBytes) override {
    const Record* record = mRecords.Get(aRecordName);
    if (!record) {
      return GMPGenericErr;
    }
    aOutBytes = record->mData.Clone();
    return GMPNoErr;
  }

  GMPErr Write(const nsACString& aRecordName,
               const nsTArray<uint8_t>& aBytes) override {
    Record* record = nullptr;
    if (!mRecords.Get(aRecordName, &record)) {
      return GMPClosedErr;
    }
    record->mData = aBytes.Clone();
    return GMPNoErr;
  }

  void Close(const nsACString& aRecordName) override {
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

already_AddRefed<GMPStorage> CreateGMPMemoryStorage(const nsACString& aNodeId,
                                                    const nsAString& aGMPName) {
  return RefPtr<GMPStorage>(new GMPMemoryStorage(aNodeId, aGMPName)).forget();
}

#undef LOG

}  // namespace mozilla::gmp
