/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "QuotaManagerDependencyFixture.h"
#include "mozIStorageConnection.h"
#include "mozIStorageService.h"
#include "mozStorageCID.h"
#include "mozilla/Scoped.h"
#include "mozilla/dom/quota/IPCStreamCipherStrategy.h"
#include "mozilla/dom/quota/QuotaCommon.h"
#include "mozilla/dom/quota/QuotaManager.h"
#include "mozilla/dom/quota/QuotaObject.h"
#include "mozilla/dom/quota/ResultExtensions.h"
#include "nsIFile.h"
#include "nsIFileProtocolHandler.h"
#include "nsIFileURL.h"
#include "nsIURIMutator.h"
#include "nss.h"

#define QM_TEST_FAIL [](nsresult) { FAIL(); }

namespace mozilla {

MOZ_TYPE_SPECIFIC_SCOPED_POINTER_TEMPLATE(ScopedNSSContext, NSSInitContext,
                                          NSS_ShutdownContext);

namespace dom::quota::test {

namespace {

void InitializeClientDirectory(const ClientMetadata& aClientMetadata) {
  QuotaManager* quotaManager = QuotaManager::Get();

  QM_TRY(MOZ_TO_RESULT(quotaManager->EnsureTemporaryStorageIsInitialized()),
         QM_TEST_FAIL);

  QM_TRY_INSPECT(const auto& directory,
                 quotaManager
                     ->EnsureTemporaryOriginIsInitialized(
                         aClientMetadata.mPersistenceType, aClientMetadata)
                     .map([](const auto& aPair) { return aPair.first; }),
                 QM_TEST_FAIL);

  QM_TRY(MOZ_TO_RESULT(directory->Append(
             Client::TypeToString(aClientMetadata.mClientType))),
         QM_TEST_FAIL);

  QM_TRY_INSPECT(const bool& exists,
                 MOZ_TO_RESULT_INVOKE_MEMBER(directory, Exists), QM_TEST_FAIL);

  if (!exists) {
    QM_TRY(MOZ_TO_RESULT(directory->Create(nsIFile::DIRECTORY_TYPE, 0755)),
           QM_TEST_FAIL);
  }
}

void CreateStorageConnection(
    const ClientMetadata& aClientMetadata,
    const Maybe<int64_t>& aMaybeDirectoryLockId,
    const Maybe<IPCStreamCipherStrategy::KeyType>& aMaybeKey,
    nsCOMPtr<mozIStorageConnection>& aConnection) {
  QuotaManager* quotaManager = QuotaManager::Get();

  QM_TRY_UNWRAP(auto databaseFile,
                quotaManager->GetOriginDirectory(aClientMetadata),
                QM_TEST_FAIL);

  QM_TRY(MOZ_TO_RESULT(databaseFile->Append(
             Client::TypeToString(aClientMetadata.mClientType))),
         QM_TEST_FAIL);

  QM_TRY(MOZ_TO_RESULT(databaseFile->Append(u"testdatabase.sqlite"_ns)),
         QM_TEST_FAIL);

  QM_TRY_INSPECT(
      const auto& protocolHandler,
      MOZ_TO_RESULT_GET_TYPED(nsCOMPtr<nsIProtocolHandler>,
                              MOZ_SELECT_OVERLOAD(do_GetService),
                              NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "file"),
      QM_TEST_FAIL);

  QM_TRY_INSPECT(const auto& fileHandler,
                 MOZ_TO_RESULT_GET_TYPED(nsCOMPtr<nsIFileProtocolHandler>,
                                         MOZ_SELECT_OVERLOAD(do_QueryInterface),
                                         protocolHandler),
                 QM_TEST_FAIL);

  QM_TRY_INSPECT(
      const auto& mutator,
      MOZ_TO_RESULT_INVOKE_MEMBER_TYPED(nsCOMPtr<nsIURIMutator>, fileHandler,
                                        NewFileURIMutator, databaseFile),
      QM_TEST_FAIL);

  const auto directoryLockIdClause = [&aMaybeDirectoryLockId] {
    nsAutoCString directoryLockIdClause;
    if (aMaybeDirectoryLockId) {
      directoryLockIdClause =
          "&directoryLockId="_ns + IntToCString(*aMaybeDirectoryLockId);
    }
    return directoryLockIdClause;
  }();

  const auto keyClause = [&aMaybeKey] {
    nsAutoCString keyClause;
    if (aMaybeKey) {
      keyClause.AssignLiteral("&key=");
      for (uint8_t byte : IPCStreamCipherStrategy::SerializeKey(*aMaybeKey)) {
        keyClause.AppendPrintf("%02x", byte);
      }
    }
    return keyClause;
  }();

  QM_TRY_INSPECT(
      const auto& fileURL,
      ([&mutator, &directoryLockIdClause,
        &keyClause]() -> Result<nsCOMPtr<nsIFileURL>, nsresult> {
        nsCOMPtr<nsIFileURL> result;
        QM_TRY(MOZ_TO_RESULT(NS_MutateURI(mutator)
                                 .SetQuery("cache=private"_ns +
                                           directoryLockIdClause + keyClause)
                                 .Finalize(result)));
        return result;
      }()),
      QM_TEST_FAIL);

  QM_TRY_INSPECT(const auto& storageService,
                 MOZ_TO_RESULT_GET_TYPED(nsCOMPtr<mozIStorageService>,
                                         MOZ_SELECT_OVERLOAD(do_GetService),
                                         MOZ_STORAGE_SERVICE_CONTRACTID),
                 QM_TEST_FAIL);

  QM_TRY_UNWRAP(auto connection,
                MOZ_TO_RESULT_INVOKE_MEMBER_TYPED(
                    nsCOMPtr<mozIStorageConnection>, storageService,
                    OpenDatabaseWithFileURL, fileURL, EmptyCString(),
                    mozIStorageService::CONNECTION_DEFAULT),
                QM_TEST_FAIL);

  QM_TRY(MOZ_TO_RESULT(
             connection->ExecuteSimpleSQL("PRAGMA journal_mode = wal;"_ns)),
         QM_TEST_FAIL);

  // The connection needs to be re-created, otherwise GetQuotaObjects is not
  // able to get the quota object for the journal file.

  QM_TRY(MOZ_TO_RESULT(connection->Close()), QM_TEST_FAIL);

  QM_TRY_UNWRAP(connection,
                MOZ_TO_RESULT_INVOKE_MEMBER_TYPED(
                    nsCOMPtr<mozIStorageConnection>, storageService,
                    OpenDatabaseWithFileURL, fileURL, EmptyCString(),
                    mozIStorageService::CONNECTION_DEFAULT),
                QM_TEST_FAIL);

  aConnection = std::move(connection);
}

}  // namespace

class TestStorageConnection : public QuotaManagerDependencyFixture {
 public:
  static void SetUpTestCase() {
    // Do this only once, do not tear it down per test case.
    if (!sNssContext) {
      sNssContext =
          NSS_InitContext("", "", "", "", nullptr,
                          NSS_INIT_READONLY | NSS_INIT_NOCERTDB |
                              NSS_INIT_NOMODDB | NSS_INIT_FORCEOPEN |
                              NSS_INIT_OPTIMIZESPACE | NSS_INIT_NOROOTINIT);
    }

    ASSERT_NO_FATAL_FAILURE(InitializeFixture());
  }

  static void TearDownTestCase() {
    ASSERT_NO_FATAL_FAILURE(ShutdownFixture());

    sNssContext = nullptr;
  }

  void TearDown() override {
    ASSERT_NO_FATAL_FAILURE(ClearStoragesForOrigin(GetTestOriginMetadata()));
  }

 private:
  inline static ScopedNSSContext sNssContext = ScopedNSSContext{};
};

TEST_F(TestStorageConnection, BaseVFS) {
  // XXX This call can't be wrapped with ASSERT_NO_FATAL_FAILURE because
  // base-toolchains builds fail with: error: duplicate label
  // 'gtest_label_testnofatal_214'
  PerformClientDirectoryTest(GetTestClientMetadata(), [](int64_t) {
    ASSERT_NO_FATAL_FAILURE(InitializeClientDirectory(GetTestClientMetadata()));

    nsCOMPtr<mozIStorageConnection> connection;
    ASSERT_NO_FATAL_FAILURE(CreateStorageConnection(
        GetTestClientMetadata(), Nothing(), Nothing(), connection));

    RefPtr<QuotaObject> quotaObject;
    RefPtr<QuotaObject> journalQuotaObject;

    nsresult rv = connection->GetQuotaObjects(
        getter_AddRefs(quotaObject), getter_AddRefs(journalQuotaObject));
    ASSERT_EQ(rv, NS_ERROR_FAILURE);

    ASSERT_FALSE(quotaObject);
    ASSERT_FALSE(journalQuotaObject);

    QM_TRY(MOZ_TO_RESULT(connection->Close()), QM_TEST_FAIL);
  });
}

TEST_F(TestStorageConnection, QuotaVFS) {
  // XXX This call can't be wrapped with ASSERT_NO_FATAL_FAILURE because
  // base-toolchains builds fail with: error: duplicate label
  // 'gtest_label_testnofatal_214'
  PerformClientDirectoryTest(
      GetTestClientMetadata(), [](int64_t aDirectoryLockId) {
        ASSERT_NO_FATAL_FAILURE(
            InitializeClientDirectory(GetTestClientMetadata()));

        nsCOMPtr<mozIStorageConnection> connection;
        ASSERT_NO_FATAL_FAILURE(CreateStorageConnection(GetTestClientMetadata(),
                                                        Some(aDirectoryLockId),
                                                        Nothing(), connection));

        RefPtr<QuotaObject> quotaObject;
        RefPtr<QuotaObject> journalQuotaObject;

        QM_TRY(MOZ_TO_RESULT(connection->GetQuotaObjects(
                   getter_AddRefs(quotaObject),
                   getter_AddRefs(journalQuotaObject))),
               QM_TEST_FAIL);

        ASSERT_TRUE(quotaObject);
        ASSERT_TRUE(journalQuotaObject);

        QM_TRY(MOZ_TO_RESULT(connection->Close()), QM_TEST_FAIL);
      });
}

TEST_F(TestStorageConnection, ObfuscatingVFS) {
  // XXX This call can't be wrapped with ASSERT_NO_FATAL_FAILURE because
  // base-toolchains builds fail with: error: duplicate label
  // 'gtest_label_testnofatal_214'
  PerformClientDirectoryTest(
      GetTestClientMetadata(), [](int64_t aDirectoryLockId) {
        ASSERT_NO_FATAL_FAILURE(
            InitializeClientDirectory(GetTestClientMetadata()));

        QM_TRY_INSPECT(const auto& key, IPCStreamCipherStrategy::GenerateKey(),
                       QM_TEST_FAIL);

        nsCOMPtr<mozIStorageConnection> connection;
        ASSERT_NO_FATAL_FAILURE(CreateStorageConnection(GetTestClientMetadata(),
                                                        Some(aDirectoryLockId),
                                                        Some(key), connection));

        RefPtr<QuotaObject> quotaObject;
        RefPtr<QuotaObject> journalQuotaObject;

        QM_TRY(MOZ_TO_RESULT(connection->GetQuotaObjects(
                   getter_AddRefs(quotaObject),
                   getter_AddRefs(journalQuotaObject))),
               QM_TEST_FAIL);

        ASSERT_TRUE(quotaObject);
        ASSERT_TRUE(journalQuotaObject);

        QM_TRY(MOZ_TO_RESULT(connection->Close()), QM_TEST_FAIL);
      });
}

}  // namespace dom::quota::test
}  // namespace mozilla
