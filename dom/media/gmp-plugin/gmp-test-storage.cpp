/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gmp-test-storage.h"
#include <vector>

#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"

class WriteRecordClient : public GMPRecordClient {
public:
  GMPErr Init(GMPRecord* aRecord,
              GMPTask* aContinuation,
              const uint8_t* aData,
              uint32_t aDataSize) {
    mRecord = aRecord;
    mContinuation = aContinuation;
    mData.insert(mData.end(), aData, aData + aDataSize);
    return mRecord->Open();
  }

  virtual void OpenComplete(GMPErr aStatus) MOZ_OVERRIDE {
    mRecord->Write(&mData.front(), mData.size());
  }

  virtual void ReadComplete(GMPErr aStatus,
                            const uint8_t* aData,
                            uint32_t aDataSize) MOZ_OVERRIDE {}

  virtual void WriteComplete(GMPErr aStatus) MOZ_OVERRIDE {
    // Note: Call Close() before running continuation, in case the
    // continuation tries to open the same record; if we call Close()
    // after running the continuation, the Close() call will arrive
    // just after the Open() call succeeds, immediately closing the
    // record we just opened.
    mRecord->Close();
    if (mContinuation) {
      GMPRunOnMainThread(mContinuation);
    }
    delete this;
  }

private:
  GMPRecord* mRecord;
  GMPTask* mContinuation;
  std::vector<uint8_t> mData;
};

GMPErr
WriteRecord(const std::string& aRecordName,
            const uint8_t* aData,
            uint32_t aNumBytes,
            GMPTask* aContinuation)
{
  GMPRecord* record;
  WriteRecordClient* client = new WriteRecordClient();
  auto err = GMPOpenRecord(aRecordName.c_str(),
                           aRecordName.size(),
                           &record,
                           client);
  if (GMP_FAILED(err)) {
    return err;
  }
  return client->Init(record, aContinuation, aData, aNumBytes);
}

GMPErr
WriteRecord(const std::string& aRecordName,
            const std::string& aData,
            GMPTask* aContinuation)
{
  return WriteRecord(aRecordName,
                     (const uint8_t*)aData.c_str(),
                     aData.size(),
                     aContinuation);
}

class ReadRecordClient : public GMPRecordClient {
public:
  GMPErr Init(GMPRecord* aRecord,
              ReadContinuation* aContinuation) {
    mRecord = aRecord;
    mContinuation = aContinuation;
    return mRecord->Open();
  }

  virtual void OpenComplete(GMPErr aStatus) MOZ_OVERRIDE {
    auto err = mRecord->Read();
    if (GMP_FAILED(err)) {
      mContinuation->ReadComplete(err, "");
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
    std::string data((const char*)aData, aDataSize);
    mContinuation->ReadComplete(GMPNoErr, data);
    delete this;
  }

  virtual void WriteComplete(GMPErr aStatus) MOZ_OVERRIDE {
  }

private:
  GMPRecord* mRecord;
  ReadContinuation* mContinuation;
};

GMPErr
ReadRecord(const std::string& aRecordName,
           ReadContinuation* aContinuation)
{
  MOZ_ASSERT(aContinuation);
  GMPRecord* record;
  ReadRecordClient* client = new ReadRecordClient();
  auto err = GMPOpenRecord(aRecordName.c_str(),
                           aRecordName.size(),
                           &record,
                           client);
  if (GMP_FAILED(err)) {
    return err;
  }
  return client->Init(record, aContinuation);
}

extern GMPPlatformAPI* g_platform_api; // Defined in gmp-fake.cpp

GMPErr
GMPOpenRecord(const char* aName,
              uint32_t aNameLength,
              GMPRecord** aOutRecord,
              GMPRecordClient* aClient)
{
  MOZ_ASSERT(g_platform_api);
  return g_platform_api->createrecord(aName, aNameLength, aOutRecord, aClient);
}

GMPErr
GMPRunOnMainThread(GMPTask* aTask)
{
  MOZ_ASSERT(g_platform_api);
  return g_platform_api->runonmainthread(aTask);
}

class OpenRecordClient : public GMPRecordClient {
public:
  GMPErr Init(GMPRecord* aRecord,
              OpenContinuation* aContinuation) {
    mRecord = aRecord;
    mContinuation = aContinuation;
    return mRecord->Open();
  }

  virtual void OpenComplete(GMPErr aStatus) MOZ_OVERRIDE {
    mContinuation->OpenComplete(aStatus, mRecord);
    delete this;
  }

  virtual void ReadComplete(GMPErr aStatus,
                            const uint8_t* aData,
                            uint32_t aDataSize) MOZ_OVERRIDE { }

  virtual void WriteComplete(GMPErr aStatus) MOZ_OVERRIDE { }

private:
  GMPRecord* mRecord;
  OpenContinuation* mContinuation;
};

GMPErr
GMPOpenRecord(const std::string& aRecordName,
              OpenContinuation* aContinuation)
{
  MOZ_ASSERT(aContinuation);
  GMPRecord* record;
  OpenRecordClient* client = new OpenRecordClient();
  auto err = GMPOpenRecord(aRecordName.c_str(),
                           aRecordName.size(),
                           &record,
                           client);
  if (GMP_FAILED(err)) {
    return err;
  }
  return client->Init(record, aContinuation);
}

GMPErr
GMPEnumRecordNames(RecvGMPRecordIteratorPtr aRecvIteratorFunc,
                   void* aUserArg)
{
  return g_platform_api->getrecordenumerator(aRecvIteratorFunc, aUserArg);
}
