/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "mozilla/SpinEventLoopUntil.h"
#include "mozilla/dom/quota/DirectoryLock.h"
#include "mozilla/dom/quota/OriginScope.h"
#include "mozilla/dom/quota/QuotaManager.h"
#include "mozilla/gtest/MozAssertions.h"
#include "QuotaManagerDependencyFixture.h"

namespace mozilla::dom::quota::test {

class TestQuotaManager : public QuotaManagerDependencyFixture {
 public:
  static void SetUpTestCase() { ASSERT_NO_FATAL_FAILURE(InitializeFixture()); }

  static void TearDownTestCase() { ASSERT_NO_FATAL_FAILURE(ShutdownFixture()); }
};

// Test OpenStorageDirectory when an opening of the storage directory is
// already ongoing and storage shutdown is scheduled after that.
TEST_F(TestQuotaManager, OpenStorageDirectory_OngoingWithScheduledShutdown) {
  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());

  ASSERT_NO_FATAL_FAILURE(AssertStorageNotInitialized());

  PerformOnBackgroundThread([]() {
    QuotaManager* quotaManager = QuotaManager::Get();
    ASSERT_TRUE(quotaManager);

    RefPtr<UniversalDirectoryLock> directoryLock;

    nsTArray<RefPtr<BoolPromise>> promises;

    promises.AppendElement(
        quotaManager
            ->OpenStorageDirectory(
                Nullable<PersistenceType>(PERSISTENCE_TYPE_PERSISTENT),
                OriginScope::FromNull(), Nullable<Client::Type>(),
                /* aExclusive */ false)
            ->Then(GetCurrentSerialEventTarget(), __func__,
                   [&directoryLock](
                       UniversalDirectoryLockPromise::ResolveOrRejectValue&&
                           aValue) {
                     if (aValue.IsReject()) {
                       return BoolPromise::CreateAndReject(aValue.RejectValue(),
                                                           __func__);
                     }

                     [&aValue]() { ASSERT_TRUE(aValue.ResolveValue()); }();

                     directoryLock = std::move(aValue.ResolveValue());

                     return BoolPromise::CreateAndResolve(true, __func__);
                   })
            ->Then(quotaManager->IOThread(), __func__,
                   [](const BoolPromise::ResolveOrRejectValue& aValue) {
                     if (aValue.IsReject()) {
                       return BoolPromise::CreateAndReject(aValue.RejectValue(),
                                                           __func__);
                     }

                     []() {
                       QuotaManager* quotaManager = QuotaManager::Get();
                       ASSERT_TRUE(quotaManager);

                       ASSERT_TRUE(
                           quotaManager->IsStorageInitializedInternal());
                     }();

                     return BoolPromise::CreateAndResolve(true, __func__);
                   })
            ->Then(GetCurrentSerialEventTarget(), __func__,
                   [&directoryLock](
                       const BoolPromise::ResolveOrRejectValue& aValue) {
                     directoryLock = nullptr;

                     if (aValue.IsReject()) {
                       return BoolPromise::CreateAndReject(aValue.RejectValue(),
                                                           __func__);
                     }

                     return BoolPromise::CreateAndResolve(true, __func__);
                   }));
    promises.AppendElement(quotaManager->ShutdownStorage());
    promises.AppendElement(
        quotaManager
            ->OpenStorageDirectory(
                Nullable<PersistenceType>(PERSISTENCE_TYPE_PERSISTENT),
                OriginScope::FromNull(), Nullable<Client::Type>(),
                /* aExclusive */ false)
            ->Then(GetCurrentSerialEventTarget(), __func__,
                   [](const UniversalDirectoryLockPromise::ResolveOrRejectValue&
                          aValue) {
                     if (aValue.IsReject()) {
                       return BoolPromise::CreateAndReject(aValue.RejectValue(),
                                                           __func__);
                     }

                     return BoolPromise::CreateAndResolve(true, __func__);
                   }));

    bool done = false;

    BoolPromise::All(GetCurrentSerialEventTarget(), promises)
        ->Then(
            GetCurrentSerialEventTarget(), __func__,
            [&done](const CopyableTArray<bool>& aResolveValues) {
              QuotaManager* quotaManager = QuotaManager::Get();
              ASSERT_TRUE(quotaManager);

              ASSERT_TRUE(quotaManager->IsStorageInitialized());

              done = true;
            },
            [&done](nsresult aRejectValue) {
              ASSERT_TRUE(false);

              done = true;
            });

    SpinEventLoopUntil("Promise is fulfilled"_ns, [&done]() { return done; });
  });

  ASSERT_NO_FATAL_FAILURE(AssertStorageInitialized());

  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());
}

// Test OpenStorageDirectory when an opening of the storage directory is
// already ongoing and an exclusive directory lock is requested after that.
TEST_F(TestQuotaManager,
       OpenStorageDirectory_OngoingWithExclusiveDirectoryLock) {
  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());

  ASSERT_NO_FATAL_FAILURE(AssertStorageNotInitialized());

  PerformOnBackgroundThread([]() {
    QuotaManager* quotaManager = QuotaManager::Get();
    ASSERT_TRUE(quotaManager);

    RefPtr<UniversalDirectoryLock> directoryLock =
        quotaManager->CreateDirectoryLockInternal(Nullable<PersistenceType>(),
                                                  OriginScope::FromNull(),
                                                  Nullable<Client::Type>(),
                                                  /* aExclusive */ true);

    nsTArray<RefPtr<BoolPromise>> promises;

    promises.AppendElement(
        quotaManager
            ->OpenStorageDirectory(
                Nullable<PersistenceType>(PERSISTENCE_TYPE_PERSISTENT),
                OriginScope::FromNull(), Nullable<Client::Type>(),
                /* aExclusive */ false)
            ->Then(
                GetCurrentSerialEventTarget(), __func__,
                [&directoryLock](
                    const UniversalDirectoryLockPromise::ResolveOrRejectValue&
                        aValue) {
                  directoryLock = nullptr;

                  if (aValue.IsReject()) {
                    return BoolPromise::CreateAndReject(aValue.RejectValue(),
                                                        __func__);
                  }

                  return BoolPromise::CreateAndResolve(true, __func__);
                }));
    promises.AppendElement(directoryLock->Acquire());
    promises.AppendElement(
        quotaManager
            ->OpenStorageDirectory(
                Nullable<PersistenceType>(PERSISTENCE_TYPE_PERSISTENT),
                OriginScope::FromNull(), Nullable<Client::Type>(),
                /* aExclusive */ false)
            ->Then(GetCurrentSerialEventTarget(), __func__,
                   [](const UniversalDirectoryLockPromise::ResolveOrRejectValue&
                          aValue) {
                     if (aValue.IsReject()) {
                       return BoolPromise::CreateAndReject(aValue.RejectValue(),
                                                           __func__);
                     }

                     return BoolPromise::CreateAndResolve(true, __func__);
                   }));

    bool done = false;

    BoolPromise::All(GetCurrentSerialEventTarget(), promises)
        ->Then(
            GetCurrentSerialEventTarget(), __func__,
            [&done](const CopyableTArray<bool>& aResolveValues) {
              QuotaManager* quotaManager = QuotaManager::Get();
              ASSERT_TRUE(quotaManager);

              ASSERT_TRUE(quotaManager->IsStorageInitialized());

              done = true;
            },
            [&done](nsresult aRejectValue) {
              ASSERT_TRUE(false);

              done = true;
            });

    SpinEventLoopUntil("Promise is fulfilled"_ns, [&done]() { return done; });
  });

  ASSERT_NO_FATAL_FAILURE(AssertStorageInitialized());

  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());
}

// Test OpenStorageDirectory when an opening of the storage directory already
// finished.
TEST_F(TestQuotaManager, OpenStorageDirectory_Finished) {
  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());

  ASSERT_NO_FATAL_FAILURE(AssertStorageNotInitialized());

  PerformOnBackgroundThread([]() {
    QuotaManager* quotaManager = QuotaManager::Get();
    ASSERT_TRUE(quotaManager);

    bool done = false;

    quotaManager
        ->OpenStorageDirectory(
            Nullable<PersistenceType>(PERSISTENCE_TYPE_PERSISTENT),
            OriginScope::FromNull(), Nullable<Client::Type>(),
            /* aExclusive */ false)
        ->Then(
            GetCurrentSerialEventTarget(), __func__,
            [&done](const UniversalDirectoryLockPromise::ResolveOrRejectValue&
                        aValue) {
              QuotaManager* quotaManager = QuotaManager::Get();
              ASSERT_TRUE(quotaManager);

              ASSERT_TRUE(quotaManager->IsStorageInitialized());

              done = true;
            });

    SpinEventLoopUntil("Promise is fulfilled"_ns, [&done]() { return done; });

    done = false;

    quotaManager
        ->OpenStorageDirectory(
            Nullable<PersistenceType>(PERSISTENCE_TYPE_PERSISTENT),
            OriginScope::FromNull(), Nullable<Client::Type>(),
            /* aExclusive */ false)
        ->Then(
            GetCurrentSerialEventTarget(), __func__,
            [&done](const UniversalDirectoryLockPromise::ResolveOrRejectValue&
                        aValue) {
              QuotaManager* quotaManager = QuotaManager::Get();
              ASSERT_TRUE(quotaManager);

              ASSERT_TRUE(quotaManager->IsStorageInitialized());

              done = true;
            });

    SpinEventLoopUntil("Promise is fulfilled"_ns, [&done]() { return done; });
  });

  ASSERT_NO_FATAL_FAILURE(AssertStorageInitialized());

  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());
}

// Test OpenStorageDirectory when an opening of the storage directory already
// finished but storage shutdown has just been scheduled.
TEST_F(TestQuotaManager, OpenStorageDirectory_FinishedWithScheduledShutdown) {
  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());

  ASSERT_NO_FATAL_FAILURE(AssertStorageNotInitialized());

  PerformOnBackgroundThread([]() {
    QuotaManager* quotaManager = QuotaManager::Get();
    ASSERT_TRUE(quotaManager);

    bool done = false;

    quotaManager
        ->OpenStorageDirectory(
            Nullable<PersistenceType>(PERSISTENCE_TYPE_PERSISTENT),
            OriginScope::FromNull(), Nullable<Client::Type>(),
            /* aExclusive */ false)
        ->Then(
            GetCurrentSerialEventTarget(), __func__,
            [&done](const UniversalDirectoryLockPromise::ResolveOrRejectValue&
                        aValue) {
              QuotaManager* quotaManager = QuotaManager::Get();
              ASSERT_TRUE(quotaManager);

              ASSERT_TRUE(quotaManager->IsStorageInitialized());

              done = true;
            });

    SpinEventLoopUntil("Promise is fulfilled"_ns, [&done]() { return done; });

    nsTArray<RefPtr<BoolPromise>> promises;

    promises.AppendElement(quotaManager->ShutdownStorage());
    promises.AppendElement(
        quotaManager
            ->OpenStorageDirectory(
                Nullable<PersistenceType>(PERSISTENCE_TYPE_PERSISTENT),
                OriginScope::FromNull(), Nullable<Client::Type>(),
                /* aExclusive */ false)
            ->Then(GetCurrentSerialEventTarget(), __func__,
                   [](const UniversalDirectoryLockPromise::ResolveOrRejectValue&
                          aValue) {
                     if (aValue.IsReject()) {
                       return BoolPromise::CreateAndReject(aValue.RejectValue(),
                                                           __func__);
                     }

                     return BoolPromise::CreateAndResolve(true, __func__);
                   }));

    done = false;

    BoolPromise::All(GetCurrentSerialEventTarget(), promises)
        ->Then(
            GetCurrentSerialEventTarget(), __func__,
            [&done](const CopyableTArray<bool>& aResolveValues) {
              QuotaManager* quotaManager = QuotaManager::Get();
              ASSERT_TRUE(quotaManager);

              ASSERT_TRUE(quotaManager->IsStorageInitialized());

              done = true;
            },
            [&done](nsresult aRejectValue) {
              ASSERT_TRUE(false);

              done = true;
            });

    SpinEventLoopUntil("Promise is fulfilled"_ns, [&done]() { return done; });
  });

  ASSERT_NO_FATAL_FAILURE(AssertStorageInitialized());

  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());
}

// Test OpenStorageDirectory when an opening of the storage directory already
// finished and an exclusive client directory lock for a non-overlapping
// origin is acquired in between.
TEST_F(TestQuotaManager,
       OpenStorageDirectory_FinishedWithExclusiveClientDirectoryLock) {
  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());

  ASSERT_NO_FATAL_FAILURE(AssertStorageNotInitialized());

  PerformOnBackgroundThread([]() {
    QuotaManager* quotaManager = QuotaManager::Get();
    ASSERT_TRUE(quotaManager);

    bool done = false;

    quotaManager
        ->OpenStorageDirectory(
            Nullable<PersistenceType>(PERSISTENCE_TYPE_PERSISTENT),
            OriginScope::FromNull(), Nullable<Client::Type>(),
            /* aExclusive */ false)
        ->Then(
            GetCurrentSerialEventTarget(), __func__,
            [&done](const UniversalDirectoryLockPromise::ResolveOrRejectValue&
                        aValue) {
              QuotaManager* quotaManager = QuotaManager::Get();
              ASSERT_TRUE(quotaManager);

              ASSERT_TRUE(quotaManager->IsStorageInitialized());

              done = true;
            });

    SpinEventLoopUntil("Promise is fulfilled"_ns, [&done]() { return done; });

    RefPtr<ClientDirectoryLock> directoryLock =
        quotaManager->CreateDirectoryLock(GetTestClientMetadata(),
                                          /* aExclusive */ true);

    done = false;

    directoryLock->Acquire()->Then(
        GetCurrentSerialEventTarget(), __func__,
        [&done](const BoolPromise::ResolveOrRejectValue& aValue) {
          done = true;
        });

    SpinEventLoopUntil("Promise is fulfilled"_ns, [&done]() { return done; });

    done = false;

    quotaManager
        ->OpenStorageDirectory(
            Nullable<PersistenceType>(PERSISTENCE_TYPE_PERSISTENT),
            OriginScope::FromNull(), Nullable<Client::Type>(),
            /* aExclusive */ false)
        ->Then(
            GetCurrentSerialEventTarget(), __func__,
            [&done](const UniversalDirectoryLockPromise::ResolveOrRejectValue&
                        aValue) {
              QuotaManager* quotaManager = QuotaManager::Get();
              ASSERT_TRUE(quotaManager);

              ASSERT_TRUE(quotaManager->IsStorageInitialized());

              done = true;
            });

    SpinEventLoopUntil("Promise is fulfilled"_ns, [&done]() { return done; });
  });

  ASSERT_NO_FATAL_FAILURE(AssertStorageInitialized());

  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());
}

// Test OpenClientDirctory when an opening of a client directory is already
// ongoing and storage shutdown is scheduled after that.
TEST_F(TestQuotaManager, OpenClientDirectory_OngoingWithScheduledShutdown) {
  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());

  ASSERT_NO_FATAL_FAILURE(AssertStorageNotInitialized());

  PerformOnBackgroundThread([]() {
    QuotaManager* quotaManager = QuotaManager::Get();
    ASSERT_TRUE(quotaManager);

    RefPtr<ClientDirectoryLock> directoryLock;

    nsTArray<RefPtr<BoolPromise>> promises;

    promises.AppendElement(
        quotaManager->OpenClientDirectory(GetTestClientMetadata())
            ->Then(
                GetCurrentSerialEventTarget(), __func__,
                [&directoryLock](
                    ClientDirectoryLockPromise::ResolveOrRejectValue&& aValue) {
                  if (aValue.IsReject()) {
                    return BoolPromise::CreateAndReject(aValue.RejectValue(),
                                                        __func__);
                  }

                  [&aValue]() { ASSERT_TRUE(aValue.ResolveValue()); }();

                  directoryLock = std::move(aValue.ResolveValue());

                  return BoolPromise::CreateAndResolve(true, __func__);
                })
            ->Then(quotaManager->IOThread(), __func__,
                   [](const BoolPromise::ResolveOrRejectValue& aValue) {
                     if (aValue.IsReject()) {
                       return BoolPromise::CreateAndReject(aValue.RejectValue(),
                                                           __func__);
                     }

                     []() {
                       QuotaManager* quotaManager = QuotaManager::Get();
                       ASSERT_TRUE(quotaManager);

                       ASSERT_TRUE(
                           quotaManager->IsStorageInitializedInternal());
                     }();

                     return BoolPromise::CreateAndResolve(true, __func__);
                   })
            ->Then(GetCurrentSerialEventTarget(), __func__,
                   [&directoryLock](
                       const BoolPromise::ResolveOrRejectValue& aValue) {
                     directoryLock = nullptr;

                     if (aValue.IsReject()) {
                       return BoolPromise::CreateAndReject(aValue.RejectValue(),
                                                           __func__);
                     }

                     return BoolPromise::CreateAndResolve(true, __func__);
                   }));
    promises.AppendElement(quotaManager->ShutdownStorage());
    promises.AppendElement(
        quotaManager->OpenClientDirectory(GetTestClientMetadata())
            ->Then(GetCurrentSerialEventTarget(), __func__,
                   [](const ClientDirectoryLockPromise::ResolveOrRejectValue&
                          aValue) {
                     if (aValue.IsReject()) {
                       return BoolPromise::CreateAndReject(aValue.RejectValue(),
                                                           __func__);
                     }

                     return BoolPromise::CreateAndResolve(true, __func__);
                   }));

    bool done = false;

    BoolPromise::All(GetCurrentSerialEventTarget(), promises)
        ->Then(
            GetCurrentSerialEventTarget(), __func__,
            [&done](const CopyableTArray<bool>& aResolveValues) {
              QuotaManager* quotaManager = QuotaManager::Get();
              ASSERT_TRUE(quotaManager);

              ASSERT_TRUE(quotaManager->IsStorageInitialized());

              done = true;
            },
            [&done](nsresult aRejectValue) {
              ASSERT_TRUE(false);

              done = true;
            });

    SpinEventLoopUntil("Promise is fulfilled"_ns, [&done]() { return done; });
  });

  ASSERT_NO_FATAL_FAILURE(AssertStorageInitialized());

  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());
}

// Test OpenClientDirectory when an opening of a client directory is already
// ongoing and an exclusive directory lock is requested after that.
TEST_F(TestQuotaManager,
       OpenClientDirectory_OngoingWithExclusiveDirectoryLock) {
  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());

  ASSERT_NO_FATAL_FAILURE(AssertStorageNotInitialized());

  PerformOnBackgroundThread([]() {
    QuotaManager* quotaManager = QuotaManager::Get();
    ASSERT_TRUE(quotaManager);

    RefPtr<UniversalDirectoryLock> directoryLock =
        quotaManager->CreateDirectoryLockInternal(Nullable<PersistenceType>(),
                                                  OriginScope::FromNull(),
                                                  Nullable<Client::Type>(),
                                                  /* aExclusive */ true);

    nsTArray<RefPtr<BoolPromise>> promises;

    promises.AppendElement(
        quotaManager->OpenClientDirectory(GetTestClientMetadata())
            ->Then(GetCurrentSerialEventTarget(), __func__,
                   [&directoryLock](
                       const ClientDirectoryLockPromise::ResolveOrRejectValue&
                           aValue) {
                     directoryLock = nullptr;

                     if (aValue.IsReject()) {
                       return BoolPromise::CreateAndReject(aValue.RejectValue(),
                                                           __func__);
                     }

                     return BoolPromise::CreateAndResolve(true, __func__);
                   }));
    promises.AppendElement(directoryLock->Acquire());
    promises.AppendElement(
        quotaManager->OpenClientDirectory(GetTestClientMetadata())
            ->Then(GetCurrentSerialEventTarget(), __func__,
                   [](const ClientDirectoryLockPromise::ResolveOrRejectValue&
                          aValue) {
                     if (aValue.IsReject()) {
                       return BoolPromise::CreateAndReject(aValue.RejectValue(),
                                                           __func__);
                     }

                     return BoolPromise::CreateAndResolve(true, __func__);
                   }));

    bool done = false;

    BoolPromise::All(GetCurrentSerialEventTarget(), promises)
        ->Then(
            GetCurrentSerialEventTarget(), __func__,
            [&done](const CopyableTArray<bool>& aResolveValues) {
              QuotaManager* quotaManager = QuotaManager::Get();
              ASSERT_TRUE(quotaManager);

              ASSERT_TRUE(quotaManager->IsStorageInitialized());

              done = true;
            },
            [&done](nsresult aRejectValue) {
              ASSERT_TRUE(false);

              done = true;
            });

    SpinEventLoopUntil("Promise is fulfilled"_ns, [&done]() { return done; });
  });

  ASSERT_NO_FATAL_FAILURE(AssertStorageInitialized());

  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());
}

// Test OpenClientDirectory when an opening of a client directory already
// finished.
TEST_F(TestQuotaManager, OpenClientDirectory_Finished) {
  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());

  ASSERT_NO_FATAL_FAILURE(AssertStorageNotInitialized());

  PerformOnBackgroundThread([]() {
    QuotaManager* quotaManager = QuotaManager::Get();
    ASSERT_TRUE(quotaManager);

    bool done = false;

    quotaManager->OpenClientDirectory(GetTestClientMetadata())
        ->Then(GetCurrentSerialEventTarget(), __func__,
               [&done](const ClientDirectoryLockPromise::ResolveOrRejectValue&
                           aValue) {
                 QuotaManager* quotaManager = QuotaManager::Get();
                 ASSERT_TRUE(quotaManager);

                 ASSERT_TRUE(quotaManager->IsStorageInitialized());

                 done = true;
               });

    SpinEventLoopUntil("Promise is fulfilled"_ns, [&done]() { return done; });

    done = false;

    quotaManager->OpenClientDirectory(GetTestClientMetadata())
        ->Then(GetCurrentSerialEventTarget(), __func__,
               [&done](const ClientDirectoryLockPromise::ResolveOrRejectValue&
                           aValue) {
                 QuotaManager* quotaManager = QuotaManager::Get();
                 ASSERT_TRUE(quotaManager);

                 ASSERT_TRUE(quotaManager->IsStorageInitialized());

                 done = true;
               });

    SpinEventLoopUntil("Promise is fulfilled"_ns, [&done]() { return done; });
  });

  ASSERT_NO_FATAL_FAILURE(AssertStorageInitialized());

  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());
}

// Test OpenClientDirectory when an opening of a client directory already
// finished but storage shutdown has just been scheduled.
TEST_F(TestQuotaManager, OpenClientDirectory_FinishedWithScheduledShutdown) {
  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());

  ASSERT_NO_FATAL_FAILURE(AssertStorageNotInitialized());

  PerformOnBackgroundThread([]() {
    QuotaManager* quotaManager = QuotaManager::Get();
    ASSERT_TRUE(quotaManager);

    bool done = false;

    quotaManager->OpenClientDirectory(GetTestClientMetadata())
        ->Then(GetCurrentSerialEventTarget(), __func__,
               [&done](const ClientDirectoryLockPromise::ResolveOrRejectValue&
                           aValue) {
                 QuotaManager* quotaManager = QuotaManager::Get();
                 ASSERT_TRUE(quotaManager);

                 ASSERT_TRUE(quotaManager->IsStorageInitialized());

                 done = true;
               });

    SpinEventLoopUntil("Promise is fulfilled"_ns, [&done]() { return done; });

    nsTArray<RefPtr<BoolPromise>> promises;

    promises.AppendElement(quotaManager->ShutdownStorage());
    promises.AppendElement(
        quotaManager->OpenClientDirectory(GetTestClientMetadata())
            ->Then(GetCurrentSerialEventTarget(), __func__,
                   [](const ClientDirectoryLockPromise::ResolveOrRejectValue&
                          aValue) {
                     if (aValue.IsReject()) {
                       return BoolPromise::CreateAndReject(aValue.RejectValue(),
                                                           __func__);
                     }

                     return BoolPromise::CreateAndResolve(true, __func__);
                   }));

    done = false;

    BoolPromise::All(GetCurrentSerialEventTarget(), promises)
        ->Then(
            GetCurrentSerialEventTarget(), __func__,
            [&done](const CopyableTArray<bool>& aResolveValues) {
              QuotaManager* quotaManager = QuotaManager::Get();
              ASSERT_TRUE(quotaManager);

              ASSERT_TRUE(quotaManager->IsStorageInitialized());

              done = true;
            },
            [&done](nsresult aRejectValue) {
              ASSERT_TRUE(false);

              done = true;
            });

    SpinEventLoopUntil("Promise is fulfilled"_ns, [&done]() { return done; });
  });

  ASSERT_NO_FATAL_FAILURE(AssertStorageInitialized());

  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());
}

// Test OpenClientDirectory when an opening of a client directory already
// finished with an exclusive client directory lock for a different origin is
// acquired in between.
TEST_F(TestQuotaManager,
       OpenClientDirectory_FinishedWithOtherExclusiveClientDirectoryLock) {
  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());

  ASSERT_NO_FATAL_FAILURE(AssertStorageNotInitialized());

  PerformOnBackgroundThread([]() {
    QuotaManager* quotaManager = QuotaManager::Get();
    ASSERT_TRUE(quotaManager);

    bool done = false;

    quotaManager->OpenClientDirectory(GetTestClientMetadata())
        ->Then(GetCurrentSerialEventTarget(), __func__,
               [&done](const ClientDirectoryLockPromise::ResolveOrRejectValue&
                           aValue) {
                 QuotaManager* quotaManager = QuotaManager::Get();
                 ASSERT_TRUE(quotaManager);

                 ASSERT_TRUE(quotaManager->IsStorageInitialized());

                 done = true;
               });

    SpinEventLoopUntil("Promise is fulfilled"_ns, [&done]() { return done; });

    RefPtr<ClientDirectoryLock> directoryLock =
        quotaManager->CreateDirectoryLock(GetOtherTestClientMetadata(),
                                          /* aExclusive */ true);

    done = false;

    directoryLock->Acquire()->Then(
        GetCurrentSerialEventTarget(), __func__,
        [&done](const BoolPromise::ResolveOrRejectValue& aValue) {
          done = true;
        });

    SpinEventLoopUntil("Promise is fulfilled"_ns, [&done]() { return done; });

    done = false;

    quotaManager->OpenClientDirectory(GetTestClientMetadata())
        ->Then(GetCurrentSerialEventTarget(), __func__,
               [&done](const ClientDirectoryLockPromise::ResolveOrRejectValue&
                           aValue) {
                 QuotaManager* quotaManager = QuotaManager::Get();
                 ASSERT_TRUE(quotaManager);

                 ASSERT_TRUE(quotaManager->IsStorageInitialized());

                 done = true;
               });

    SpinEventLoopUntil("Promise is fulfilled"_ns, [&done]() { return done; });
  });

  ASSERT_NO_FATAL_FAILURE(AssertStorageInitialized());

  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());
}

// Test simple InitializeStorage.
TEST_F(TestQuotaManager, InitializeStorage_Simple) {
  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());

  ASSERT_NO_FATAL_FAILURE(AssertStorageNotInitialized());

  PerformOnBackgroundThread([]() {
    QuotaManager* quotaManager = QuotaManager::Get();
    ASSERT_TRUE(quotaManager);

    bool done = false;

    quotaManager->InitializeStorage()->Then(
        GetCurrentSerialEventTarget(), __func__,
        [&done](const BoolPromise::ResolveOrRejectValue& aValue) {
          QuotaManager* quotaManager = QuotaManager::Get();
          ASSERT_TRUE(quotaManager);

          ASSERT_TRUE(quotaManager->IsStorageInitialized());

          done = true;
        });

    SpinEventLoopUntil("Promise is fulfilled"_ns, [&done]() { return done; });
  });

  ASSERT_NO_FATAL_FAILURE(AssertStorageInitialized());

  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());
}

// Test InitializeStorage when a storage initialization is already ongoing.
TEST_F(TestQuotaManager, InitializeStorage_Ongoing) {
  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());

  ASSERT_NO_FATAL_FAILURE(AssertStorageNotInitialized());

  PerformOnBackgroundThread([]() {
    QuotaManager* quotaManager = QuotaManager::Get();
    ASSERT_TRUE(quotaManager);

    nsTArray<RefPtr<BoolPromise>> promises;

    promises.AppendElement(quotaManager->InitializeStorage());
    promises.AppendElement(quotaManager->InitializeStorage());

    bool done = false;

    BoolPromise::All(GetCurrentSerialEventTarget(), promises)
        ->Then(
            GetCurrentSerialEventTarget(), __func__,
            [&done](const CopyableTArray<bool>& aResolveValues) {
              QuotaManager* quotaManager = QuotaManager::Get();
              ASSERT_TRUE(quotaManager);

              ASSERT_TRUE(quotaManager->IsStorageInitialized());

              done = true;
            },
            [&done](nsresult aRejectValue) {
              ASSERT_TRUE(false);

              done = true;
            });

    SpinEventLoopUntil("Promise is fulfilled"_ns, [&done]() { return done; });
  });

  ASSERT_NO_FATAL_FAILURE(AssertStorageInitialized());

  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());
}

// Test InitializeStorage when a storage initialization is already ongoing and
// storage shutdown is scheduled after that.
TEST_F(TestQuotaManager, InitializeStorage_OngoingWithScheduledShutdown) {
  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());

  ASSERT_NO_FATAL_FAILURE(AssertStorageNotInitialized());

  PerformOnBackgroundThread([]() {
    QuotaManager* quotaManager = QuotaManager::Get();
    ASSERT_TRUE(quotaManager);

    nsTArray<RefPtr<BoolPromise>> promises;

    promises.AppendElement(quotaManager->InitializeStorage());
    promises.AppendElement(quotaManager->ShutdownStorage());
    promises.AppendElement(quotaManager->InitializeStorage());

    bool done = false;

    BoolPromise::All(GetCurrentSerialEventTarget(), promises)
        ->Then(
            GetCurrentSerialEventTarget(), __func__,
            [&done](const CopyableTArray<bool>& aResolveValues) {
              QuotaManager* quotaManager = QuotaManager::Get();
              ASSERT_TRUE(quotaManager);

              ASSERT_TRUE(quotaManager->IsStorageInitialized());

              done = true;
            },
            [&done](nsresult aRejectValue) {
              ASSERT_TRUE(false);

              done = true;
            });

    SpinEventLoopUntil("Promise is fulfilled"_ns, [&done]() { return done; });
  });

  ASSERT_NO_FATAL_FAILURE(AssertStorageInitialized());

  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());
}

// Test InitializeStorage when a storage initialization is already ongoing and
// an exclusive directory lock is requested after that.
TEST_F(TestQuotaManager, InitializeStorage_OngoingWithExclusiveDirectoryLock) {
  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());

  ASSERT_NO_FATAL_FAILURE(AssertStorageNotInitialized());

  PerformOnBackgroundThread([]() {
    QuotaManager* quotaManager = QuotaManager::Get();
    ASSERT_TRUE(quotaManager);

    RefPtr<UniversalDirectoryLock> directoryLock =
        quotaManager->CreateDirectoryLockInternal(Nullable<PersistenceType>(),
                                                  OriginScope::FromNull(),
                                                  Nullable<Client::Type>(),
                                                  /* aExclusive */ true);

    nsTArray<RefPtr<BoolPromise>> promises;

    promises.AppendElement(quotaManager->InitializeStorage()->Then(
        GetCurrentSerialEventTarget(), __func__,
        [&directoryLock](const BoolPromise::ResolveOrRejectValue& aValue) {
          // The exclusive directory lock must be released when the first
          // storage initialization is finished, otherwise it would endlessly
          // block the second storage initialization.
          directoryLock = nullptr;

          if (aValue.IsReject()) {
            return BoolPromise::CreateAndReject(aValue.RejectValue(), __func__);
          }

          return BoolPromise::CreateAndResolve(true, __func__);
        }));
    promises.AppendElement(directoryLock->Acquire());
    promises.AppendElement(quotaManager->InitializeStorage());

    bool done = false;

    BoolPromise::All(GetCurrentSerialEventTarget(), promises)
        ->Then(
            GetCurrentSerialEventTarget(), __func__,
            [&done](const CopyableTArray<bool>& aResolveValues) {
              QuotaManager* quotaManager = QuotaManager::Get();
              ASSERT_TRUE(quotaManager);

              ASSERT_TRUE(quotaManager->IsStorageInitialized());

              done = true;
            },
            [&done](nsresult aRejectValue) {
              ASSERT_TRUE(false);

              done = true;
            });

    SpinEventLoopUntil("Promise is fulfilled"_ns, [&done]() { return done; });
  });

  ASSERT_NO_FATAL_FAILURE(AssertStorageInitialized());

  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());
}

// Test InitializeStorage when a storage initialization is already ongoing and
// shared client directory locks are requested after that.
// The shared client directory locks don't have to be released in this case.
TEST_F(TestQuotaManager, InitializeStorage_OngoingWithClientDirectoryLocks) {
  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());

  ASSERT_NO_FATAL_FAILURE(AssertStorageNotInitialized());

  PerformOnBackgroundThread([]() {
    QuotaManager* quotaManager = QuotaManager::Get();
    ASSERT_TRUE(quotaManager);

    RefPtr<ClientDirectoryLock> directoryLock =
        quotaManager->CreateDirectoryLock(GetTestClientMetadata(),
                                          /* aExclusive */ false);

    RefPtr<ClientDirectoryLock> directoryLock2 =
        quotaManager->CreateDirectoryLock(GetTestClientMetadata(),
                                          /* aExclusive */ false);

    nsTArray<RefPtr<BoolPromise>> promises;

    promises.AppendElement(quotaManager->InitializeStorage());
    promises.AppendElement(directoryLock->Acquire());
    promises.AppendElement(quotaManager->InitializeStorage());
    promises.AppendElement(directoryLock2->Acquire());

    bool done = false;

    BoolPromise::All(GetCurrentSerialEventTarget(), promises)
        ->Then(
            GetCurrentSerialEventTarget(), __func__,
            [&done](const CopyableTArray<bool>& aResolveValues) {
              QuotaManager* quotaManager = QuotaManager::Get();
              ASSERT_TRUE(quotaManager);

              ASSERT_TRUE(quotaManager->IsStorageInitialized());

              done = true;
            },
            [&done](nsresult aRejectValue) {
              ASSERT_TRUE(false);

              done = true;
            });

    SpinEventLoopUntil("Promise is fulfilled"_ns, [&done]() { return done; });
  });

  ASSERT_NO_FATAL_FAILURE(AssertStorageInitialized());

  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());
}

// Test InitializeStorage when a storage initialization is already ongoing and
// shared client directory locks are requested after that with storage shutdown
// scheduled in between.
TEST_F(TestQuotaManager,
       InitializeStorage_OngoingWithClientDirectoryLocksAndScheduledShutdown) {
  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());

  ASSERT_NO_FATAL_FAILURE(AssertStorageNotInitialized());

  PerformOnBackgroundThread([]() {
    QuotaManager* quotaManager = QuotaManager::Get();
    ASSERT_TRUE(quotaManager);

    RefPtr<ClientDirectoryLock> directoryLock =
        quotaManager->CreateDirectoryLock(GetTestClientMetadata(),
                                          /* aExclusive */ false);

    directoryLock->OnInvalidate(
        [&directoryLock]() { directoryLock = nullptr; });

    RefPtr<ClientDirectoryLock> directoryLock2 =
        quotaManager->CreateDirectoryLock(GetTestClientMetadata(),
                                          /* aExclusive */ false);

    nsTArray<RefPtr<BoolPromise>> promises;

    promises.AppendElement(quotaManager->InitializeStorage());
    promises.AppendElement(directoryLock->Acquire());
    promises.AppendElement(quotaManager->ShutdownStorage());
    promises.AppendElement(quotaManager->InitializeStorage());
    promises.AppendElement(directoryLock2->Acquire());

    bool done = false;

    BoolPromise::All(GetCurrentSerialEventTarget(), promises)
        ->Then(
            GetCurrentSerialEventTarget(), __func__,
            [&done](const CopyableTArray<bool>& aResolveValues) {
              QuotaManager* quotaManager = QuotaManager::Get();
              ASSERT_TRUE(quotaManager);

              ASSERT_TRUE(quotaManager->IsStorageInitialized());

              done = true;
            },
            [&done](nsresult aRejectValue) {
              ASSERT_TRUE(false);

              done = true;
            });

    SpinEventLoopUntil("Promise is fulfilled"_ns, [&done]() { return done; });
  });

  ASSERT_NO_FATAL_FAILURE(AssertStorageInitialized());

  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());
}

// Test InitializeStorage when a storage initialization already finished.
TEST_F(TestQuotaManager, InitializeStorage_Finished) {
  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());

  ASSERT_NO_FATAL_FAILURE(AssertStorageNotInitialized());

  PerformOnBackgroundThread([]() {
    QuotaManager* quotaManager = QuotaManager::Get();
    ASSERT_TRUE(quotaManager);

    bool done = false;

    quotaManager->InitializeStorage()->Then(
        GetCurrentSerialEventTarget(), __func__,
        [&done](const BoolPromise::ResolveOrRejectValue& aValue) {
          QuotaManager* quotaManager = QuotaManager::Get();
          ASSERT_TRUE(quotaManager);

          ASSERT_TRUE(quotaManager->IsStorageInitialized());

          done = true;
        });

    SpinEventLoopUntil("Promise is fulfilled"_ns, [&done]() { return done; });

    done = false;

    quotaManager->InitializeStorage()->Then(
        GetCurrentSerialEventTarget(), __func__,
        [&done](const BoolPromise::ResolveOrRejectValue& aValue) {
          QuotaManager* quotaManager = QuotaManager::Get();
          ASSERT_TRUE(quotaManager);

          ASSERT_TRUE(quotaManager->IsStorageInitialized());

          done = true;
        });

    SpinEventLoopUntil("Promise is fulfilled"_ns, [&done]() { return done; });
  });

  ASSERT_NO_FATAL_FAILURE(AssertStorageInitialized());

  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());
}

// Test InitializeStorage when a storage initialization already finished but
// storage shutdown has just been scheduled.
TEST_F(TestQuotaManager, InitializeStorage_FinishedWithScheduledShutdown) {
  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());

  ASSERT_NO_FATAL_FAILURE(AssertStorageNotInitialized());

  PerformOnBackgroundThread([]() {
    QuotaManager* quotaManager = QuotaManager::Get();
    ASSERT_TRUE(quotaManager);

    bool done = false;

    quotaManager->InitializeStorage()->Then(
        GetCurrentSerialEventTarget(), __func__,
        [&done](const BoolPromise::ResolveOrRejectValue& aValue) {
          QuotaManager* quotaManager = QuotaManager::Get();
          ASSERT_TRUE(quotaManager);

          ASSERT_TRUE(quotaManager->IsStorageInitialized());

          done = true;
        });

    SpinEventLoopUntil("Promise is fulfilled"_ns, [&done]() { return done; });

    nsTArray<RefPtr<BoolPromise>> promises;

    promises.AppendElement(quotaManager->ShutdownStorage());
    promises.AppendElement(quotaManager->InitializeStorage());

    done = false;

    BoolPromise::All(GetCurrentSerialEventTarget(), promises)
        ->Then(
            GetCurrentSerialEventTarget(), __func__,
            [&done](const CopyableTArray<bool>& aResolveValues) {
              QuotaManager* quotaManager = QuotaManager::Get();
              ASSERT_TRUE(quotaManager);

              ASSERT_TRUE(quotaManager->IsStorageInitialized());

              done = true;
            },
            [&done](nsresult aRejectValue) {
              ASSERT_TRUE(false);

              done = true;
            });

    SpinEventLoopUntil("Promise is fulfilled"_ns, [&done]() { return done; });
  });

  ASSERT_NO_FATAL_FAILURE(AssertStorageInitialized());

  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());
}

// Test InitializeStorage when a storage initialization already finished and
// shared client directory locks are requested immediately after requesting
// storage initialization.
TEST_F(TestQuotaManager, InitializeStorage_FinishedWithClientDirectoryLocks) {
  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());

  ASSERT_NO_FATAL_FAILURE(AssertStorageNotInitialized());

  PerformOnBackgroundThread([]() {
    QuotaManager* quotaManager = QuotaManager::Get();
    ASSERT_TRUE(quotaManager);

    RefPtr<ClientDirectoryLock> directoryLock =
        quotaManager->CreateDirectoryLock(GetTestClientMetadata(),
                                          /* aExclusive */ false);

    nsTArray<RefPtr<BoolPromise>> promises;

    promises.AppendElement(quotaManager->InitializeStorage());
    promises.AppendElement(directoryLock->Acquire());

    bool done = false;

    BoolPromise::All(GetCurrentSerialEventTarget(), promises)
        ->Then(
            GetCurrentSerialEventTarget(), __func__,
            [&done](const CopyableTArray<bool>& aResolveValues) {
              QuotaManager* quotaManager = QuotaManager::Get();
              ASSERT_TRUE(quotaManager);

              ASSERT_TRUE(quotaManager->IsStorageInitialized());

              done = true;
            },
            [&done](nsresult aRejectValue) {
              ASSERT_TRUE(false);

              done = true;
            });

    SpinEventLoopUntil("Promise is fulfilled"_ns, [&done]() { return done; });

    RefPtr<ClientDirectoryLock> directoryLock2 =
        quotaManager->CreateDirectoryLock(GetTestClientMetadata(),
                                          /* aExclusive */ false);

    promises.Clear();

    promises.AppendElement(quotaManager->InitializeStorage());
    promises.AppendElement(directoryLock2->Acquire());

    done = false;

    BoolPromise::All(GetCurrentSerialEventTarget(), promises)
        ->Then(
            GetCurrentSerialEventTarget(), __func__,
            [&done](const CopyableTArray<bool>& aResolveValues) {
              QuotaManager* quotaManager = QuotaManager::Get();
              ASSERT_TRUE(quotaManager);

              ASSERT_TRUE(quotaManager->IsStorageInitialized());

              done = true;
            },
            [&done](nsresult aRejectValue) {
              ASSERT_TRUE(false);

              done = true;
            });

    SpinEventLoopUntil("Promise is fulfilled"_ns, [&done]() { return done; });
  });

  ASSERT_NO_FATAL_FAILURE(AssertStorageInitialized());

  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());
}

// Test InitializeStorage when a storage initialization already finished and
// shared client directory locks are requested immediatelly after requesting
// storage initialization with storage shutdown performed in between.
// The shared client directory lock is released when it gets invalidated by
// storage shutdown which then unblocks the shutdown.
TEST_F(TestQuotaManager,
       InitializeStorage_FinishedWithClientDirectoryLocksAndScheduledShutdown) {
  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());

  ASSERT_NO_FATAL_FAILURE(AssertStorageNotInitialized());

  PerformOnBackgroundThread([]() {
    QuotaManager* quotaManager = QuotaManager::Get();
    ASSERT_TRUE(quotaManager);

    RefPtr<ClientDirectoryLock> directoryLock =
        quotaManager->CreateDirectoryLock(GetTestClientMetadata(),
                                          /* aExclusive */ false);

    directoryLock->OnInvalidate(
        [&directoryLock]() { directoryLock = nullptr; });

    nsTArray<RefPtr<BoolPromise>> promises;

    promises.AppendElement(quotaManager->InitializeStorage());
    promises.AppendElement(directoryLock->Acquire());

    bool done = false;

    BoolPromise::All(GetCurrentSerialEventTarget(), promises)
        ->Then(
            GetCurrentSerialEventTarget(), __func__,
            [&done](const CopyableTArray<bool>& aResolveValues) {
              QuotaManager* quotaManager = QuotaManager::Get();
              ASSERT_TRUE(quotaManager);

              ASSERT_TRUE(quotaManager->IsStorageInitialized());

              done = true;
            },
            [&done](nsresult aRejectValue) {
              ASSERT_TRUE(false);

              done = true;
            });

    SpinEventLoopUntil("Promise is fulfilled"_ns, [&done]() { return done; });

    done = false;

    quotaManager->ShutdownStorage()->Then(
        GetCurrentSerialEventTarget(), __func__,
        [&done](const BoolPromise::ResolveOrRejectValue& aValue) {
          QuotaManager* quotaManager = QuotaManager::Get();
          ASSERT_TRUE(quotaManager);

          ASSERT_FALSE(quotaManager->IsStorageInitialized());

          done = true;
        });

    SpinEventLoopUntil("Promise is fulfilled"_ns, [&done]() { return done; });

    RefPtr<ClientDirectoryLock> directoryLock2 =
        quotaManager->CreateDirectoryLock(GetTestClientMetadata(),
                                          /* aExclusive */ false);

    promises.Clear();

    promises.AppendElement(quotaManager->InitializeStorage());
    promises.AppendElement(directoryLock2->Acquire());

    done = false;

    BoolPromise::All(GetCurrentSerialEventTarget(), promises)
        ->Then(
            GetCurrentSerialEventTarget(), __func__,
            [&done](const CopyableTArray<bool>& aResolveValues) {
              QuotaManager* quotaManager = QuotaManager::Get();
              ASSERT_TRUE(quotaManager);

              ASSERT_TRUE(quotaManager->IsStorageInitialized());

              done = true;
            },
            [&done](nsresult aRejectValue) {
              ASSERT_TRUE(false);

              done = true;
            });

    SpinEventLoopUntil("Promise is fulfilled"_ns, [&done]() { return done; });
  });

  ASSERT_NO_FATAL_FAILURE(AssertStorageInitialized());

  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());
}

TEST_F(TestQuotaManager,
       InitializeTemporaryStorage_FinishedWithScheduledShutdown) {
  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());

  ASSERT_NO_FATAL_FAILURE(AssertStorageNotInitialized());

  PerformOnBackgroundThread([]() {
    nsTArray<RefPtr<BoolPromise>> promises;

    QuotaManager* quotaManager = QuotaManager::Get();
    ASSERT_TRUE(quotaManager);

    promises.AppendElement(quotaManager->InitializeStorage());
    promises.AppendElement(quotaManager->InitializeTemporaryStorage());

    bool done = false;

    BoolPromise::All(GetCurrentSerialEventTarget(), promises)
        ->Then(
            GetCurrentSerialEventTarget(), __func__,
            [&done](const CopyableTArray<bool>& aResolveValues) {
              QuotaManager* quotaManager = QuotaManager::Get();
              ASSERT_TRUE(quotaManager);

              ASSERT_TRUE(quotaManager->IsStorageInitialized());
              ASSERT_TRUE(quotaManager->IsTemporaryStorageInitialized());

              done = true;
            },
            [&done](nsresult aRejectValue) {
              ASSERT_TRUE(false);

              done = true;
            });

    SpinEventLoopUntil("Promise is fulfilled"_ns, [&done]() { return done; });

    promises.Clear();

    promises.AppendElement(quotaManager->ShutdownStorage());
    promises.AppendElement(quotaManager->InitializeStorage());
    promises.AppendElement(quotaManager->InitializeTemporaryStorage());

    done = false;

    BoolPromise::All(GetCurrentSerialEventTarget(), promises)
        ->Then(
            GetCurrentSerialEventTarget(), __func__,
            [&done](const CopyableTArray<bool>& aResolveValues) {
              QuotaManager* quotaManager = QuotaManager::Get();
              ASSERT_TRUE(quotaManager);

              ASSERT_TRUE(quotaManager->IsStorageInitialized());
              ASSERT_TRUE(quotaManager->IsTemporaryStorageInitialized());

              done = true;
            },
            [&done](nsresult aRejectValue) {
              ASSERT_TRUE(false);

              done = true;
            });

    SpinEventLoopUntil("Promise is fulfilled"_ns, [&done]() { return done; });
  });

  ASSERT_NO_FATAL_FAILURE(AssertStorageInitialized());

  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());
}

// Test simple ShutdownStorage.
TEST_F(TestQuotaManager, ShutdownStorage_Simple) {
  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());

  ASSERT_NO_FATAL_FAILURE(AssertStorageNotInitialized());

  ASSERT_NO_FATAL_FAILURE(InitializeStorage());

  ASSERT_NO_FATAL_FAILURE(AssertStorageInitialized());

  PerformOnBackgroundThread([]() {
    QuotaManager* quotaManager = QuotaManager::Get();
    ASSERT_TRUE(quotaManager);

    bool done = false;

    quotaManager->ShutdownStorage()->Then(
        GetCurrentSerialEventTarget(), __func__,
        [&done](const BoolPromise::ResolveOrRejectValue& aValue) {
          QuotaManager* quotaManager = QuotaManager::Get();
          ASSERT_TRUE(quotaManager);

          ASSERT_FALSE(quotaManager->IsStorageInitialized());

          done = true;
        });

    SpinEventLoopUntil("Promise is fulfilled"_ns, [&done]() { return done; });
  });

  ASSERT_NO_FATAL_FAILURE(AssertStorageNotInitialized());

  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());
}

// Test ShutdownStorage when a storage shutdown is already ongoing.
TEST_F(TestQuotaManager, ShutdownStorage_Ongoing) {
  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());

  ASSERT_NO_FATAL_FAILURE(AssertStorageNotInitialized());

  ASSERT_NO_FATAL_FAILURE(InitializeStorage());

  ASSERT_NO_FATAL_FAILURE(AssertStorageInitialized());

  PerformOnBackgroundThread([]() {
    QuotaManager* quotaManager = QuotaManager::Get();
    ASSERT_TRUE(quotaManager);

    nsTArray<RefPtr<BoolPromise>> promises;

    promises.AppendElement(quotaManager->ShutdownStorage());
    promises.AppendElement(quotaManager->ShutdownStorage());

    bool done = false;

    BoolPromise::All(GetCurrentSerialEventTarget(), promises)
        ->Then(
            GetCurrentSerialEventTarget(), __func__,
            [&done](const CopyableTArray<bool>& aResolveValues) {
              QuotaManager* quotaManager = QuotaManager::Get();
              ASSERT_TRUE(quotaManager);

              ASSERT_FALSE(quotaManager->IsStorageInitialized());

              done = true;
            },
            [&done](nsresult aRejectValue) {
              ASSERT_TRUE(false);

              done = true;
            });

    SpinEventLoopUntil("Promise is fulfilled"_ns, [&done]() { return done; });
  });

  ASSERT_NO_FATAL_FAILURE(AssertStorageNotInitialized());

  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());
}

// Test ShutdownStorage when a storage shutdown is already ongoing and storage
// initialization is scheduled after that.
TEST_F(TestQuotaManager, ShutdownStorage_OngoingWithScheduledInitialization) {
  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());

  ASSERT_NO_FATAL_FAILURE(AssertStorageNotInitialized());

  ASSERT_NO_FATAL_FAILURE(InitializeStorage());

  ASSERT_NO_FATAL_FAILURE(AssertStorageInitialized());

  PerformOnBackgroundThread([]() {
    QuotaManager* quotaManager = QuotaManager::Get();
    ASSERT_TRUE(quotaManager);

    nsTArray<RefPtr<BoolPromise>> promises;

    promises.AppendElement(quotaManager->ShutdownStorage());
    promises.AppendElement(quotaManager->InitializeStorage());
    promises.AppendElement(quotaManager->ShutdownStorage());

    bool done = false;

    BoolPromise::All(GetCurrentSerialEventTarget(), promises)
        ->Then(
            GetCurrentSerialEventTarget(), __func__,
            [&done](const CopyableTArray<bool>& aResolveValues) {
              QuotaManager* quotaManager = QuotaManager::Get();
              ASSERT_TRUE(quotaManager);

              ASSERT_FALSE(quotaManager->IsStorageInitialized());

              done = true;
            },
            [&done](nsresult aRejectValue) {
              ASSERT_TRUE(false);

              done = true;
            });

    SpinEventLoopUntil("Promise is fulfilled"_ns, [&done]() { return done; });
  });

  ASSERT_NO_FATAL_FAILURE(AssertStorageNotInitialized());

  ASSERT_NO_FATAL_FAILURE(ShutdownStorage());
}

// Test ShutdownStorage when a storage shutdown is already ongoing and a shared
// client directory lock is requested after that.
// The shared client directory lock doesn't have to be explicitly released
// because it gets invalidated while it's still pending which causes that any
// directory locks that were blocked by the shared client directory lock become
// unblocked.
TEST_F(TestQuotaManager, ShutdownStorage_OngoingWithClientDirectoryLock) {
  PerformOnBackgroundThread([]() {
    QuotaManager* quotaManager = QuotaManager::Get();
    ASSERT_TRUE(quotaManager);

    RefPtr<ClientDirectoryLock> directoryLock =
        quotaManager->CreateDirectoryLock(GetTestClientMetadata(),
                                          /* aExclusive */ false);

    nsTArray<RefPtr<BoolPromise>> promises;

    // This creates an exclusive directory lock internally.
    promises.AppendElement(quotaManager->ShutdownStorage());

    // This directory lock can't be acquired yet because a storage shutdown
    // (which uses an exclusive diretory lock internall) is ongoing.
    promises.AppendElement(directoryLock->Acquire());

    // This second ShutdownStorage invalidates the directoryLock, so that
    // directory lock can't ever be successfully acquired, the promise for it
    // will be rejected when the first ShutdownStorage is finished (it releases
    // its exclusive directory lock);
    promises.AppendElement(quotaManager->ShutdownStorage());

    bool done = false;

    BoolPromise::AllSettled(GetCurrentSerialEventTarget(), promises)
        ->Then(
            GetCurrentSerialEventTarget(), __func__,
            [&done](
                const BoolPromise::AllSettledPromiseType::ResolveOrRejectValue&
                    aValues) { done = true; });

    SpinEventLoopUntil("Promise is fulfilled"_ns, [&done]() { return done; });
  });
}

}  // namespace mozilla::dom::quota::test
