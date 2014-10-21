/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gmp-test-decryptor.h"
#include "gmp-test-storage.h"

#include <string>
#include <vector>
#include <iostream>
#include <istream>
#include <iterator>
#include <sstream>
#include <assert.h>

#include "mozilla/Attributes.h"
#include "mozilla/NullPtr.h"

using namespace std;

FakeDecryptor* FakeDecryptor::sInstance = nullptr;

static bool sFinishedTruncateTest = false;
static bool sFinishedReplaceTest = false;
static bool sMultiClientTest = false;

void
MaybeFinish()
{
  if (sFinishedTruncateTest && sFinishedReplaceTest && sMultiClientTest) {
    FakeDecryptor::Message("test-storage complete");
  }
}

FakeDecryptor::FakeDecryptor(GMPDecryptorHost* aHost)
  : mHost(aHost)
  , mCallback(nullptr)
{
  assert(!sInstance);
  sInstance = this;
}

void FakeDecryptor::DecryptingComplete()
{
  sInstance = nullptr;
  delete this;
}

void
FakeDecryptor::Message(const std::string& aMessage)
{
  assert(sInstance);
  const static std::string sid("fake-session-id");
  sInstance->mCallback->SessionMessage(sid.c_str(), sid.size(),
                                       (const uint8_t*)aMessage.c_str(), aMessage.size(),
                                       nullptr, 0);
}

std::vector<std::string>
Tokenize(const std::string& aString)
{
  std::stringstream strstr(aString);
  std::istream_iterator<std::string> it(strstr), end;
  return std::vector<std::string>(it, end);
}

static const string TruncateRecordId = "truncate-record-id";
static const string TruncateRecordData = "I will soon be truncated";

class ReadThenTask : public GMPTask {
public:
  ReadThenTask(string aId, ReadContinuation* aThen)
    : mId(aId)
    , mThen(aThen)
  {}
  void Run() MOZ_OVERRIDE {
    ReadRecord(mId, mThen);
  }
  void Destroy() MOZ_OVERRIDE {
    delete this;
  }
  ReadContinuation* mThen;
  string mId;
};

class TestEmptyContinuation : public ReadContinuation {
public:
  void ReadComplete(GMPErr aErr, const std::string& aData) MOZ_OVERRIDE {
    if (aData != "") {
      FakeDecryptor::Message("FAIL TestEmptyContinuation record was not truncated");
    }
    sFinishedTruncateTest = true;
    MaybeFinish();
    delete this;
  }
};

class TruncateContinuation : public ReadContinuation {
public:
  void ReadComplete(GMPErr aErr, const std::string& aData) MOZ_OVERRIDE {
    if (aData != TruncateRecordData) {
      FakeDecryptor::Message("FAIL TruncateContinuation read data doesn't match written data");
    }
    WriteRecord(TruncateRecordId, nullptr, 0,
                new ReadThenTask(TruncateRecordId, new TestEmptyContinuation()));
    delete this;
  }
};

class VerifyAndFinishContinuation : public ReadContinuation {
public:
  VerifyAndFinishContinuation(string aValue)
    : mValue(aValue)
  {}
  void ReadComplete(GMPErr aErr, const std::string& aData) MOZ_OVERRIDE {
    if (aData != mValue) {
      FakeDecryptor::Message("FAIL VerifyAndFinishContinuation read data doesn't match expected data");
    }
    sFinishedReplaceTest = true;
    MaybeFinish();
    delete this;
  }
  string mValue;
};

class VerifyAndOverwriteContinuation : public ReadContinuation {
public:
  VerifyAndOverwriteContinuation(string aId, string aValue, string aOverwrite)
    : mId(aId)
    , mValue(aValue)
    , mOverwrite(aOverwrite)
  {}
  void ReadComplete(GMPErr aErr, const std::string& aData) MOZ_OVERRIDE {
    if (aData != mValue) {
      FakeDecryptor::Message("FAIL VerifyAndOverwriteContinuation read data doesn't match expected data");
    }
    WriteRecord(mId, mOverwrite, new ReadThenTask(mId, new VerifyAndFinishContinuation(mOverwrite)));
    delete this;
  }
  string mId;
  string mValue;
  string mOverwrite;
};

static const string OpenAgainRecordId = "open-again-record-id";

class OpenedSecondTimeContinuation : public OpenContinuation {
public:
  OpenedSecondTimeContinuation(GMPRecord* aRecord)
    : mRecord(aRecord)
  {
  }

  virtual void OpenComplete(GMPErr aStatus, GMPRecord* aRecord) MOZ_OVERRIDE {
    if (GMP_SUCCEEDED(aStatus)) {
      FakeDecryptor::Message("FAIL OpenSecondTimeContinuation should not be able to re-open record.");
    }

    // Succeeded, open should have failed.
    sMultiClientTest = true;
    MaybeFinish();

    mRecord->Close();

    delete this;
  }
  GMPRecord* mRecord;
};

class OpenedFirstTimeContinuation : public OpenContinuation {
public:
  virtual void OpenComplete(GMPErr aStatus, GMPRecord* aRecord) MOZ_OVERRIDE {
    if (GMP_FAILED(aStatus)) {
      FakeDecryptor::Message("FAIL OpenAgainContinuation to open record initially.");
      sMultiClientTest = true;
      MaybeFinish();
      return;
    }

    auto err = GMPOpenRecord(OpenAgainRecordId, new OpenedSecondTimeContinuation(aRecord));

    delete this;
  }
};

void
FakeDecryptor::TestStorage()
{
  // Basic I/O tests. We run three cases concurrently. The tests, like
  // GMPStorage run asynchronously. When they've all passed, we send
  // a message back to the parent process, or a failure message if not.

  // Test 1: Basic I/O test, and test that writing 0 bytes in a record
  // deletes record.
  //
  // Write data to truncate record, then
  // read data, verify that we read what we wrote, then
  // write 0 bytes to truncate record, then
  // read data, verify that 0 bytes was read
  // set sFinishedTruncateTest=true and MaybeFinish().
  WriteRecord(TruncateRecordId,
              TruncateRecordData,
              new ReadThenTask(TruncateRecordId, new TruncateContinuation()));

  // Test 2: Test that overwriting a record with a shorter record truncates
  // the record to the shorter record.
  //
  // Write record, then
  // read and verify record, then
  // write a shorter record to same record.
  // read and verify
  // set sFinishedReplaceTest=true and MaybeFinish().
  string id = "record1";
  string record1 = "This is the first write to a record.";
  string overwrite = "A shorter record";
  WriteRecord(id,
              record1,
              new ReadThenTask(id, new VerifyAndOverwriteContinuation(id, record1, overwrite)));

  // Test 3: Test that opening a record while it's already open fails.
  //
  // Open record1, then
  // open record1, should fail.
  // close record1,
  // set sMultiClientTest=true and MaybeFinish().

  GMPOpenRecord(OpenAgainRecordId, new OpenedFirstTimeContinuation());

  // Note: Once all tests finish, dispatch "test-pass" message,
  // which ends the test for the parent.
}

class ReportWritten : public GMPTask {
public:
  ReportWritten(const string& aRecordId, const string& aValue)
    : mRecordId(aRecordId)
    , mValue(aValue)
  {}
  void Run() MOZ_OVERRIDE {
    FakeDecryptor::Message("stored " + mRecordId + " " + mValue);
  }
  void Destroy() MOZ_OVERRIDE {
    delete this;
  }
  const string mRecordId;
  const string mValue;
};

class ReportReadStatusContinuation : public ReadContinuation {
public:
  ReportReadStatusContinuation(const string& aRecordId)
    : mRecordId(aRecordId)
  {}
  void ReadComplete(GMPErr aErr, const std::string& aData) MOZ_OVERRIDE {
    if (GMP_FAILED(aErr)) {
      FakeDecryptor::Message("retrieve " + mRecordId + " failed");
    } else {
      stringstream ss;
      ss << aData.size();
      string len;
      ss >> len;
      FakeDecryptor::Message("retrieve " + mRecordId + " succeeded (length " +
                             len + " bytes)");
    }
    delete this;
  }
  string mRecordId;
};

void
FakeDecryptor::UpdateSession(uint32_t aPromiseId,
                             const char* aSessionId,
                             uint32_t aSessionIdLength,
                             const uint8_t* aResponse,
                             uint32_t aResponseSize)
{
  std::string response((const char*)aResponse, (const char*)(aResponse)+aResponseSize);
  std::vector<std::string> tokens = Tokenize(response);
  const string& task = tokens[0];
  if (task == "test-storage") {
    TestStorage();
  } else if (task == "store") {
      // send "stored record" message on complete.
    const string& id = tokens[1];
    const string& value = tokens[2];
    WriteRecord(id,
                value,
                new ReportWritten(id, value));
  } else if (task == "retrieve") {
    const string& id = tokens[1];
    ReadRecord(id, new ReportReadStatusContinuation(id));
  }
}
