/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileSystemDataManager.h"
#include "gtest/gtest.h"
#include "mozIStorageService.h"
#include "mozStorageCID.h"
#include "mozilla/SpinEventLoopUntil.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/dom/quota/QuotaManager.h"
#include "mozilla/dom/quota/QuotaManagerService.h"
#include "nsIPrefBranch.h"
#include "nsIPrefService.h"
#include "nsIQuotaCallbacks.h"
#include "nsIQuotaRequests.h"

namespace mozilla::dom::fs::test {

namespace {

quota::OriginMetadata GetTestOriginMetadata() {
  return quota::OriginMetadata{""_ns, "example.com"_ns, "http://example.com"_ns,
                               quota::PERSISTENCE_TYPE_DEFAULT};
}

}  // namespace

class TestFileSystemDataManager : public ::testing::Test {
 public:
  class RequestResolver final : public nsIQuotaCallback {
   public:
    RequestResolver() : mDone(false) {}

    bool Done() const { return mDone; }

    NS_DECL_ISUPPORTS

    NS_IMETHOD OnComplete(nsIQuotaRequest* aRequest) override {
      mDone = true;

      return NS_OK;
    }

   private:
    ~RequestResolver() = default;

    bool mDone;
  };

  static void SetUpTestCase() {
    // The first initialization of storage service must be done on the main
    // thread.
    nsCOMPtr<mozIStorageService> storageService =
        do_GetService(MOZ_STORAGE_SERVICE_CONTRACTID);
    ASSERT_TRUE(storageService);

    nsIObserver* observer = quota::QuotaManager::GetObserver();
    ASSERT_TRUE(observer);

    nsresult rv = observer->Observe(nullptr, "profile-do-change", nullptr);
    ASSERT_TRUE(NS_SUCCEEDED(rv));

    AutoJSAPI jsapi;

    bool ok = jsapi.Init(xpc::PrivilegedJunkScope());
    ASSERT_TRUE(ok);

    nsCOMPtr<nsIQuotaManagerService> qms =
        quota::QuotaManagerService::GetOrCreate();
    ASSERT_TRUE(qms);

    // QuotaManagerService::Initialized fails if the testing pref is not set.
    nsCOMPtr<nsIPrefBranch> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);

    prefs->SetBoolPref("dom.quotaManager.testing", true);

    nsCOMPtr<nsIQuotaRequest> request;
    rv = qms->StorageInitialized(getter_AddRefs(request));
    ASSERT_TRUE(NS_SUCCEEDED(rv));

    prefs->SetBoolPref("dom.quotaManager.testing", false);

    RefPtr<RequestResolver> resolver = new RequestResolver();

    rv = request->SetCallback(resolver);
    ASSERT_TRUE(NS_SUCCEEDED(rv));

    SpinEventLoopUntil("Promise is fulfilled"_ns,
                       [&resolver]() { return resolver->Done(); });

    quota::QuotaManager* quotaManager = quota::QuotaManager::Get();
    ASSERT_TRUE(quotaManager);

    ASSERT_TRUE(quotaManager->OwningThread());

    sBackgroundTarget = quotaManager->OwningThread();
  }

  static void TearDownTestCase() {
    nsIObserver* observer = quota::QuotaManager::GetObserver();
    ASSERT_TRUE(observer);

    nsresult rv =
        observer->Observe(nullptr, "profile-before-change-qm", nullptr);
    ASSERT_TRUE(NS_SUCCEEDED(rv));

    bool done = false;

    InvokeAsync(sBackgroundTarget, __func__,
                []() {
                  quota::QuotaManager::Reset();

                  return BoolPromise::CreateAndResolve(true, __func__);
                })
        ->Then(GetCurrentSerialEventTarget(), __func__,
               [&done](const BoolPromise::ResolveOrRejectValue& value) {
                 done = true;
               });

    SpinEventLoopUntil("Promise is fulfilled"_ns, [&done]() { return done; });

    sBackgroundTarget = nullptr;
  }

  static const nsCOMPtr<nsISerialEventTarget>& BackgroundTargetStrongRef() {
    return sBackgroundTarget;
  }

 private:
  static nsCOMPtr<nsISerialEventTarget> sBackgroundTarget;
};

NS_IMPL_ISUPPORTS(TestFileSystemDataManager::RequestResolver, nsIQuotaCallback)

nsCOMPtr<nsISerialEventTarget> TestFileSystemDataManager::sBackgroundTarget;

TEST_F(TestFileSystemDataManager, GetOrCreateFileSystemDataManager) {
  auto backgroundTask = []() -> RefPtr<BoolPromise> {
    bool done = false;

    data::FileSystemDataManager::GetOrCreateFileSystemDataManager(
        GetTestOriginMetadata())
        ->Then(
            GetCurrentSerialEventTarget(), __func__,
            [](Registered<data::FileSystemDataManager> registeredDataManager) {
              RefPtr<data::FileSystemDataManager> dataManager =
                  registeredDataManager.get();

              registeredDataManager = nullptr;

              return dataManager->OnClose();
            },
            [](nsresult rejectValue) {
              return BoolPromise::CreateAndReject(rejectValue, __func__);
            })
        ->Then(
            GetCurrentSerialEventTarget(), __func__,
            [&done](const BoolPromise::ResolveOrRejectValue&) { done = true; });

    SpinEventLoopUntil("Promise is fulfilled"_ns, [&done]() { return done; });

    return BoolPromise::CreateAndResolve(true, __func__);
  };

  bool done = false;

  InvokeAsync(BackgroundTargetStrongRef(), __func__, std::move(backgroundTask))
      ->Then(GetCurrentSerialEventTarget(), __func__,
             [&done](const BoolPromise::ResolveOrRejectValue& value) {
               done = true;
             });

  SpinEventLoopUntil("Promise is fulfilled"_ns, [&done]() { return done; });
}

TEST_F(TestFileSystemDataManager,
       GetOrCreateFileSystemDataManager_PendingOpen) {
  auto backgroundTask = []() -> RefPtr<BoolPromise> {
    Registered<data::FileSystemDataManager> rdm1;

    Registered<data::FileSystemDataManager> rdm2;

    {
      bool done1 = false;

      data::FileSystemDataManager::GetOrCreateFileSystemDataManager(
          GetTestOriginMetadata())
          ->Then(
              GetCurrentSerialEventTarget(), __func__,
              [&rdm1, &done1](Registered<data::FileSystemDataManager>
                                  registeredDataManager) {
                ASSERT_TRUE(registeredDataManager->IsOpen());

                rdm1 = std::move(registeredDataManager);

                done1 = true;
              },
              [&done1](nsresult rejectValue) { done1 = true; });

      bool done2 = false;

      data::FileSystemDataManager::GetOrCreateFileSystemDataManager(
          GetTestOriginMetadata())
          ->Then(
              GetCurrentSerialEventTarget(), __func__,
              [&rdm2, &done2](Registered<data::FileSystemDataManager>
                                  registeredDataManager) {
                ASSERT_TRUE(registeredDataManager->IsOpen());

                rdm2 = std::move(registeredDataManager);

                done2 = true;
              },
              [&done2](nsresult rejectValue) { done2 = true; });

      SpinEventLoopUntil("Promise is fulfilled"_ns,
                         [&done1, &done2]() { return done1 && done2; });
    }

    RefPtr<data::FileSystemDataManager> dm1 = rdm1.unwrap();

    RefPtr<data::FileSystemDataManager> dm2 = rdm2.unwrap();

    {
      bool done1 = false;

      dm1->OnClose()->Then(
          GetCurrentSerialEventTarget(), __func__,
          [&done1](const BoolPromise::ResolveOrRejectValue&) { done1 = true; });

      bool done2 = false;

      dm2->OnClose()->Then(
          GetCurrentSerialEventTarget(), __func__,
          [&done2](const BoolPromise::ResolveOrRejectValue&) { done2 = true; });

      SpinEventLoopUntil("Promise is fulfilled"_ns,
                         [&done1, &done2]() { return done1 && done2; });

      return BoolPromise::CreateAndResolve(true, __func__);
    }
  };

  bool done = false;

  InvokeAsync(BackgroundTargetStrongRef(), __func__, std::move(backgroundTask))
      ->Then(GetCurrentSerialEventTarget(), __func__,
             [&done](const BoolPromise::ResolveOrRejectValue& value) {
               done = true;
             });

  SpinEventLoopUntil("Promise is fulfilled"_ns, [&done]() { return done; });
}

TEST_F(TestFileSystemDataManager,
       GetOrCreateFileSystemDataManager_PendingClose) {
  auto backgroundTask = []() -> RefPtr<BoolPromise> {
    Registered<data::FileSystemDataManager> rdm;

    {
      bool done = false;

      data::FileSystemDataManager::GetOrCreateFileSystemDataManager(
          GetTestOriginMetadata())
          ->Then(
              GetCurrentSerialEventTarget(), __func__,
              [&rdm, &done](Registered<data::FileSystemDataManager>
                                registeredDataManager) {
                ASSERT_TRUE(registeredDataManager->IsOpen());

                rdm = std::move(registeredDataManager);

                done = true;
              },
              [&done](nsresult rejectValue) { done = true; });

      SpinEventLoopUntil("Promise is fulfilled"_ns, [&done]() { return done; });
    }

    RefPtr<data::FileSystemDataManager> dm = rdm.unwrap();

    Unused << dm;

    {
      bool done = false;

      data::FileSystemDataManager::GetOrCreateFileSystemDataManager(
          GetTestOriginMetadata())
          ->Then(
              GetCurrentSerialEventTarget(), __func__,
              [](Registered<data::FileSystemDataManager>
                     registeredDataManager) {
                RefPtr<data::FileSystemDataManager> dataManager =
                    registeredDataManager.get();

                registeredDataManager = nullptr;

                return dataManager->OnClose();
              },
              [](nsresult rejectValue) {
                return BoolPromise::CreateAndReject(rejectValue, __func__);
              })
          ->Then(GetCurrentSerialEventTarget(), __func__,
                 [&done](const BoolPromise::ResolveOrRejectValue&) {
                   done = true;
                 });

      SpinEventLoopUntil("Promise is fulfilled"_ns, [&done]() { return done; });

      return BoolPromise::CreateAndResolve(true, __func__);
    }
  };

  bool done = false;

  InvokeAsync(BackgroundTargetStrongRef(), __func__, std::move(backgroundTask))
      ->Then(GetCurrentSerialEventTarget(), __func__,
             [&done](const BoolPromise::ResolveOrRejectValue& value) {
               done = true;
             });

  SpinEventLoopUntil("Promise is fulfilled"_ns, [&done]() { return done; });
}

}  // namespace mozilla::dom::fs::test
