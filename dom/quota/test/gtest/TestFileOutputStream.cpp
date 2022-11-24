/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include "mozilla/dom/quota/Client.h"
#include "mozilla/dom/quota/CommonMetadata.h"
#include "mozilla/dom/quota/FileStreams.h"
#include "mozilla/dom/quota/QuotaManager.h"
#include "mozilla/dom/quota/QuotaManagerService.h"
#include "mozilla/dom/quota/QuotaCommon.h"
#include "mozilla/dom/quota/ResultExtensions.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/gtest/MozAssertions.h"
#include "mozilla/SpinEventLoopUntil.h"
#include "mozIStorageService.h"
#include "mozStorageCID.h"
#include "nsIPrefBranch.h"
#include "nsIPrefService.h"
#include "nsIQuotaCallbacks.h"
#include "nsIQuotaRequests.h"

namespace mozilla::dom::quota::test {

// TODO: Refactor this in the scope of Bug 1801364
class TestFileOutputStream : public ::testing::Test {
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
    ASSERT_NS_SUCCEEDED(rv);

    AutoJSAPI jsapi;

    bool ok = jsapi.Init(xpc::PrivilegedJunkScope());
    ASSERT_TRUE(ok);

    // QuotaManagerService::Initialized fails if the testing pref is not set.
    nsCOMPtr<nsIPrefBranch> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);

    prefs->SetBoolPref("dom.quotaManager.testing", true);

    prefs->SetIntPref("dom.quotaManager.temporaryStorage.fixedLimit",
                      mQuotaLimit);

    nsCOMPtr<nsIQuotaManagerService> qms =
        quota::QuotaManagerService::GetOrCreate();
    ASSERT_TRUE(qms);

    nsCOMPtr<nsIQuotaRequest> request;
    rv = qms->StorageInitialized(getter_AddRefs(request));
    ASSERT_NS_SUCCEEDED(rv);

    prefs->SetBoolPref("dom.quotaManager.testing", false);

    RefPtr<RequestResolver> resolver = new RequestResolver();

    rv = request->SetCallback(resolver);
    ASSERT_NS_SUCCEEDED(rv);

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
    ASSERT_NS_SUCCEEDED(rv);

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

  static nsCOMPtr<nsISerialEventTarget> sBackgroundTarget;

  static const int32_t mQuotaLimit = 8192;
};

NS_IMPL_ISUPPORTS(TestFileOutputStream::RequestResolver, nsIQuotaCallback)

nsCOMPtr<nsISerialEventTarget> TestFileOutputStream::sBackgroundTarget;

TEST_F(TestFileOutputStream, extendFileStreamWithSetEOF) {
  auto ioTask = []() {
    quota::QuotaManager* quotaManager = quota::QuotaManager::Get();

    auto originMetadata =
        quota::OriginMetadata{""_ns, "example.com"_ns, "http://example.com"_ns,
                              quota::PERSISTENCE_TYPE_DEFAULT};

    {
      ASSERT_NS_SUCCEEDED(quotaManager->EnsureStorageIsInitialized());

      ASSERT_NS_SUCCEEDED(quotaManager->EnsureTemporaryStorageIsInitialized());

      auto res = quotaManager->EnsureTemporaryOriginIsInitialized(
          quota::PERSISTENCE_TYPE_DEFAULT, originMetadata);
      ASSERT_TRUE(res.isOk());
    }

    const int64_t groupLimit =
        static_cast<int64_t>(quotaManager->GetGroupLimit());
    ASSERT_TRUE(mQuotaLimit * 1024LL == groupLimit);

    // We don't use the tested stream itself to check the file size as it
    // may report values which have not been written to disk.
    RefPtr<quota::FileOutputStream> check = MakeRefPtr<quota::FileOutputStream>(
        quota::PERSISTENCE_TYPE_DEFAULT, originMetadata,
        quota::Client::Type::SDB);

    RefPtr<quota::FileOutputStream> stream =
        MakeRefPtr<quota::FileOutputStream>(quota::PERSISTENCE_TYPE_DEFAULT,
                                            originMetadata,
                                            quota::Client::Type::SDB);

    {
      auto testPathRes = quotaManager->GetDirectoryForOrigin(
          quota::PERSISTENCE_TYPE_DEFAULT, originMetadata.mOrigin);

      ASSERT_TRUE(testPathRes.isOk());

      nsCOMPtr<nsIFile> testPath = testPathRes.unwrap();

      ASSERT_NS_SUCCEEDED(testPath->AppendRelativePath(u"sdb"_ns));

      ASSERT_NS_SUCCEEDED(
          testPath->AppendRelativePath(u"tTestFileOutputStream.txt"_ns));

      bool exists = true;
      ASSERT_NS_SUCCEEDED(testPath->Exists(&exists));

      if (exists) {
        ASSERT_NS_SUCCEEDED(testPath->Remove(/* recursive */ false));
      }

      ASSERT_NS_SUCCEEDED(testPath->Exists(&exists));
      ASSERT_FALSE(exists);

      ASSERT_NS_SUCCEEDED(testPath->Create(nsIFile::NORMAL_FILE_TYPE, 0666));

      ASSERT_NS_SUCCEEDED(testPath->Exists(&exists));
      ASSERT_TRUE(exists);

      nsCOMPtr<nsIFile> checkPath;
      ASSERT_NS_SUCCEEDED(testPath->Clone(getter_AddRefs(checkPath)));

      const int32_t IOFlags = -1;
      const int32_t perm = -1;
      const int32_t behaviorFlags = 0;
      ASSERT_NS_SUCCEEDED(stream->Init(testPath, IOFlags, perm, behaviorFlags));

      ASSERT_NS_SUCCEEDED(check->Init(testPath, IOFlags, perm, behaviorFlags));
    }

    // Check that we start with an empty file
    int64_t avail = 42;
    ASSERT_NS_SUCCEEDED(check->GetSize(&avail));

    ASSERT_TRUE(0 == avail);

    // Enlarge the file
    const int64_t toSize = groupLimit;
    ASSERT_NS_SUCCEEDED(stream->Seek(nsISeekableStream::NS_SEEK_SET, toSize));

    ASSERT_NS_SUCCEEDED(check->GetSize(&avail));

    ASSERT_TRUE(0 == avail);

    ASSERT_NS_SUCCEEDED(stream->SetEOF());

    ASSERT_NS_SUCCEEDED(check->GetSize(&avail));

    ASSERT_TRUE(toSize == avail);

    // Try to enlarge the file past the limit
    const int64_t overGroupLimit = groupLimit + 1;

    // Seeking is allowed
    ASSERT_NS_SUCCEEDED(
        stream->Seek(nsISeekableStream::NS_SEEK_SET, overGroupLimit));

    ASSERT_NS_SUCCEEDED(check->GetSize(&avail));

    ASSERT_TRUE(toSize == avail);

    // Setting file size to exceed quota should yield no device space error
    ASSERT_TRUE(NS_ERROR_FILE_NO_DEVICE_SPACE == stream->SetEOF());

    ASSERT_NS_SUCCEEDED(check->GetSize(&avail));

    ASSERT_TRUE(toSize == avail);

    // Shrink the file
    const int64_t toHalfSize = toSize / 2;
    ASSERT_NS_SUCCEEDED(
        stream->Seek(nsISeekableStream::NS_SEEK_SET, toHalfSize));

    ASSERT_NS_SUCCEEDED(check->GetSize(&avail));

    ASSERT_TRUE(toSize == avail);

    ASSERT_NS_SUCCEEDED(stream->SetEOF());

    ASSERT_NS_SUCCEEDED(check->GetSize(&avail));

    ASSERT_TRUE(toHalfSize == avail);

    // Shrink the file back to nothing
    ASSERT_NS_SUCCEEDED(stream->Seek(nsISeekableStream::NS_SEEK_SET, 0));

    ASSERT_NS_SUCCEEDED(check->GetSize(&avail));

    ASSERT_TRUE(toHalfSize == avail);

    ASSERT_NS_SUCCEEDED(stream->SetEOF());

    ASSERT_NS_SUCCEEDED(check->GetSize(&avail));

    ASSERT_TRUE(0 == avail);
  };

  quota::QuotaManager* quotaManager = quota::QuotaManager::Get();

  bool done = false;

  InvokeAsync(quotaManager->IOThread(), __func__,
              [ioTask = std::move(ioTask)]() {
                ioTask();
                return BoolPromise::CreateAndResolve(true, __func__);
              })
      ->Then(GetCurrentSerialEventTarget(), __func__,
             [&done](const BoolPromise::ResolveOrRejectValue& value) {
               done = true;
             });

  SpinEventLoopUntil("Promise is fulfilled"_ns, [&done]() { return done; });
}

}  // namespace mozilla::dom::quota::test
