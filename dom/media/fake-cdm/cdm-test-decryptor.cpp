/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "cdm-test-decryptor.h"
#include "cdm-test-storage.h"
#include "cdm-test-output-protection.h"

#include <mutex>
#include <string>
#include <vector>
#include <iostream>
#include <istream>
#include <iterator>
#include <sstream>
#include <set>
#include <thread>

#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"

FakeDecryptor* FakeDecryptor::sInstance = nullptr;

class TestManager {
 public:
  TestManager() = default;

  // Register a test with the test manager.
  void BeginTest(const std::string& aTestID) {
    std::lock_guard<std::mutex> lock(mMutex);
    auto found = mTestIDs.find(aTestID);
    if (found == mTestIDs.end()) {
      mTestIDs.insert(aTestID);
    } else {
      Error("FAIL BeginTest test already existed: " + aTestID);
    }
  }

  // Notify the test manager that the test is finished. If all tests are done,
  // test manager will send "test-storage complete" to notify the parent that
  // all tests are finished and also delete itself.
  void EndTest(const std::string& aTestID) {
    bool isEmpty = false;
    {
      std::lock_guard<std::mutex> lock(mMutex);
      auto found = mTestIDs.find(aTestID);
      if (found != mTestIDs.end()) {
        mTestIDs.erase(aTestID);
        isEmpty = mTestIDs.empty();
      } else {
        Error("FAIL EndTest test not existed: " + aTestID);
        return;
      }
    }
    if (isEmpty) {
      Finish();
      delete this;
    }
  }

 private:
  ~TestManager() = default;

  static void Error(const std::string& msg) { FakeDecryptor::Message(msg); }

  static void Finish() { FakeDecryptor::Message("test-storage complete"); }

  std::mutex mMutex;
  std::set<std::string> mTestIDs;
};

FakeDecryptor::FakeDecryptor(cdm::Host_10* aHost) : mHost(aHost) {
  MOZ_ASSERT(!sInstance);
  sInstance = this;
}

void FakeDecryptor::Message(const std::string& aMessage) {
  MOZ_ASSERT(sInstance);
  const static std::string sid("fake-session-id");
  sInstance->mHost->OnSessionMessage(sid.c_str(), sid.size(),
                                     cdm::MessageType::kLicenseRequest,
                                     aMessage.c_str(), aMessage.size());
}

std::vector<std::string> Tokenize(const std::string& aString) {
  std::stringstream strstr(aString);
  std::istream_iterator<std::string> it(strstr), end;
  return std::vector<std::string>(it, end);
}

static const std::string TruncateRecordId = "truncate-record-id";
static const std::string TruncateRecordData = "I will soon be truncated";

template <class Continuation>
class WriteRecordSuccessTask {
 public:
  WriteRecordSuccessTask(std::string aId, Continuation aThen)
      : mId(aId), mThen(std::move(aThen)) {}

  void operator()() { ReadRecord(FakeDecryptor::sInstance->mHost, mId, mThen); }

  std::string mId;
  Continuation mThen;
};

class WriteRecordFailureTask {
 public:
  explicit WriteRecordFailureTask(const std::string& aMessage,
                                  TestManager* aTestManager = nullptr,
                                  const std::string& aTestID = "")
      : mMessage(aMessage), mTestmanager(aTestManager), mTestID(aTestID) {}

  void operator()() {
    FakeDecryptor::Message(mMessage);
    if (mTestmanager) {
      mTestmanager->EndTest(mTestID);
    }
  }

 private:
  std::string mMessage;
  TestManager* const mTestmanager;
  const std::string mTestID;
};

class TestEmptyContinuation : public ReadContinuation {
 public:
  TestEmptyContinuation(TestManager* aTestManager, const std::string& aTestID)
      : mTestmanager(aTestManager), mTestID(aTestID) {}

  virtual void operator()(bool aSuccess, const uint8_t* aData,
                          uint32_t aDataSize) override {
    if (aDataSize) {
      FakeDecryptor::Message(
          "FAIL TestEmptyContinuation record was not truncated");
    }
    mTestmanager->EndTest(mTestID);
  }

 private:
  TestManager* const mTestmanager;
  const std::string mTestID;
};

class TruncateContinuation : public ReadContinuation {
 public:
  TruncateContinuation(const std::string& aID, TestManager* aTestManager,
                       const std::string& aTestID)
      : mID(aID), mTestmanager(aTestManager), mTestID(aTestID) {}

  virtual void operator()(bool aSuccess, const uint8_t* aData,
                          uint32_t aDataSize) override {
    if (std::string(reinterpret_cast<const char*>(aData), aDataSize) !=
        TruncateRecordData) {
      FakeDecryptor::Message(
          "FAIL TruncateContinuation read data doesn't match written data");
    }
    auto cont = TestEmptyContinuation(mTestmanager, mTestID);
    auto msg = "FAIL in TruncateContinuation write.";
    WriteRecord(FakeDecryptor::sInstance->mHost, mID, nullptr, 0,
                WriteRecordSuccessTask<TestEmptyContinuation>(mID, cont),
                WriteRecordFailureTask(msg, mTestmanager, mTestID));
  }

 private:
  const std::string mID;
  TestManager* const mTestmanager;
  const std::string mTestID;
};

class VerifyAndFinishContinuation : public ReadContinuation {
 public:
  explicit VerifyAndFinishContinuation(std::string aValue,
                                       TestManager* aTestManager,
                                       const std::string& aTestID)
      : mValue(aValue), mTestmanager(aTestManager), mTestID(aTestID) {}

  virtual void operator()(bool aSuccess, const uint8_t* aData,
                          uint32_t aDataSize) override {
    if (std::string(reinterpret_cast<const char*>(aData), aDataSize) !=
        mValue) {
      FakeDecryptor::Message(
          "FAIL VerifyAndFinishContinuation read data doesn't match expected "
          "data");
    }
    mTestmanager->EndTest(mTestID);
  }

 private:
  std::string mValue;
  TestManager* const mTestmanager;
  const std::string mTestID;
};

class VerifyAndOverwriteContinuation : public ReadContinuation {
 public:
  VerifyAndOverwriteContinuation(std::string aId, std::string aValue,
                                 std::string aOverwrite,
                                 TestManager* aTestManager,
                                 const std::string& aTestID)
      : mId(aId),
        mValue(aValue),
        mOverwrite(aOverwrite),
        mTestmanager(aTestManager),
        mTestID(aTestID) {}

  virtual void operator()(bool aSuccess, const uint8_t* aData,
                          uint32_t aDataSize) override {
    if (std::string(reinterpret_cast<const char*>(aData), aDataSize) !=
        mValue) {
      FakeDecryptor::Message(
          "FAIL VerifyAndOverwriteContinuation read data doesn't match "
          "expected data");
    }
    auto cont = VerifyAndFinishContinuation(mOverwrite, mTestmanager, mTestID);
    auto msg = "FAIL in VerifyAndOverwriteContinuation write.";
    WriteRecord(FakeDecryptor::sInstance->mHost, mId, mOverwrite,
                WriteRecordSuccessTask<VerifyAndFinishContinuation>(mId, cont),
                WriteRecordFailureTask(msg, mTestmanager, mTestID));
  }

 private:
  std::string mId;
  std::string mValue;
  std::string mOverwrite;
  TestManager* const mTestmanager;
  const std::string mTestID;
};

static const std::string OpenAgainRecordId = "open-again-record-id";

class OpenedSecondTimeContinuation : public OpenContinuation {
 public:
  explicit OpenedSecondTimeContinuation(TestManager* aTestManager,
                                        const std::string& aTestID)
      : mTestmanager(aTestManager), mTestID(aTestID) {}

  void operator()(bool aSuccess) override {
    if (!aSuccess) {
      FakeDecryptor::Message(
          "FAIL OpenSecondTimeContinuation should not be able to re-open "
          "record.");
    }
    // Succeeded, open should have failed.
    mTestmanager->EndTest(mTestID);
  }

 private:
  TestManager* const mTestmanager;
  const std::string mTestID;
};

class OpenedFirstTimeContinuation : public OpenContinuation {
 public:
  OpenedFirstTimeContinuation(const std::string& aID, TestManager* aTestManager,
                              const std::string& aTestID)
      : mID(aID), mTestmanager(aTestManager), mTestID(aTestID) {}

  void operator()(bool aSuccess) override {
    if (!aSuccess) {
      FakeDecryptor::Message(
          "FAIL OpenAgainContinuation to open record initially.");
      mTestmanager->EndTest(mTestID);
      return;
    }

    auto cont = OpenedSecondTimeContinuation(mTestmanager, mTestID);
    OpenRecord(FakeDecryptor::sInstance->mHost, mID, cont);
  }

 private:
  const std::string mID;
  TestManager* const mTestmanager;
  const std::string mTestID;
};

static void DoTestStorage(const std::string& aPrefix,
                          TestManager* aTestManager) {
  MOZ_ASSERT(FakeDecryptor::sInstance->mHost,
             "FakeDecryptor::sInstance->mHost should not be null");
  // Basic I/O tests. We run three cases concurrently. The tests, like
  // CDMStorage run asynchronously. When they've all passed, we send
  // a message back to the parent process, or a failure message if not.

  // Test 1: Basic I/O test, and test that writing 0 bytes in a record
  // deletes record.
  //
  // Write data to truncate record, then
  // read data, verify that we read what we wrote, then
  // write 0 bytes to truncate record, then
  // read data, verify that 0 bytes was read
  const std::string id1 = aPrefix + TruncateRecordId;
  const std::string testID1 = aPrefix + "write-test-1";
  aTestManager->BeginTest(testID1);
  auto cont1 = TruncateContinuation(id1, aTestManager, testID1);
  auto msg1 = "FAIL in TestStorage writing TruncateRecord.";
  WriteRecord(FakeDecryptor::sInstance->mHost, id1, TruncateRecordData,
              WriteRecordSuccessTask<TruncateContinuation>(id1, cont1),
              WriteRecordFailureTask(msg1, aTestManager, testID1));

  // Test 2: Test that overwriting a record with a shorter record truncates
  // the record to the shorter record.
  //
  // Write record, then
  // read and verify record, then
  // write a shorter record to same record.
  // read and verify
  std::string id2 = aPrefix + "record1";
  std::string record1 = "This is the first write to a record.";
  std::string overwrite = "A shorter record";
  const std::string testID2 = aPrefix + "write-test-2";
  aTestManager->BeginTest(testID2);
  auto task2 = VerifyAndOverwriteContinuation(id2, record1, overwrite,
                                              aTestManager, testID2);
  auto msg2 = "FAIL in TestStorage writing record1.";
  WriteRecord(
      FakeDecryptor::sInstance->mHost, id2, record1,
      WriteRecordSuccessTask<VerifyAndOverwriteContinuation>(id2, task2),
      WriteRecordFailureTask(msg2, aTestManager, testID2));

  // Test 3: Test that opening a record while it's already open fails.
  //
  // Open record1, then
  // open record1, should fail.
  // close record1
  const std::string id3 = aPrefix + OpenAgainRecordId;
  const std::string testID3 = aPrefix + "open-test-1";
  aTestManager->BeginTest(testID3);
  auto task3 = OpenedFirstTimeContinuation(id3, aTestManager, testID3);
  OpenRecord(FakeDecryptor::sInstance->mHost, id3, task3);
}

void FakeDecryptor::TestStorage() {
  auto* testManager = new TestManager();
  // Main thread tests.
  DoTestStorage("mt1-", testManager);
  DoTestStorage("mt2-", testManager);

  // Note: Once all tests finish, TestManager will dispatch "test-pass" message,
  // which ends the test for the parent.
}

class ReportWritten {
 public:
  ReportWritten(const std::string& aRecordId, const std::string& aValue)
      : mRecordId(aRecordId), mValue(aValue) {}
  void operator()() {
    FakeDecryptor::Message("stored " + mRecordId + " " + mValue);
  }

  const std::string mRecordId;
  const std::string mValue;
};

class ReportReadStatusContinuation : public ReadContinuation {
 public:
  explicit ReportReadStatusContinuation(const std::string& aRecordId)
      : mRecordId(aRecordId) {}
  void operator()(bool aSuccess, const uint8_t* aData,
                  uint32_t aDataSize) override {
    if (!aSuccess) {
      FakeDecryptor::Message("retrieve " + mRecordId + " failed");
    } else {
      std::stringstream ss;
      ss << aDataSize;
      std::string len;
      ss >> len;
      FakeDecryptor::Message("retrieve " + mRecordId + " succeeded (length " +
                             len + " bytes)");
    }
  }
  std::string mRecordId;
};

class ReportReadRecordContinuation : public ReadContinuation {
 public:
  explicit ReportReadRecordContinuation(const std::string& aRecordId)
      : mRecordId(aRecordId) {}
  void operator()(bool aSuccess, const uint8_t* aData,
                  uint32_t aDataSize) override {
    if (!aSuccess) {
      FakeDecryptor::Message("retrieved " + mRecordId + " failed");
    } else {
      FakeDecryptor::Message(
          "retrieved " + mRecordId + " " +
          std::string(reinterpret_cast<const char*>(aData), aDataSize));
    }
  }
  std::string mRecordId;
};

enum ShutdownMode { ShutdownNormal, ShutdownTimeout, ShutdownStoreToken };

static ShutdownMode sShutdownMode = ShutdownNormal;
static std::string sShutdownToken;

void FakeDecryptor::UpdateSession(uint32_t aPromiseId, const char* aSessionId,
                                  uint32_t aSessionIdLength,
                                  const uint8_t* aResponse,
                                  uint32_t aResponseSize) {
  MOZ_ASSERT(FakeDecryptor::sInstance->mHost,
             "FakeDecryptor::sInstance->mHost should not be null");
  std::string response((const char*)aResponse,
                       (const char*)(aResponse) + aResponseSize);
  std::vector<std::string> tokens = Tokenize(response);
  const std::string& task = tokens[0];
  if (task == "test-storage") {
    TestStorage();
  } else if (task == "store") {
    // send "stored record" message on complete.
    const std::string& id = tokens[1];
    const std::string& value = tokens[2];
    WriteRecord(FakeDecryptor::sInstance->mHost, id, value,
                ReportWritten(id, value),
                WriteRecordFailureTask("FAIL in writing record."));
  } else if (task == "retrieve") {
    const std::string& id = tokens[1];
    ReadRecord(FakeDecryptor::sInstance->mHost, id,
               ReportReadStatusContinuation(id));
  } else if (task == "shutdown-mode") {
    const std::string& mode = tokens[1];
    if (mode == "timeout") {
      sShutdownMode = ShutdownTimeout;
    } else if (mode == "token") {
      sShutdownMode = ShutdownStoreToken;
      sShutdownToken = tokens[2];
      Message("shutdown-token received " + sShutdownToken);
    }
  } else if (task == "retrieve-shutdown-token") {
    ReadRecord(FakeDecryptor::sInstance->mHost, "shutdown-token",
               ReportReadRecordContinuation("shutdown-token"));
  } else if (task == "test-op-apis") {
    mozilla::cdmtest::TestOuputProtectionAPIs();
  }
}
