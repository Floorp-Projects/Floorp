/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_FS_TEST_GTEST_QUOTAMANAGERDEPENDENCYFIXTURE_H_
#define DOM_FS_TEST_GTEST_QUOTAMANAGERDEPENDENCYFIXTURE_H_

#include "gtest/gtest.h"
#include "mozIStorageService.h"
#include "mozStorageCID.h"
#include "mozilla/SpinEventLoopUntil.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/dom/quota/QuotaCommon.h"
#include "mozilla/dom/quota/QuotaManager.h"
#include "mozilla/dom/quota/QuotaManagerService.h"
#include "mozilla/dom/quota/ResultExtensions.h"
#include "mozilla/gtest/MozAssertions.h"
#include "nsIPrefBranch.h"
#include "nsIPrefService.h"
#include "nsIQuotaCallbacks.h"
#include "nsIQuotaRequests.h"
#include "nsScriptSecurityManager.h"

namespace mozilla::dom::fs::test {

class QuotaManagerDependencyFixture : public ::testing::Test {
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

  virtual const quota::OriginMetadata& GetOriginMetadata() const;

 protected:
  void SetUp() override {
    // The first initialization of storage service must be done on the main
    // thread.
    nsCOMPtr<mozIStorageService> storageService =
        do_GetService(MOZ_STORAGE_SERVICE_CONTRACTID);
    ASSERT_TRUE(storageService);

    nsIObserver* observer = quota::QuotaManager::GetObserver();
    ASSERT_TRUE(observer);

    nsresult rv = observer->Observe(nullptr, "profile-do-change", nullptr);
    ASSERT_NS_SUCCEEDED(rv);

    AutoJSAPI jsapi;

    bool ok = jsapi.Init(xpc::PrivilegedJunkScope());
    ASSERT_TRUE(ok);

    quota::OriginMetadata originMeta = GetOriginMetadata();
    nsCOMPtr<nsIScriptSecurityManager> ssm =
        nsScriptSecurityManager::GetScriptSecurityManager();
    ASSERT_TRUE(ssm);

    nsCOMPtr<nsIPrincipal> principal;
    rv = ssm->CreateContentPrincipalFromOrigin(originMeta.mOrigin,
                                               getter_AddRefs(principal));
    ASSERT_NS_SUCCEEDED(rv);

    nsCOMPtr<nsIQuotaManagerService> qms =
        quota::QuotaManagerService::GetOrCreate();
    ASSERT_TRUE(qms);

    nsCOMPtr<nsIQuotaRequest> clearRequest;

    RefPtr<RequestResolver> clearResolver = new RequestResolver();
    ASSERT_TRUE(clearResolver);

    const nsAutoString clientType = NS_ConvertUTF8toUTF16(
        quota::Client::TypeToText(quota::Client::Type::FILESYSTEM));
    rv = qms->ClearStoragesForPrincipal(
        principal,
        quota::PersistenceTypeToString(
            quota::PersistenceType::PERSISTENCE_TYPE_DEFAULT),
        clientType,
        /* aClearAll */ true, getter_AddRefs(clearRequest));
    ASSERT_NS_SUCCEEDED(rv);

    rv = clearRequest->SetCallback(clearResolver);
    ASSERT_NS_SUCCEEDED(rv);

    SpinEventLoopUntil("Promise is fulfilled"_ns,
                       [&clearResolver]() { return clearResolver->Done(); });

    // QuotaManagerService::Initialized fails if the testing pref is not set.
    nsCOMPtr<nsIPrefBranch> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);

    prefs->SetBoolPref("dom.quotaManager.testing", true);

    nsCOMPtr<nsIQuotaRequest> request;
    rv = qms->StorageInitialized(getter_AddRefs(request));
    ASSERT_NS_SUCCEEDED(rv);

    RefPtr<RequestResolver> resolver = new RequestResolver();

    rv = request->SetCallback(resolver);
    ASSERT_NS_SUCCEEDED(rv);

    SpinEventLoopUntil("Promise is fulfilled"_ns,
                       [&resolver]() { return resolver->Done(); });

    prefs->SetBoolPref("dom.quotaManager.testing", false);

    quota::QuotaManager* quotaManager = quota::QuotaManager::Get();
    ASSERT_TRUE(quotaManager);

    ASSERT_TRUE(quotaManager->OwningThread());

    sBackgroundTarget = quotaManager->OwningThread();
  }

  void TearDown() override {
    nsIObserver* observer = quota::QuotaManager::GetObserver();
    ASSERT_TRUE(observer);

    nsresult rv =
        observer->Observe(nullptr, "profile-before-change-qm", nullptr);
    ASSERT_NS_SUCCEEDED(rv);

    PerformOnBackgroundTarget([]() {
      quota::QuotaManager::Reset();

      return BoolPromise::CreateAndResolve(true, __func__);
    });

    sBackgroundTarget = nullptr;
  }

  /* Convenience method for tasks which must be called on background target */
  template <class Invokable, class... Args>
  static void PerformOnBackgroundTarget(Invokable&& aInvokable,
                                        Args&&... aArgs) {
    bool done = false;
    auto boundTask =
        // For c++17, bind is cleaner than tuple for parameter pack forwarding
        // NOLINTNEXTLINE(modernize-avoid-bind)
        std::bind(std::forward<Invokable>(aInvokable),
                  std::forward<Args>(aArgs)...);
    InvokeAsync(BackgroundTargetStrongRef(), __func__, std::move(boundTask))
        ->Then(GetCurrentSerialEventTarget(), __func__,
               [&done](const BoolPromise::ResolveOrRejectValue& /* aValue */) {
                 done = true;
               });

    SpinEventLoopUntil("Promise is fulfilled"_ns, [&done]() { return done; });
  }

  /* Convenience method for tasks which must be executed on IO thread */
  template <class Invokable, class... Args>
  static void PerformOnIOThread(Invokable&& aInvokable, Args&&... aArgs) {
    quota::QuotaManager* quotaManager = quota::QuotaManager::Get();
    ASSERT_TRUE(quotaManager);

    bool done = false;
    auto boundTask =
        // For c++17, bind is cleaner than tuple for parameter pack forwarding
        // NOLINTNEXTLINE(modernize-avoid-bind)
        std::bind(std::forward<Invokable>(aInvokable),
                  std::forward<Args>(aArgs)...);
    InvokeAsync(quotaManager->IOThread(), __func__,
                [ioTask = std::move(boundTask)]() {
                  ioTask();
                  return BoolPromise::CreateAndResolve(true, __func__);
                })
        ->Then(GetCurrentSerialEventTarget(), __func__,
               [&done](const BoolPromise::ResolveOrRejectValue& value) {
                 done = true;
               });

    SpinEventLoopUntil("Promise is fulfilled"_ns, [&done]() { return done; });
  }

  static const nsCOMPtr<nsISerialEventTarget>& BackgroundTargetStrongRef() {
    return sBackgroundTarget;
  }

 private:
  static nsCOMPtr<nsISerialEventTarget> sBackgroundTarget;
};

}  // namespace mozilla::dom::fs::test

#endif  // DOM_FS_TEST_GTEST_QUOTAMANAGERDEPENDENCYFIXTURE_H_
