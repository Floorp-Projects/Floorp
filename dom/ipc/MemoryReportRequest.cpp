/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MemoryReportRequest.h"

namespace mozilla {
namespace dom {

MemoryReportRequestParent::MemoryReportRequestParent(uint32_t aGeneration)
  : mGeneration(aGeneration)
{
  MOZ_COUNT_CTOR(MemoryReportRequestParent);
  mReporterManager = nsMemoryReporterManager::GetOrCreate();
  NS_WARNING_ASSERTION(mReporterManager, "GetOrCreate failed");
}

mozilla::ipc::IPCResult
MemoryReportRequestParent::RecvReport(const MemoryReport& aReport)
{
  if (mReporterManager) {
    mReporterManager->HandleChildReport(mGeneration, aReport);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
MemoryReportRequestParent::Recv__delete__()
{
  // Notifying the reporter manager is done in ActorDestroy, because
  // it needs to happen even if the child process exits mid-report.
  // (The reporter manager will time out eventually, but let's avoid
  // that if possible.)
  return IPC_OK();
}

void
MemoryReportRequestParent::ActorDestroy(ActorDestroyReason aWhy)
{
  if (mReporterManager) {
    mReporterManager->EndProcessReport(mGeneration, aWhy == Deletion);
    mReporterManager = nullptr;
  }
}

MemoryReportRequestParent::~MemoryReportRequestParent()
{
  MOZ_ASSERT(!mReporterManager);
  MOZ_COUNT_DTOR(MemoryReportRequestParent);
}

NS_IMPL_ISUPPORTS(MemoryReportRequestChild, nsIRunnable)

MemoryReportRequestChild::MemoryReportRequestChild(
  bool aAnonymize, const MaybeFileDesc& aDMDFile,
  const nsACString& aProcessString)
 : mAnonymize(aAnonymize),
   mProcessString(aProcessString)
{
  if (aDMDFile.type() == MaybeFileDesc::TFileDescriptor) {
    mDMDFile = aDMDFile.get_FileDescriptor();
  }
}

MemoryReportRequestChild::~MemoryReportRequestChild()
{
}

class HandleReportCallback final : public nsIHandleReportCallback
{
public:
  NS_DECL_ISUPPORTS

  explicit HandleReportCallback(MemoryReportRequestChild* aActor,
                                const nsACString& aProcess)
  : mActor(aActor)
  , mProcess(aProcess)
  { }

  NS_IMETHOD Callback(const nsACString& aProcess, const nsACString &aPath,
                      int32_t aKind, int32_t aUnits, int64_t aAmount,
                      const nsACString& aDescription,
                      nsISupports* aUnused) override
  {
    MemoryReport memreport(mProcess, nsCString(aPath), aKind, aUnits,
                           aAmount, nsCString(aDescription));
    mActor->SendReport(memreport);
    return NS_OK;
  }
private:
  ~HandleReportCallback() = default;

  RefPtr<MemoryReportRequestChild> mActor;
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

  explicit FinishReportingCallback(MemoryReportRequestChild* aActor)
  : mActor(aActor)
  {
  }

  NS_IMETHOD Callback(nsISupports* aUnused) override
  {
    bool sent = PMemoryReportRequestChild::Send__delete__(mActor);
    return sent ? NS_OK : NS_ERROR_FAILURE;
  }

private:
  ~FinishReportingCallback() = default;

  RefPtr<MemoryReportRequestChild> mActor;
};

NS_IMPL_ISUPPORTS(
  FinishReportingCallback
, nsIFinishReportingCallback
)

NS_IMETHODIMP MemoryReportRequestChild::Run()
{
  nsCOMPtr<nsIMemoryReporterManager> mgr =
    do_GetService("@mozilla.org/memory-reporter-manager;1");

  // Run the reporters.  The callback will turn each measurement into a
  // MemoryReport.
  RefPtr<HandleReportCallback> handleReport =
    new HandleReportCallback(this, mProcessString);
  RefPtr<FinishReportingCallback> finishReporting =
    new FinishReportingCallback(this);

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
