/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MemoryReportRequest.h"
#include "mozilla/Unused.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/gfx/GPUParent.h"

namespace mozilla {
namespace dom {

MemoryReportRequestHost::MemoryReportRequestHost(uint32_t aGeneration)
  : mGeneration(aGeneration),
    mSuccess(false)
{
  MOZ_COUNT_CTOR(MemoryReportRequestHost);
  mReporterManager = nsMemoryReporterManager::GetOrCreate();
  NS_WARNING_ASSERTION(mReporterManager, "GetOrCreate failed");
}

void
MemoryReportRequestHost::RecvReport(const MemoryReport& aReport)
{
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

void
MemoryReportRequestHost::Finish(uint32_t aGeneration)
{
  // Skip reports from older generations. See the comment in RecvReport.
  if (mGeneration != aGeneration) {
    return;
  }
  mSuccess = true;
}

MemoryReportRequestHost::~MemoryReportRequestHost()
{
  MOZ_COUNT_DTOR(MemoryReportRequestHost);

  if (mReporterManager) {
    mReporterManager->EndProcessReport(mGeneration, mSuccess);
    mReporterManager = nullptr;
  }
}

NS_IMPL_ISUPPORTS(MemoryReportRequestClient, nsIRunnable)

/* static */ void
MemoryReportRequestClient::Start(uint32_t aGeneration,
                                 bool aAnonymize,
                                 bool aMinimizeMemoryUsage,
                                 const MaybeFileDesc& aDMDFile,
                                 const nsACString& aProcessString)
{
  RefPtr<MemoryReportRequestClient> request = new MemoryReportRequestClient(
    aGeneration,
    aAnonymize,
    aDMDFile,
    aProcessString);

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

MemoryReportRequestClient::MemoryReportRequestClient(uint32_t aGeneration,
                                                     bool aAnonymize,
                                                     const MaybeFileDesc& aDMDFile,
                                                     const nsACString& aProcessString)
 : mGeneration(aGeneration),
   mAnonymize(aAnonymize),
   mProcessString(aProcessString)
{
  if (aDMDFile.type() == MaybeFileDesc::TFileDescriptor) {
    mDMDFile = aDMDFile.get_FileDescriptor();
  }
}

MemoryReportRequestClient::~MemoryReportRequestClient()
{
}

class HandleReportCallback final : public nsIHandleReportCallback
{
public:
  NS_DECL_ISUPPORTS

  explicit HandleReportCallback(uint32_t aGeneration,
                                const nsACString& aProcess)
  : mGeneration(aGeneration)
  , mProcess(aProcess)
  { }

  NS_IMETHOD Callback(const nsACString& aProcess, const nsACString &aPath,
                      int32_t aKind, int32_t aUnits, int64_t aAmount,
                      const nsACString& aDescription,
                      nsISupports* aUnused) override
  {
    MemoryReport memreport(mProcess, nsCString(aPath), aKind, aUnits,
                           aAmount, mGeneration, nsCString(aDescription));
    switch (XRE_GetProcessType()) {
      case GeckoProcessType_Content:
        ContentChild::GetSingleton()->SendAddMemoryReport(memreport);
        break;
      case GeckoProcessType_GPU:
        Unused << gfx::GPUParent::GetSingleton()->SendAddMemoryReport(memreport);
        break;
      default:
        MOZ_ASSERT_UNREACHABLE("Unhandled process type");
    }
    return NS_OK;
  }
private:
  ~HandleReportCallback() = default;

  uint32_t mGeneration;
  const nsCString mProcess;
};

NS_IMPL_ISUPPORTS(
  HandleReportCallback
, nsIHandleReportCallback
)

class FinishReportingCallback final : public nsIFinishReportingCallback
{
public:
  NS_DECL_ISUPPORTS

  explicit FinishReportingCallback(uint32_t aGeneration)
  : mGeneration(aGeneration)
  {
  }

  NS_IMETHOD Callback(nsISupports* aUnused) override
  {
    bool sent = false;
    switch (XRE_GetProcessType()) {
      case GeckoProcessType_Content:
        sent = ContentChild::GetSingleton()->SendFinishMemoryReport(mGeneration);
        break;
      case GeckoProcessType_GPU:
        sent = gfx::GPUParent::GetSingleton()->SendFinishMemoryReport(mGeneration);
        break;
      default:
        MOZ_ASSERT_UNREACHABLE("Unhandled process type");
    }
    return sent ? NS_OK : NS_ERROR_FAILURE;
  }

private:
  ~FinishReportingCallback() = default;

  uint32_t mGeneration;
};

NS_IMPL_ISUPPORTS(
  FinishReportingCallback
, nsIFinishReportingCallback
)

NS_IMETHODIMP MemoryReportRequestClient::Run()
{
  nsCOMPtr<nsIMemoryReporterManager> mgr =
    do_GetService("@mozilla.org/memory-reporter-manager;1");

  // Run the reporters.  The callback will turn each measurement into a
  // MemoryReport.
  RefPtr<HandleReportCallback> handleReport =
    new HandleReportCallback(mGeneration, mProcessString);
  RefPtr<FinishReportingCallback> finishReporting =
    new FinishReportingCallback(mGeneration);

  nsresult rv =
    mgr->GetReportsForThisProcessExtended(handleReport, nullptr, mAnonymize,
                                          FileDescriptorToFILE(mDMDFile, "wb"),
                                          finishReporting, nullptr);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "GetReportsForThisProcessExtended failed");
  return rv;
}

} // namespace dom
} // namespace mozilla
