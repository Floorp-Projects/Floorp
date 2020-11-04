/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsMemoryReporterManager.h"
#include "MemoryReportRequest.h"
#include "mozilla/ipc/FileDescriptor.h"
#include "mozilla/ipc/FileDescriptorUtils.h"

using namespace mozilla::ipc;

namespace mozilla::dom {

MemoryReportRequestHost::MemoryReportRequestHost(uint32_t aGeneration)
    : mGeneration(aGeneration), mSuccess(false) {
  MOZ_COUNT_CTOR(MemoryReportRequestHost);
  mReporterManager = nsMemoryReporterManager::GetOrCreate();
  NS_WARNING_ASSERTION(mReporterManager, "GetOrCreate failed");
}

void MemoryReportRequestHost::RecvReport(const MemoryReport& aReport) {
  // Skip reports from older generations. We need to do this here since we
  // could receive older reports from a subprocesses before it acknowledges
  // a new request, and we only track one active request per process.
  if (aReport.generation() != mGeneration) {
    return;
  }

  if (mReporterManager) {
    mReporterManager->HandleChildReport(mGeneration, aReport);
  }
}

void MemoryReportRequestHost::Finish(uint32_t aGeneration) {
  // Skip reports from older generations. See the comment in RecvReport.
  if (mGeneration != aGeneration) {
    return;
  }
  mSuccess = true;
}

MemoryReportRequestHost::~MemoryReportRequestHost() {
  MOZ_COUNT_DTOR(MemoryReportRequestHost);

  if (mReporterManager) {
    mReporterManager->EndProcessReport(mGeneration, mSuccess);
    mReporterManager = nullptr;
  }
}

NS_IMPL_ISUPPORTS(MemoryReportRequestClient, nsIRunnable)

/* static */ void MemoryReportRequestClient::Start(
    uint32_t aGeneration, bool aAnonymize, bool aMinimizeMemoryUsage,
    const Maybe<FileDescriptor>& aDMDFile, const nsACString& aProcessString,
    const ReportCallback& aReportCallback,
    const FinishCallback& aFinishCallback) {
  RefPtr<MemoryReportRequestClient> request = new MemoryReportRequestClient(
      aGeneration, aAnonymize, aDMDFile, aProcessString, aReportCallback,
      aFinishCallback);

  DebugOnly<nsresult> rv;
  if (aMinimizeMemoryUsage) {
    nsCOMPtr<nsIMemoryReporterManager> mgr =
        do_GetService("@mozilla.org/memory-reporter-manager;1");
    rv = mgr->MinimizeMemoryUsage(request);
    // mgr will eventually call actor->Run()
  } else {
    rv = request->Run();
  }

  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "actor operation failed");
}

MemoryReportRequestClient::MemoryReportRequestClient(
    uint32_t aGeneration, bool aAnonymize,
    const Maybe<FileDescriptor>& aDMDFile, const nsACString& aProcessString,
    const ReportCallback& aReportCallback,
    const FinishCallback& aFinishCallback)
    : mGeneration(aGeneration),
      mAnonymize(aAnonymize),
      mProcessString(aProcessString),
      mReportCallback(aReportCallback),
      mFinishCallback(aFinishCallback) {
  if (aDMDFile.isSome()) {
    mDMDFile = aDMDFile.value();
  }
}

MemoryReportRequestClient::~MemoryReportRequestClient() = default;

class HandleReportCallback final : public nsIHandleReportCallback {
 public:
  using ReportCallback = typename MemoryReportRequestClient::ReportCallback;

  NS_DECL_ISUPPORTS

  explicit HandleReportCallback(uint32_t aGeneration,
                                const nsACString& aProcess,
                                const ReportCallback& aReportCallback)
      : mGeneration(aGeneration),
        mProcess(aProcess),
        mReportCallback(aReportCallback) {}

  NS_IMETHOD Callback(const nsACString& aProcess, const nsACString& aPath,
                      int32_t aKind, int32_t aUnits, int64_t aAmount,
                      const nsACString& aDescription,
                      nsISupports* aUnused) override {
    MemoryReport memreport(mProcess, nsCString(aPath), aKind, aUnits, aAmount,
                           mGeneration, nsCString(aDescription));
    mReportCallback(memreport);
    return NS_OK;
  }

 private:
  ~HandleReportCallback() = default;

  uint32_t mGeneration;
  const nsCString mProcess;
  ReportCallback mReportCallback;
};

NS_IMPL_ISUPPORTS(HandleReportCallback, nsIHandleReportCallback)

class FinishReportingCallback final : public nsIFinishReportingCallback {
 public:
  using FinishCallback = typename MemoryReportRequestClient::FinishCallback;

  NS_DECL_ISUPPORTS

  explicit FinishReportingCallback(uint32_t aGeneration,
                                   const FinishCallback& aFinishCallback)
      : mGeneration(aGeneration), mFinishCallback(aFinishCallback) {}

  NS_IMETHOD Callback(nsISupports* aUnused) override {
    mFinishCallback(mGeneration);
    return NS_OK;
  }

 private:
  ~FinishReportingCallback() = default;

  uint32_t mGeneration;
  FinishCallback mFinishCallback;
};

NS_IMPL_ISUPPORTS(FinishReportingCallback, nsIFinishReportingCallback)

NS_IMETHODIMP MemoryReportRequestClient::Run() {
  nsCOMPtr<nsIMemoryReporterManager> mgr =
      do_GetService("@mozilla.org/memory-reporter-manager;1");

  // Run the reporters.  The callback will turn each measurement into a
  // MemoryReport.
  RefPtr<HandleReportCallback> handleReport =
      new HandleReportCallback(mGeneration, mProcessString, mReportCallback);
  RefPtr<FinishReportingCallback> finishReporting =
      new FinishReportingCallback(mGeneration, mFinishCallback);

  nsresult rv = mgr->GetReportsForThisProcessExtended(
      handleReport, nullptr, mAnonymize, FileDescriptorToFILE(mDMDFile, "wb"),
      finishReporting, nullptr);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "GetReportsForThisProcessExtended failed");
  return rv;
}

}  // namespace mozilla::dom
