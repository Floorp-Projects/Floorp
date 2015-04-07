/*
 * Copyright 2015, Mozilla Foundation and contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "ClearKeyStorage.h"
#include "ClearKeyUtils.h"

#include "gmp-task-utils.h"

#include <assert.h>
#include "ArrayUtils.h"

#include <vector>

static GMPErr
RunOnMainThread(GMPTask* aTask)
{
  return GetPlatform()->runonmainthread(aTask);
}

GMPErr
OpenRecord(const char* aName,
           uint32_t aNameLength,
           GMPRecord** aOutRecord,
           GMPRecordClient* aClient)
{
  return GetPlatform()->createrecord(aName, aNameLength, aOutRecord, aClient);
}

class WriteRecordClient : public GMPRecordClient {
public:
  /*
   * This function will take the memory ownership of the parameters and
   * delete them when done.
   */
  static void Write(const std::string& aRecordName,
                    const std::vector<uint8_t>& aData,
                    GMPTask* aOnSuccess,
                    GMPTask* aOnFailure) {
    (new WriteRecordClient(aData, aOnSuccess, aOnFailure))->Do(aRecordName);
  }

  virtual void OpenComplete(GMPErr aStatus) override {
    if (GMP_FAILED(aStatus) ||
        GMP_FAILED(mRecord->Write(&mData.front(), mData.size()))) {
      Done(mOnFailure, mOnSuccess);
    }
  }

  virtual void ReadComplete(GMPErr aStatus,
                            const uint8_t* aData,
                            uint32_t aDataSize) override {
    assert(false); // Should not reach here.
  }

  virtual void WriteComplete(GMPErr aStatus) override {
    if (GMP_FAILED(aStatus)) {
      Done(mOnFailure, mOnSuccess);
    } else {
      Done(mOnSuccess, mOnFailure);
    }
  }

private:
  WriteRecordClient(const std::vector<uint8_t>& aData,
                    GMPTask* aOnSuccess,
                    GMPTask* aOnFailure)
    : mRecord(nullptr)
    , mOnSuccess(aOnSuccess)
    , mOnFailure(aOnFailure)
    , mData(aData) {}

  void Do(const std::string& aName) {
    auto err = OpenRecord(aName.c_str(), aName.size(), &mRecord, this);
    if (GMP_FAILED(err) ||
        GMP_FAILED(mRecord->Open())) {
      Done(mOnFailure, mOnSuccess);
    }
  }

  void Done(GMPTask* aToRun, GMPTask* aToDestroy) {
    // Note: Call Close() before running continuation, in case the
    // continuation tries to open the same record; if we call Close()
    // after running the continuation, the Close() call will arrive
    // just after the Open() call succeeds, immediately closing the
    // record we just opened.
    if (mRecord) {
      mRecord->Close();
    }
    aToDestroy->Destroy();
    RunOnMainThread(aToRun);
    delete this;
  }

  GMPRecord* mRecord;
  GMPTask* mOnSuccess;
  GMPTask* mOnFailure;
  const std::vector<uint8_t> mData;
};

void
StoreData(const std::string& aRecordName,
          const std::vector<uint8_t>& aData,
          GMPTask* aOnSuccess,
          GMPTask* aOnFailure)
{
  WriteRecordClient::Write(aRecordName, aData, aOnSuccess, aOnFailure);
}

class ReadRecordClient : public GMPRecordClient {
public:
  /*
   * This function will take the memory ownership of the parameters and
   * delete them when done.
   */
  static void Read(const std::string& aRecordName,
                   ReadContinuation* aContinuation) {
    assert(aContinuation);
    (new ReadRecordClient(aContinuation))->Do(aRecordName);
  }

  virtual void OpenComplete(GMPErr aStatus) override {
    auto err = aStatus;
    if (GMP_FAILED(err) ||
        GMP_FAILED(err = mRecord->Read())) {
      Done(err, nullptr, 0);
    }
  }

  virtual void ReadComplete(GMPErr aStatus,
                            const uint8_t* aData,
                            uint32_t aDataSize) override {
    Done(aStatus, aData, aDataSize);
  }

  virtual void WriteComplete(GMPErr aStatus) override {
    assert(false); // Should not reach here.
  }

private:
  explicit ReadRecordClient(ReadContinuation* aContinuation)
    : mRecord(nullptr)
    , mContinuation(aContinuation) {}

  void Do(const std::string& aName) {
    auto err = OpenRecord(aName.c_str(), aName.size(), &mRecord, this);
    if (GMP_FAILED(err) ||
        GMP_FAILED(err = mRecord->Open())) {
      Done(err, nullptr, 0);
    }
  }

  void Done(GMPErr err, const uint8_t* aData, uint32_t aDataSize) {
    // Note: Call Close() before running continuation, in case the
    // continuation tries to open the same record; if we call Close()
    // after running the continuation, the Close() call will arrive
    // just after the Open() call succeeds, immediately closing the
    // record we just opened.
    if (mRecord) {
      mRecord->Close();
    }
    mContinuation->ReadComplete(err, aData, aDataSize);
    delete mContinuation;
    delete this;
  }

  GMPRecord* mRecord;
  ReadContinuation* mContinuation;
};

void
ReadData(const std::string& aRecordName,
         ReadContinuation* aContinuation)
{
  ReadRecordClient::Read(aRecordName, aContinuation);
}

GMPErr
EnumRecordNames(RecvGMPRecordIteratorPtr aRecvIteratorFunc)
{
  return GetPlatform()->getrecordenumerator(aRecvIteratorFunc, nullptr);
}
