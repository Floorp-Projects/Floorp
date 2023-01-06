/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileSystemQuotaClient.h"
#include "GetDirectoryForOrigin.h"
#include "QuotaManagerDependencyFixture.h"
#include "TestHelpers.h"
#include "datamodel/FileSystemDataManager.h"
#include "datamodel/FileSystemDatabaseManager.h"
#include "datamodel/FileSystemFileManager.h"
#include "gtest/gtest.h"
#include "mozIStorageService.h"
#include "mozStorageCID.h"
#include "mozilla/SpinEventLoopUntil.h"
#include "mozilla/dom/PFileSystemManager.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/dom/quota/FileStreams.h"
#include "mozilla/dom/quota/QuotaCommon.h"
#include "mozilla/dom/quota/QuotaManager.h"
#include "mozilla/dom/quota/QuotaManagerService.h"
#include "mozilla/dom/quota/ResultExtensions.h"
#include "mozilla/dom/quota/UsageInfo.h"
#include "mozilla/gtest/MozAssertions.h"
#include "nsIFile.h"
#include "nsIFileURL.h"
#include "nsIPrefBranch.h"
#include "nsIPrefService.h"
#include "nsIQuotaCallbacks.h"
#include "nsIQuotaRequests.h"
#include "nsNetUtil.h"
#include "nsScriptSecurityManager.h"

// Promise returning test task helpers
#define TEST_PROMISE_ASSERT_CHECK_META(didFail, someAssert, ...)     \
  bool MOZ_REMOVE_PAREN(didFail) = true;                             \
  [&] {                                                              \
    MOZ_REMOVE_PAREN(someAssert)(__VA_ARGS__);                       \
    MOZ_REMOVE_PAREN(didFail) = false;                               \
  }();                                                               \
  if (MOZ_REMOVE_PAREN(didFail)) {                                   \
    return BoolPromise::CreateAndReject(NS_ERROR_FAILURE, __func__); \
  }

#define TEST_PROMISE_ASSERT_CHECK(...) \
  TEST_PROMISE_ASSERT_CHECK_META(MOZ_UNIQUE_VAR(testVar), __VA_ARGS__)

// Promise returning test assertions
#define TEST_PROMISE_ASSERT_TRUE(expr) \
  TEST_PROMISE_ASSERT_CHECK(ASSERT_TRUE, expr)

#define TEST_PROMISE_ASSERT_NSEQ(lhs, rhs) \
  TEST_PROMISE_ASSERT_CHECK(ASSERT_NSEQ, lhs, rhs)

#define TEST_PROMISE_ASSERT_SUCCESS(rhs) TEST_PROMISE_ASSERT_NSEQ(NS_OK, rhs)

#define TEST_TRY_PROMISE_UNWRAP_META(tempVar, target, expr)           \
  auto MOZ_REMOVE_PAREN(tempVar) = (expr);                            \
  if (!MOZ_REMOVE_PAREN(tempVar).isOk()) {                            \
    EXPECT_TRUE(MOZ_REMOVE_PAREN(tempVar).isOk());                    \
    return BoolPromise::CreateAndReject(                              \
        ToNSResult(MOZ_REMOVE_PAREN(tempVar).unwrapErr()), __func__); \
  }                                                                   \
  MOZ_REMOVE_PAREN(target) = MOZ_REMOVE_PAREN(tempVar).unwrap();

#define TEST_TRY_PROMISE_UNWRAP(target, expr) \
  TEST_TRY_PROMISE_UNWRAP_META(MOZ_UNIQUE_VAR(testVar), target, expr)

namespace mozilla::dom::fs::test {

quota::OriginMetadata GetTestQuotaOriginMetadata() {
  return quota::OriginMetadata{""_ns, "quotaexample.com"_ns,
                               "http://quotaexample.com"_ns,
                               quota::PERSISTENCE_TYPE_DEFAULT};
}

class TestFileSystemQuotaClient : public QuotaManagerDependencyFixture {
 public:
  // ExceedsPreallocation value may depend on platform and sqlite version!
  static const int sExceedsPreallocation = 32 * 1024;

  const quota::OriginMetadata& GetOriginMetadata() const override {
    static const auto& testMetadata = GetTestQuotaOriginMetadata();

    return testMetadata;
  }

 protected:
  void TearDown() override {
    PerformOnIOThread([]() {
      // We use QM_TRY here to avoid failures if this cleanup is unnecessary
      const auto& originMeta = GetTestQuotaOriginMetadata();

      QM_TRY_INSPECT(const nsCOMPtr<nsIFile>& dbFile,
                     data::GetDatabaseFile(originMeta.mOrigin), QM_VOID);
      ASSERT_TRUE(dbFile);

      bool exists = false;
      QM_TRY(MOZ_TO_RESULT(dbFile->Exists(&exists)), QM_VOID);
      QM_TRY(OkIf(exists), QM_VOID);

#if FS_QUOTA_MANAGEMENT_ENABLED
      int64_t dbSize = 0;
      QM_TRY(MOZ_TO_RESULT(dbFile->GetFileSize(&dbSize)), QM_VOID);

      quota::QuotaManager* qm = quota::QuotaManager::Get();
      QM_TRY(OkIf(qm), QM_VOID);
      qm->DecreaseUsageForClient(
          quota::ClientMetadata{originMeta, quota::Client::FILESYSTEM}, dbSize);
#endif

      QM_WARNONLY_TRY(MOZ_TO_RESULT(dbFile->Remove(/* recursive */ false)));

      exists = true;
      QM_TRY(MOZ_TO_RESULT(dbFile->Exists(&exists)), QM_VOID);
      ASSERT_FALSE(exists);
    });

    QuotaManagerDependencyFixture::TearDown();
  }

  static void InitializeStorage() {
    auto backgroundTask = []() -> RefPtr<BoolPromise> {
      quota::QuotaManager* quotaManager = quota::QuotaManager::Get();
      TEST_PROMISE_ASSERT_TRUE(quotaManager);

      quotaManager->IOThread()->Dispatch(
          NS_NewRunnableFunction(
              "TestFileSystemQuotaClient",
              []() {
                quota::QuotaManager* qm = quota::QuotaManager::Get();
                ASSERT_TRUE(qm);

                ASSERT_NSEQ(NS_OK, qm->EnsureStorageIsInitialized());

                ASSERT_NSEQ(NS_OK, qm->EnsureTemporaryStorageIsInitialized());

                const quota::OriginMetadata& testOriginMeta =
                    GetTestQuotaOriginMetadata();

                auto dirInfoRes = qm->EnsureTemporaryOriginIsInitialized(
                    quota::PERSISTENCE_TYPE_DEFAULT, testOriginMeta);
                if (dirInfoRes.isErr()) {
                  ASSERT_NSEQ(NS_OK, dirInfoRes.unwrapErr());
                }

                qm->EnsureQuotaForOrigin(testOriginMeta);
              }),
          NS_DISPATCH_SYNC);

      return BoolPromise::CreateAndResolve(true, __func__);
    };

    PerformOnBackgroundTarget(std::move(backgroundTask));
  }

  static const Name& GetTestFileName() {
    static Name testFileName = []() {
      nsCString testCFileName;
      testCFileName.SetLength(sExceedsPreallocation);
      return NS_ConvertASCIItoUTF16(testCFileName.BeginReading(),
                                    sExceedsPreallocation);
    }();

    return testFileName;
  }

  static uint64_t BytesOfName(const Name& aName) {
    return static_cast<uint64_t>(aName.Length() * sizeof(Name::char_type));
  }

  static const nsCString& GetTestData() {
    static const nsCString sTestData = "There is a way out of every box"_ns;
    return sTestData;
  }

  static void CreateNewEmptyFile(
      data::FileSystemDatabaseManager* const aDatabaseManager,
      const FileSystemChildMetadata& aFileSlot, EntryId& aFileId) {
    // The file should not exist yet
    Result<EntryId, QMResult> existingTestFile =
        aDatabaseManager->GetOrCreateFile(aFileSlot, /* create */ false);
    ASSERT_TRUE(existingTestFile.isErr());
    ASSERT_NSEQ(NS_ERROR_DOM_NOT_FOUND_ERR,
                ToNSResult(existingTestFile.unwrapErr()));

    // Create a new file
    TEST_TRY_UNWRAP(aFileId, aDatabaseManager->GetOrCreateFile(
                                 aFileSlot, /* create */ true));
  }

  static void WriteDataToFile(
      data::FileSystemDatabaseManager* const aDatabaseManager,
      const EntryId& aFileId, const nsCString& aData) {
    nsString type;
    TimeStamp lastModMilliS = 0;
    Path path;
    nsCOMPtr<nsIFile> fileObj;

    ASSERT_NSEQ(NS_OK, aDatabaseManager->GetFile(aFileId, type, lastModMilliS,
                                                 path, fileObj));

    uint32_t written = 0;
    ASSERT_NE(written, aData.Length());

#if FS_QUOTA_MANAGEMENT_ENABLED
    const quota::OriginMetadata& testOriginMeta = GetTestQuotaOriginMetadata();

    TEST_TRY_UNWRAP(nsCOMPtr<nsIOutputStream> fileStream,
                    quota::CreateFileOutputStream(
                        quota::PERSISTENCE_TYPE_DEFAULT, testOriginMeta,
                        quota::Client::FILESYSTEM, fileObj));

    auto finallyClose = MakeScopeExit(
        [&fileStream]() { ASSERT_NSEQ(NS_OK, fileStream->Close()); });
    ASSERT_NSEQ(NS_OK,
                fileStream->Write(aData.get(), aData.Length(), &written));

    ASSERT_EQ(aData.Length(), written);
#else
    std::FILE* writable = nullptr;
    ASSERT_NSEQ(NS_OK, fileObj->OpenANSIFileDesc("w", &writable));
    ASSERT_TRUE(writable);
    auto finallyClose = MakeScopeExit([&writable]() { std::fclose(writable); });
    std::fwrite(aData.get(), sizeof(aData[0]), aData.Length(), writable);
#endif
  }

  /* Static for use in callbacks */
  static void CreateRegisteredDataManager(
      Registered<data::FileSystemDataManager>& aRegisteredDataManager) {
    bool done = false;

    data::FileSystemDataManager::GetOrCreateFileSystemDataManager(
        GetTestQuotaOriginMetadata())
        ->Then(
            GetCurrentSerialEventTarget(), __func__,
            [&aRegisteredDataManager,
             &done](Registered<data::FileSystemDataManager>
                        registeredDataManager) mutable {
              auto doneOnReturn = MakeScopeExit([&done]() { done = true; });

              ASSERT_TRUE(registeredDataManager->IsOpen());
              aRegisteredDataManager = std::move(registeredDataManager);
            },
            [&done](nsresult rejectValue) {
              auto doneOnReturn = MakeScopeExit([&done]() { done = true; });

              ASSERT_NSEQ(NS_OK, rejectValue);
            });

    SpinEventLoopUntil("Promise is fulfilled"_ns, [&done]() { return done; });

    ASSERT_TRUE(aRegisteredDataManager);
    ASSERT_TRUE(aRegisteredDataManager->IsOpen());
    ASSERT_TRUE(aRegisteredDataManager->MutableDatabaseManagerPtr());
  }

  static void CheckUsageEqualTo(const quota::UsageInfo& aUsage,
                                uint64_t expected) {
    EXPECT_TRUE(aUsage.FileUsage().isNothing());
    auto dbUsage = aUsage.DatabaseUsage();
    ASSERT_TRUE(dbUsage.isSome());
    const auto actual = dbUsage.value();
    ASSERT_EQ(actual, expected);
  }

  static void CheckUsageGreaterThan(const quota::UsageInfo& aUsage,
                                    uint64_t expected) {
    EXPECT_TRUE(aUsage.FileUsage().isNothing());
    auto dbUsage = aUsage.DatabaseUsage();
    ASSERT_TRUE(dbUsage.isSome());
    const auto actual = dbUsage.value();
    ASSERT_GT(actual, expected);
  }
};

TEST_F(TestFileSystemQuotaClient, DISABLED_CheckUsageBeforeAnyFilesOnDisk) {
  auto backgroundTask = []() -> RefPtr<BoolPromise> {
    mozilla::Atomic<bool> isCanceled{false};
    auto ioTask = [&isCanceled](const RefPtr<quota::Client>& quotaClient,
                                data::FileSystemDatabaseManager* dbm) {
      ASSERT_FALSE(isCanceled);
      const quota::OriginMetadata& testOriginMeta =
          GetTestQuotaOriginMetadata();
      const Origin& testOrigin = testOriginMeta.mOrigin;

      // After initialization,
      // * database size is not zero
      // * GetUsageForOrigin and InitOrigin should agree
      TEST_TRY_UNWRAP(quota::UsageInfo usageNow,
                      quotaClient->InitOrigin(quota::PERSISTENCE_TYPE_DEFAULT,
                                              testOriginMeta, isCanceled));
      ASSERT_NO_FATAL_FAILURE(CheckUsageGreaterThan(usageNow, 0u));
      const auto initialDbUsage = usageNow.DatabaseUsage().value();

      TEST_TRY_UNWRAP(usageNow, quotaClient->GetUsageForOrigin(
                                    quota::PERSISTENCE_TYPE_DEFAULT,
                                    testOriginMeta, isCanceled));
      ASSERT_NO_FATAL_FAILURE(CheckUsageEqualTo(usageNow, initialDbUsage));

      // Create a new file
      TEST_TRY_UNWRAP(const EntryId rootId, data::GetRootHandle(testOrigin));
      FileSystemChildMetadata fileData(rootId, GetTestFileName());

      EntryId testFileId;
      ASSERT_NO_FATAL_FAILURE(CreateNewEmptyFile(dbm, fileData, testFileId));

      // After a new file has been created (only in the database),
      // * database size has increased
      // * GetUsageForOrigin and InitOrigin should agree
      const auto expectedUse = initialDbUsage + BytesOfName(GetTestFileName());

      TEST_TRY_UNWRAP(usageNow, quotaClient->GetUsageForOrigin(
                                    quota::PERSISTENCE_TYPE_DEFAULT,
                                    testOriginMeta, isCanceled));
      ASSERT_NO_FATAL_FAILURE(CheckUsageEqualTo(usageNow, expectedUse));

      TEST_TRY_UNWRAP(usageNow,
                      quotaClient->InitOrigin(quota::PERSISTENCE_TYPE_DEFAULT,
                                              testOriginMeta, isCanceled));
      ASSERT_NO_FATAL_FAILURE(CheckUsageEqualTo(usageNow, expectedUse));
    };

    RefPtr<mozilla::dom::quota::Client> quotaClient = fs::CreateQuotaClient();
    TEST_PROMISE_ASSERT_TRUE(quotaClient);

    // For uninitialized database, file usage is nothing
    auto checkTask =
        [&isCanceled](const RefPtr<mozilla::dom::quota::Client>& quotaClient) {
          TEST_TRY_UNWRAP(quota::UsageInfo usageNow,
                          quotaClient->GetUsageForOrigin(
                              quota::PERSISTENCE_TYPE_DEFAULT,
                              GetTestQuotaOriginMetadata(), isCanceled));

          ASSERT_TRUE(usageNow.DatabaseUsage().isNothing());
          EXPECT_TRUE(usageNow.FileUsage().isNothing());
        };

    PerformOnIOThread(std::move(checkTask),
                      RefPtr<mozilla::dom::quota::Client>{quotaClient});

    // Initialize database
    Registered<data::FileSystemDataManager> rdm;
    CreateRegisteredDataManager(rdm);
    TEST_PROMISE_ASSERT_TRUE(!HasFatalFailure());

    // Run tests with an initialized database
    PerformOnIOThread(std::move(ioTask), std::move(quotaClient),
                      rdm->MutableDatabaseManagerPtr());

    return BoolPromise::CreateAndResolve(true, __func__);
  };

  PerformOnBackgroundTarget(std::move(backgroundTask));
}

TEST_F(TestFileSystemQuotaClient, DISABLED_WritesToFilesShouldIncreaseUsage) {
  auto backgroundTask = []() -> RefPtr<BoolPromise> {
    mozilla::Atomic<bool> isCanceled{false};
    auto ioTask = [&isCanceled](
                      const RefPtr<mozilla::dom::quota::Client>& quotaClient,
                      data::FileSystemDatabaseManager* dbm) {
      const quota::OriginMetadata& testOriginMeta =
          GetTestQuotaOriginMetadata();
      const Origin& testOrigin = testOriginMeta.mOrigin;

      TEST_TRY_UNWRAP(const EntryId rootId, data::GetRootHandle(testOrigin));
      FileSystemChildMetadata fileData(rootId, GetTestFileName());

      EntryId testFileId;
      ASSERT_NO_FATAL_FAILURE(CreateNewEmptyFile(dbm, fileData, testFileId));
      // const auto testFileDbUsage = usageNow.DatabaseUsage().value();

      TEST_TRY_UNWRAP(
          quota::UsageInfo usageNow,
          quotaClient->GetUsageForOrigin(quota::PERSISTENCE_TYPE_DEFAULT,
                                         testOriginMeta, isCanceled));
      ASSERT_TRUE(usageNow.DatabaseUsage().isSome());
      const auto testFileDbUsage = usageNow.DatabaseUsage().value();

      TEST_TRY_UNWRAP(usageNow,
                      quotaClient->InitOrigin(quota::PERSISTENCE_TYPE_DEFAULT,
                                              testOriginMeta, isCanceled));
      ASSERT_NO_FATAL_FAILURE(CheckUsageEqualTo(usageNow, testFileDbUsage));

      // Fill the file with some content
      const nsCString& testData = GetTestData();
      const auto expectedTotalUsage = testFileDbUsage + testData.Length();

      ASSERT_NO_FATAL_FAILURE(WriteDataToFile(dbm, testFileId, testData));

      // In this test we don't lock the file -> no rescan is expected
      // and InitOrigin should return the previous value
      TEST_TRY_UNWRAP(usageNow,
                      quotaClient->InitOrigin(quota::PERSISTENCE_TYPE_DEFAULT,
                                              testOriginMeta, isCanceled));
      ASSERT_NO_FATAL_FAILURE(CheckUsageEqualTo(usageNow, testFileDbUsage));

      // When data manager unlocks the file, it should call update
      // but in this test we call it directly
      ASSERT_NSEQ(NS_OK, dbm->UpdateUsage(testFileId));

      // Disk usage should have increased after writing
      TEST_TRY_UNWRAP(usageNow,
                      quotaClient->InitOrigin(quota::PERSISTENCE_TYPE_DEFAULT,
                                              testOriginMeta, isCanceled));
      ASSERT_NO_FATAL_FAILURE(CheckUsageEqualTo(usageNow, expectedTotalUsage));

      // The usage values should now agree
      TEST_TRY_UNWRAP(usageNow, quotaClient->GetUsageForOrigin(
                                    quota::PERSISTENCE_TYPE_DEFAULT,
                                    testOriginMeta, isCanceled));
      ASSERT_NO_FATAL_FAILURE(CheckUsageEqualTo(usageNow, expectedTotalUsage));
    };

    RefPtr<mozilla::dom::quota::Client> quotaClient = fs::CreateQuotaClient();
    TEST_PROMISE_ASSERT_TRUE(quotaClient);

    // Storage initialization
    InitializeStorage();
    TEST_PROMISE_ASSERT_TRUE(!HasFatalFailure());

    // Initialize database
    Registered<data::FileSystemDataManager> rdm;
    CreateRegisteredDataManager(rdm);
    TEST_PROMISE_ASSERT_TRUE(!HasFatalFailure());

    // Run tests with an initialized database
    PerformOnIOThread(std::move(ioTask), std::move(quotaClient),
                      rdm->MutableDatabaseManagerPtr());

    return BoolPromise::CreateAndResolve(true, __func__);
  };

  PerformOnBackgroundTarget(std::move(backgroundTask));
}

TEST_F(TestFileSystemQuotaClient,
       DISABLED_TrackedFilesOnInitOriginShouldCauseRescan) {
  auto backgroundTask = []() -> RefPtr<BoolPromise> {
    mozilla::Atomic<bool> isCanceled{false};
    EntryId* testFileId = new EntryId();
    auto cleanupFileId = MakeScopeExit([&testFileId] { delete testFileId; });

    auto fileCreation = [&testFileId](data::FileSystemDatabaseManager* dbm) {
      const Origin& testOrigin = GetTestQuotaOriginMetadata().mOrigin;

      TEST_TRY_UNWRAP(const EntryId rootId, data::GetRootHandle(testOrigin));
      FileSystemChildMetadata fileData(rootId, GetTestFileName());

      EntryId someId;
      ASSERT_NO_FATAL_FAILURE(CreateNewEmptyFile(dbm, fileData, someId));
      testFileId->Append(someId);
    };

    auto writingToFile =
        [&isCanceled, testFileId](
            const RefPtr<mozilla::dom::quota::Client>& quotaClient,
            data::FileSystemDatabaseManager* dbm) {
          const quota::OriginMetadata& testOriginMeta =
              GetTestQuotaOriginMetadata();
          TEST_TRY_UNWRAP(
              quota::UsageInfo usageNow,
              quotaClient->GetUsageForOrigin(quota::PERSISTENCE_TYPE_DEFAULT,
                                             testOriginMeta, isCanceled));
          ASSERT_TRUE(usageNow.DatabaseUsage().isSome());
          const auto testFileDbUsage = usageNow.DatabaseUsage().value();

          // Fill the file with some content
          const auto& testData = GetTestData();
          const auto expectedTotalUsage = testFileDbUsage + testData.Length();

          ASSERT_NO_FATAL_FAILURE(WriteDataToFile(dbm, *testFileId, testData));

          // We don't call update now - because the file is tracked and
          // InitOrigin should perform a rescan
          TEST_TRY_UNWRAP(
              usageNow, quotaClient->InitOrigin(quota::PERSISTENCE_TYPE_DEFAULT,
                                                testOriginMeta, isCanceled));
          ASSERT_NO_FATAL_FAILURE(
              CheckUsageEqualTo(usageNow, expectedTotalUsage));

          // As always, the cached and scanned values should agree
          TEST_TRY_UNWRAP(usageNow, quotaClient->GetUsageForOrigin(
                                        quota::PERSISTENCE_TYPE_DEFAULT,
                                        testOriginMeta, isCanceled));
          ASSERT_NO_FATAL_FAILURE(
              CheckUsageEqualTo(usageNow, expectedTotalUsage));
        };

    RefPtr<mozilla::dom::quota::Client> quotaClient = fs::CreateQuotaClient();
    TEST_PROMISE_ASSERT_TRUE(quotaClient);

    // Storage initialization
    InitializeStorage();
    TEST_PROMISE_ASSERT_TRUE(!HasFatalFailure());

    // Initialize database
    Registered<data::FileSystemDataManager> rdm;
    CreateRegisteredDataManager(rdm);
    TEST_PROMISE_ASSERT_TRUE(!HasFatalFailure());

    PerformOnIOThread(std::move(fileCreation),
                      rdm->MutableDatabaseManagerPtr());

    // This should force a rescan
    TEST_PROMISE_ASSERT_TRUE(rdm->LockExclusive(*testFileId));
    PerformOnIOThread(std::move(writingToFile), std::move(quotaClient),
                      rdm->MutableDatabaseManagerPtr());

    return BoolPromise::CreateAndResolve(true, __func__);
  };

  PerformOnBackgroundTarget(std::move(backgroundTask));
}

TEST_F(TestFileSystemQuotaClient, DISABLED_RemovingFileShouldDecreaseUsage) {
  auto backgroundTask = []() -> RefPtr<BoolPromise> {
    mozilla::Atomic<bool> isCanceled{false};
    auto ioTask = [&isCanceled](
                      const RefPtr<mozilla::dom::quota::Client>& quotaClient,
                      data::FileSystemDatabaseManager* dbm) {
      const quota::OriginMetadata& testOriginMeta =
          GetTestQuotaOriginMetadata();
      const Origin& testOrigin = testOriginMeta.mOrigin;

      TEST_TRY_UNWRAP(const EntryId rootId, data::GetRootHandle(testOrigin));
      FileSystemChildMetadata fileData(rootId, GetTestFileName());

      EntryId testFileId;
      ASSERT_NO_FATAL_FAILURE(CreateNewEmptyFile(dbm, fileData, testFileId));
      TEST_TRY_UNWRAP(quota::UsageInfo usageNow,
                      quotaClient->InitOrigin(quota::PERSISTENCE_TYPE_DEFAULT,
                                              testOriginMeta, isCanceled));
      ASSERT_TRUE(usageNow.DatabaseUsage().isSome());
      const auto testFileDbUsage = usageNow.DatabaseUsage().value();

      // Fill the file with some content
      const nsCString& testData = GetTestData();
      const auto expectedTotalUsage = testFileDbUsage + testData.Length();

      ASSERT_NO_FATAL_FAILURE(WriteDataToFile(dbm, testFileId, testData));

      // Currently, usage is expected to be updated on unlock by data manager
      // but here UpdateUsage() is called directly
      ASSERT_NSEQ(NS_OK, dbm->UpdateUsage(testFileId));

      // At least some file disk usage should have appeared after unlocking
      TEST_TRY_UNWRAP(usageNow, quotaClient->GetUsageForOrigin(
                                    quota::PERSISTENCE_TYPE_DEFAULT,
                                    testOriginMeta, isCanceled));
      ASSERT_NO_FATAL_FAILURE(CheckUsageEqualTo(usageNow, expectedTotalUsage));

      TEST_TRY_UNWRAP(bool wasRemoved,
                      dbm->RemoveFile({rootId, GetTestFileName()}));
      ASSERT_TRUE(wasRemoved);

      // Removes cascade and usage table should be up to date immediately
      TEST_TRY_UNWRAP(usageNow,
                      quotaClient->InitOrigin(quota::PERSISTENCE_TYPE_DEFAULT,
                                              testOriginMeta, isCanceled));
      ASSERT_NO_FATAL_FAILURE(CheckUsageEqualTo(usageNow, testFileDbUsage));

      // GetUsageForOrigin should agree
      TEST_TRY_UNWRAP(usageNow, quotaClient->GetUsageForOrigin(
                                    quota::PERSISTENCE_TYPE_DEFAULT,
                                    testOriginMeta, isCanceled));
// Enable when bug 1806363 is fixed
#if !FS_QUOTA_MANAGEMENT_ENABLED
      ASSERT_NO_FATAL_FAILURE(CheckUsageEqualTo(usageNow, testFileDbUsage));
#endif
    };

    RefPtr<mozilla::dom::quota::Client> quotaClient = fs::CreateQuotaClient();
    TEST_PROMISE_ASSERT_TRUE(quotaClient);

    // Storage initialization
    InitializeStorage();
    TEST_PROMISE_ASSERT_TRUE(!HasFatalFailure());

    // Initialize database
    Registered<data::FileSystemDataManager> rdm;
    CreateRegisteredDataManager(rdm);
    TEST_PROMISE_ASSERT_TRUE(!HasFatalFailure());

    // Run tests with an initialized database
    PerformOnIOThread(std::move(ioTask), std::move(quotaClient),
                      rdm->MutableDatabaseManagerPtr());

    return BoolPromise::CreateAndResolve(true, __func__);
  };

  PerformOnBackgroundTarget(std::move(backgroundTask));
}

}  // namespace mozilla::dom::fs::test
