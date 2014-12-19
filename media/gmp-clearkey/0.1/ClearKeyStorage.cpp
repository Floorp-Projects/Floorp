/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ClearKeyStorage.h"
#include "ClearKeyUtils.h"

#include "gmp-task-utils.h"

#include "mozilla/Assertions.h"
#include "mozilla/NullPtr.h"
#include "mozilla/ArrayUtils.h"

#include <vector>

static GMPErr
RunOnMainThread(GMPTask* aTask)
{
  return GetPlatform()->runonmainthread(aTask);
}

class WriteRecordClient : public GMPRecordClient {
public:
  GMPErr Init(GMPRecord* aRecord,
              GMPTask* aOnSuccess,
              GMPTask* aOnFailure,
              const uint8_t* aData,
              uint32_t aDataSize) {
    mRecord = aRecord;
    mOnSuccess = aOnSuccess;
    mOnFailure = aOnFailure;
    mData.insert(mData.end(), aData, aData + aDataSize);
    return mRecord->Open();
  }

  virtual void OpenComplete(GMPErr aStatus) MOZ_OVERRIDE {
    if (GMP_FAILED(aStatus) ||
        GMP_FAILED(mRecord->Write(&mData.front(), mData.size()))) {
      RunOnMainThread(mOnFailure);
      mOnSuccess->Destroy();
    }
  }

  virtual void ReadComplete(GMPErr aStatus,
                            const uint8_t* aData,
                            uint32_t aDataSize) MOZ_OVERRIDE {
    MOZ_ASSERT(false, "Should not reach here.");
  }

  virtual void WriteComplete(GMPErr aStatus) MOZ_OVERRIDE {
    // Note: Call Close() before running continuation, in case the
    // continuation tries to open the same record; if we call Close()
    // after running the continuation, the Close() call will arrive
    // just after the Open() call succeeds, immediately closing the
    // record we just opened.
    mRecord->Close();
    if (GMP_SUCCEEDED(aStatus)) {
      RunOnMainThread(mOnSuccess);
      mOnFailure->Destroy();
    } else {
      RunOnMainThread(mOnFailure);
      mOnSuccess->Destroy();
    }
    delete this;
  }

private:
  GMPRecord* mRecord;
  GMPTask* mOnSuccess;
  GMPTask* mOnFailure;
  std::vector<uint8_t> mData;
};

GMPErr
OpenRecord(const char* aName,
           uint32_t aNameLength,
           GMPRecord** aOutRecord,
           GMPRecordClient* aClient)
{
  return GetPlatform()->createrecord(aName, aNameLength, aOutRecord, aClient);
}

void
StoreData(const std::string& aRecordName,
          const std::vector<uint8_t>& aData,
          GMPTask* aOnSuccess,
          GMPTask* aOnFailure)
{
  GMPRecord* record;
  WriteRecordClient* client = new WriteRecordClient();
  if (GMP_FAILED(OpenRecord(aRecordName.c_str(),
                            aRecordName.size(),
                            &record,
                            client)) ||
      GMP_FAILED(client->Init(record,
                              aOnSuccess,
                              aOnFailure,
                              &aData.front(),
                              aData.size()))) {
    RunOnMainThread(aOnFailure);
    aOnSuccess->Destroy();
  }
}

class ReadRecordClient : public GMPRecordClient {
public:
  ReadRecordClient()
    : mRecord(nullptr)
    , mContinuation(nullptr)
  {}
  ~ReadRecordClient() {
    delete mContinuation;
  }

  GMPErr Init(GMPRecord* aRecord,
              ReadContinuation* aContinuation) {
    mRecord = aRecord;
    mContinuation = aContinuation;
    return mRecord->Open();
  }

  virtual void OpenComplete(GMPErr aStatus) MOZ_OVERRIDE {
    auto err = mRecord->Read();
    if (GMP_FAILED(err)) {
      mContinuation->ReadComplete(err, nullptr, 0);
      delete this;
    }
  }

  virtual void ReadComplete(GMPErr aStatus,
                            const uint8_t* aData,
                            uint32_t aDataSize) MOZ_OVERRIDE {
    // Note: Call Close() before running continuation, in case the
    // continuation tries to open the same record; if we call Close()
    // after running the continuation, the Close() call will arrive
    // just after the Open() call succeeds, immediately closing the
    // record we just opened.
    mRecord->Close();
    mContinuation->ReadComplete(GMPNoErr, aData, aDataSize);
    delete this;
  }

  virtual void WriteComplete(GMPErr aStatus) MOZ_OVERRIDE {
    MOZ_ASSERT(false, "Should not reach here.");
  }

private:
  GMPRecord* mRecord;
  ReadContinuation* mContinuation;
};

void
ReadData(const std::string& aRecordName,
         ReadContinuation* aContinuation)
{
  MOZ_ASSERT(aContinuation);
  GMPRecord* record;
  ReadRecordClient* client = new ReadRecordClient();
  auto err = OpenRecord(aRecordName.c_str(),
                        aRecordName.size(),
                        &record,
                        client);
  if (GMP_FAILED(err) ||
      GMP_FAILED(client->Init(record, aContinuation))) {
    aContinuation->ReadComplete(err, nullptr, 0);
    delete aContinuation;
  }
}

GMPErr
EnumRecordNames(RecvGMPRecordIteratorPtr aRecvIteratorFunc)
{
  return GetPlatform()->getrecordenumerator(aRecvIteratorFunc, nullptr);
}
