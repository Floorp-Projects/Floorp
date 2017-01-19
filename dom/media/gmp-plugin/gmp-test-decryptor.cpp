/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gmp-test-decryptor.h"
#include "gmp-test-storage.h"
#include "gmp-test-output-protection.h"

#include <string>
#include <vector>
#include <iostream>
#include <istream>
#include <iterator>
#include <sstream>
#include <set>

#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"

using namespace std;

std::string FakeDecryptor::sNodeId;

FakeDecryptor* FakeDecryptor::sInstance = nullptr;
extern GMPPlatformAPI* g_platform_api; // Defined in gmp-fake.cpp

class GMPMutexAutoLock
{
public:
  explicit GMPMutexAutoLock(GMPMutex* aMutex) : mMutex(aMutex) {
    mMutex->Acquire();
  }
  ~GMPMutexAutoLock() {
    mMutex->Release();
  }
private:
  GMPMutex* const mMutex;
};

class TestManager {
public:
  TestManager() : mMutex(CreateMutex()) {}

  // Register a test with the test manager.
  void BeginTest(const string& aTestID) {
    GMPMutexAutoLock lock(mMutex);
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
  void EndTest(const string& aTestID) {
    bool isEmpty = false;
    {
      GMPMutexAutoLock lock(mMutex);
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
  ~TestManager() {
    mMutex->Destroy();
  }

  static void Error(const string& msg) {
    FakeDecryptor::Message(msg);
  }

  static void Finish() {
    FakeDecryptor::Message("test-storage complete");
  }

  static GMPMutex* CreateMutex() {
    GMPMutex* mutex = nullptr;
    g_platform_api->createmutex(&mutex);
    return mutex;
  }

  GMPMutex* const mMutex;
  set<string> mTestIDs;
};

FakeDecryptor::FakeDecryptor(GMPDecryptorHost* aHost)
  : mCallback(nullptr)
  , mHost(aHost)
{
  MOZ_ASSERT(!sInstance);
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
  MOZ_ASSERT(sInstance);
  const static std::string sid("fake-session-id");
  sInstance->mCallback->SessionMessage(sid.c_str(), sid.size(),
                                       kGMPLicenseRequest,
                                       (const uint8_t*)aMessage.c_str(), aMessage.size());
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
  void Run() override {
    ReadRecord(mId, mThen);
  }
  void Destroy() override {
    delete this;
  }
  string mId;
  ReadContinuation* mThen;
};

class SendMessageTask : public GMPTask {
public:
  explicit SendMessageTask(const string& aMessage,
                           TestManager* aTestManager = nullptr,
                           const string& aTestID = "")
    : mMessage(aMessage), mTestmanager(aTestManager), mTestID(aTestID) {}

  void Run() override {
    FakeDecryptor::Message(mMessage);
    if (mTestmanager) {
      mTestmanager->EndTest(mTestID);
    }
  }

  void Destroy() override {
    delete this;
  }

private:
  string mMessage;
  TestManager* const mTestmanager;
  const string mTestID;
};

class TestEmptyContinuation : public ReadContinuation {
public:
  TestEmptyContinuation(TestManager* aTestManager, const string& aTestID)
    : mTestmanager(aTestManager), mTestID(aTestID) {}

  void ReadComplete(GMPErr aErr, const std::string& aData) override {
    if (aData != "") {
      FakeDecryptor::Message("FAIL TestEmptyContinuation record was not truncated");
    }
    mTestmanager->EndTest(mTestID);
    delete this;
  }

private:
  TestManager* const mTestmanager;
  const string mTestID;
};

class TruncateContinuation : public ReadContinuation {
public:
  TruncateContinuation(const string& aID,
                       TestManager* aTestManager,
                       const string& aTestID)
    : mID(aID), mTestmanager(aTestManager), mTestID(aTestID) {}

  void ReadComplete(GMPErr aErr, const std::string& aData) override {
    if (aData != TruncateRecordData) {
      FakeDecryptor::Message("FAIL TruncateContinuation read data doesn't match written data");
    }
    auto cont = new TestEmptyContinuation(mTestmanager, mTestID);
    auto msg = "FAIL in TruncateContinuation write.";
    auto failTask = new SendMessageTask(msg, mTestmanager, mTestID);
    WriteRecord(mID, nullptr, 0, new ReadThenTask(mID, cont), failTask);
    delete this;
  }

private:
  const string mID;
  TestManager* const mTestmanager;
  const string mTestID;
};

class VerifyAndFinishContinuation : public ReadContinuation {
public:
  explicit VerifyAndFinishContinuation(string aValue,
                                       TestManager* aTestManager,
                                       const string& aTestID)
  : mValue(aValue), mTestmanager(aTestManager), mTestID(aTestID) {}

  void ReadComplete(GMPErr aErr, const std::string& aData) override {
    if (aData != mValue) {
      FakeDecryptor::Message("FAIL VerifyAndFinishContinuation read data doesn't match expected data");
    }
    mTestmanager->EndTest(mTestID);
    delete this;
  }

private:
  string mValue;
  TestManager* const mTestmanager;
  const string mTestID;
};

class VerifyAndOverwriteContinuation : public ReadContinuation {
public:
  VerifyAndOverwriteContinuation(string aId, string aValue, string aOverwrite,
                                 TestManager* aTestManager, const string& aTestID)
    : mId(aId)
    , mValue(aValue)
    , mOverwrite(aOverwrite)
    , mTestmanager(aTestManager)
    , mTestID(aTestID)
  {}

  void ReadComplete(GMPErr aErr, const std::string& aData) override {
    if (aData != mValue) {
      FakeDecryptor::Message("FAIL VerifyAndOverwriteContinuation read data doesn't match expected data");
    }
    auto cont = new VerifyAndFinishContinuation(mOverwrite, mTestmanager, mTestID);
    auto msg = "FAIL in VerifyAndOverwriteContinuation write.";
    auto failTask = new SendMessageTask(msg, mTestmanager, mTestID);
    WriteRecord(mId, mOverwrite, new ReadThenTask(mId, cont), failTask);
    delete this;
  }

private:
  string mId;
  string mValue;
  string mOverwrite;
  TestManager* const mTestmanager;
  const string mTestID;
};

static const string OpenAgainRecordId = "open-again-record-id";

class OpenedSecondTimeContinuation : public OpenContinuation {
public:
  explicit OpenedSecondTimeContinuation(GMPRecord* aRecord,
                                        TestManager* aTestManager,
                                        const string& aTestID)
    : mRecord(aRecord), mTestmanager(aTestManager), mTestID(aTestID) {
    MOZ_ASSERT(aRecord);
  }

  void OpenComplete(GMPErr aStatus, GMPRecord* aRecord) override {
    if (GMP_SUCCEEDED(aStatus)) {
      FakeDecryptor::Message("FAIL OpenSecondTimeContinuation should not be able to re-open record.");
    }
    if (aRecord) {
      aRecord->Close();
    }
    // Succeeded, open should have failed.
    mTestmanager->EndTest(mTestID);
    mRecord->Close();
  }

private:
  GMPRecord* mRecord;
  TestManager* const mTestmanager;
  const string mTestID;
};

class OpenedFirstTimeContinuation : public OpenContinuation {
public:
  OpenedFirstTimeContinuation(const string& aID,
                              TestManager* aTestManager,
                              const string& aTestID)
    : mID(aID), mTestmanager(aTestManager), mTestID(aTestID) {}

  void OpenComplete(GMPErr aStatus, GMPRecord* aRecord) override {
    if (GMP_FAILED(aStatus)) {
      FakeDecryptor::Message("FAIL OpenAgainContinuation to open record initially.");
      mTestmanager->EndTest(mTestID);
      if (aRecord) {
        aRecord->Close();
      }
      return;
    }

    auto cont = new OpenedSecondTimeContinuation(aRecord, mTestmanager, mTestID);
    GMPOpenRecord(mID, cont);
  }

private:
  const string mID;
  TestManager* const mTestmanager;
  const string mTestID;
};

static void
DoTestStorage(const string& aPrefix, TestManager* aTestManager)
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
  const string id1 = aPrefix + TruncateRecordId;
  const string testID1 = aPrefix + "write-test-1";
  aTestManager->BeginTest(testID1);
  auto cont1 = new TruncateContinuation(id1, aTestManager, testID1);
  auto msg1 = "FAIL in TestStorage writing TruncateRecord.";
  auto failTask1 = new SendMessageTask(msg1, aTestManager, testID1);
  WriteRecord(id1, TruncateRecordData,
              new ReadThenTask(id1, cont1), failTask1);

  // Test 2: Test that overwriting a record with a shorter record truncates
  // the record to the shorter record.
  //
  // Write record, then
  // read and verify record, then
  // write a shorter record to same record.
  // read and verify
  string id2 = aPrefix + "record1";
  string record1 = "This is the first write to a record.";
  string overwrite = "A shorter record";
  const string testID2 = aPrefix + "write-test-2";
  aTestManager->BeginTest(testID2);
  auto task2 = new VerifyAndOverwriteContinuation(id2, record1, overwrite,
                                                  aTestManager, testID2);
  auto msg2 = "FAIL in TestStorage writing record1.";
  auto failTask2 = new SendMessageTask(msg2, aTestManager, testID2);
  WriteRecord(id2, record1, new ReadThenTask(id2, task2), failTask2);

  // Test 3: Test that opening a record while it's already open fails.
  //
  // Open record1, then
  // open record1, should fail.
  // close record1
  const string id3 = aPrefix + OpenAgainRecordId;
  const string testID3 = aPrefix + "open-test-1";
  aTestManager->BeginTest(testID3);
  auto task3 = new OpenedFirstTimeContinuation(id3, aTestManager, testID3);
  GMPOpenRecord(id3, task3);
}

class TestStorageTask : public GMPTask {
public:
  TestStorageTask(const string& aPrefix, TestManager* aTestManager)
    : mPrefix(aPrefix), mTestManager(aTestManager) {}
  void Destroy() override { delete this; }
  void Run() override {
    DoTestStorage(mPrefix, mTestManager);
  }
private:
  const string mPrefix;
  TestManager* const mTestManager;
};

void
FakeDecryptor::TestStorage()
{
  auto* testManager = new TestManager();
  GMPThread* thread1 = nullptr;
  GMPThread* thread2 = nullptr;

  // Main thread tests.
  DoTestStorage("mt1-", testManager);
  DoTestStorage("mt2-", testManager);

  // Off-main-thread tests.
  if (GMP_SUCCEEDED(g_platform_api->createthread(&thread1))) {
    thread1->Post(new TestStorageTask("thread1-", testManager));
  } else {
    FakeDecryptor::Message("FAIL to create thread1 for storage tests");
  }

  if (GMP_SUCCEEDED(g_platform_api->createthread(&thread2))) {
    thread2->Post(new TestStorageTask("thread2-", testManager));
  } else {
    FakeDecryptor::Message("FAIL to create thread2 for storage tests");
  }

  if (thread1) {
    thread1->Join();
  }

  if (thread2) {
    thread2->Join();
  }

  // Note: Once all tests finish, TestManager will dispatch "test-pass" message,
  // which ends the test for the parent.
}

class ReportWritten : public GMPTask {
public:
  ReportWritten(const string& aRecordId, const string& aValue)
    : mRecordId(aRecordId)
    , mValue(aValue)
  {}
  void Run() override {
    FakeDecryptor::Message("stored " + mRecordId + " " + mValue);
  }
  void Destroy() override {
    delete this;
  }
  const string mRecordId;
  const string mValue;
};

class ReportReadStatusContinuation : public ReadContinuation {
public:
  explicit ReportReadStatusContinuation(const string& aRecordId)
    : mRecordId(aRecordId)
  {}
  void ReadComplete(GMPErr aErr, const std::string& aData) override {
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

class ReportReadRecordContinuation : public ReadContinuation {
public:
  explicit ReportReadRecordContinuation(const string& aRecordId)
    : mRecordId(aRecordId)
  {}
  void ReadComplete(GMPErr aErr, const std::string& aData) override {
    if (GMP_FAILED(aErr)) {
      FakeDecryptor::Message("retrieved " + mRecordId + " failed");
    } else {
      FakeDecryptor::Message("retrieved " + mRecordId + " " + aData);
    }
    delete this;
  }
  string mRecordId;
};

static void
RecvGMPRecordIterator(GMPRecordIterator* aRecordIterator,
                      void* aUserArg,
                      GMPErr aStatus)
{
  FakeDecryptor* decryptor = reinterpret_cast<FakeDecryptor*>(aUserArg);
  decryptor->ProcessRecordNames(aRecordIterator, aStatus);
}

void
FakeDecryptor::ProcessRecordNames(GMPRecordIterator* aRecordIterator,
                                  GMPErr aStatus)
{
  if (sInstance != this) {
    FakeDecryptor::Message("Error aUserArg was not passed through GetRecordIterator");
    return;
  }
  if (GMP_FAILED(aStatus)) {
    FakeDecryptor::Message("Error GetRecordIterator failed");
    return;
  }
  std::string response("record-names ");
  bool first = true;
  const char* name = nullptr;
  uint32_t len = 0;
  while (GMP_SUCCEEDED(aRecordIterator->GetName(&name, &len))) {
    std::string s(name, name+len);
    if (!first) {
      response += ",";
    } else {
      first = false;
    }
    response += s;
    aRecordIterator->NextRecord();
  }
  aRecordIterator->Close();
  FakeDecryptor::Message(response);
}

enum ShutdownMode {
  ShutdownNormal,
  ShutdownTimeout,
  ShutdownStoreToken
};

static ShutdownMode sShutdownMode = ShutdownNormal;
static string sShutdownToken = "";

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
                new ReportWritten(id, value),
                new SendMessageTask("FAIL in writing record."));
  } else if (task == "retrieve") {
    const string& id = tokens[1];
    ReadRecord(id, new ReportReadStatusContinuation(id));
  } else if (task == "shutdown-mode") {
    const string& mode = tokens[1];
    if (mode == "timeout") {
      sShutdownMode = ShutdownTimeout;
    } else if (mode == "token") {
      sShutdownMode = ShutdownStoreToken;
      sShutdownToken = tokens[2];
      Message("shutdown-token received " + sShutdownToken);
    }
  } else if (task == "retrieve-shutdown-token") {
    ReadRecord("shutdown-token", new ReportReadRecordContinuation("shutdown-token"));
  } else if (task == "test-op-apis") {
    mozilla::gmptest::TestOuputProtectionAPIs();
  } else if (task == "retrieve-plugin-voucher") {
    const uint8_t* rawVoucher = nullptr;
    uint32_t length = 0;
    mHost->GetPluginVoucher(&rawVoucher, &length);
    std::string voucher((const char*)rawVoucher, (const char*)(rawVoucher + length));
    Message("retrieved plugin-voucher: " + voucher);
  } else if (task == "retrieve-record-names") {
    GMPEnumRecordNames(&RecvGMPRecordIterator, this);
  } else if (task == "retrieve-node-id") {
    Message("node-id " + sNodeId);
  }
}
