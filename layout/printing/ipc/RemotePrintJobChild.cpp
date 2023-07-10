/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RemotePrintJobChild.h"

#include "mozilla/SpinEventLoopUntil.h"
#include "mozilla/Unused.h"
#include "nsPagePrintTimer.h"
#include "nsPrintJob.h"
#include "private/pprio.h"

namespace mozilla {
namespace layout {

NS_IMPL_ISUPPORTS(RemotePrintJobChild, nsIWebProgressListener)

RemotePrintJobChild::RemotePrintJobChild() = default;

nsresult RemotePrintJobChild::InitializePrint(const nsString& aDocumentTitle,
                                              const int32_t& aStartPage,
                                              const int32_t& aEndPage) {
  // Print initialization can sometimes display a dialog in the parent, so we
  // need to spin a nested event loop until initialization completes.
  Unused << SendInitializePrint(aDocumentTitle, aStartPage, aEndPage);
  mozilla::SpinEventLoopUntil("RemotePrintJobChild::InitializePrint"_ns,
                              [&]() { return mPrintInitialized; });

  return mInitializationResult;
}

mozilla::ipc::IPCResult RemotePrintJobChild::RecvPrintInitializationResult(
    const nsresult& aRv, const mozilla::ipc::FileDescriptor& aFd) {
  mPrintInitialized = true;
  mInitializationResult = aRv;
  if (NS_SUCCEEDED(aRv)) {
    SetNextPageFD(aFd);
  }
  return IPC_OK();
}

PRFileDesc* RemotePrintJobChild::GetNextPageFD() {
  MOZ_ASSERT(!mDestroyed);
  MOZ_ASSERT(mNextPageFD);
  PRFileDesc* fd = mNextPageFD;
  mNextPageFD = nullptr;
  return fd;
}

void RemotePrintJobChild::SetNextPageFD(
    const mozilla::ipc::FileDescriptor& aFd) {
  MOZ_ASSERT(!mDestroyed);
  auto handle = aFd.ClonePlatformHandle();
  mNextPageFD = PR_ImportFile(PROsfd(handle.release()));
}

void RemotePrintJobChild::ProcessPage(const IntSize& aSizeInPoints,
                                      nsTArray<uint64_t>&& aDeps) {
  MOZ_ASSERT(mPagePrintTimer);

  mPagePrintTimer->WaitForRemotePrint();
  if (!mDestroyed) {
    Unused << SendProcessPage(aSizeInPoints.width, aSizeInPoints.height,
                              std::move(aDeps));
  }
}

mozilla::ipc::IPCResult RemotePrintJobChild::RecvPageProcessed(
    const mozilla::ipc::FileDescriptor& aFd) {
  MOZ_ASSERT(mPagePrintTimer);
  SetNextPageFD(aFd);

  mPagePrintTimer->RemotePrintFinished();
  return IPC_OK();
}

mozilla::ipc::IPCResult RemotePrintJobChild::RecvAbortPrint(
    const nsresult& aRv) {
  MOZ_ASSERT(mPrintJob);

  mPrintJob->CleanupOnFailure(aRv, true);
  return IPC_OK();
}

void RemotePrintJobChild::SetPagePrintTimer(nsPagePrintTimer* aPagePrintTimer) {
  MOZ_ASSERT(!mDestroyed);
  MOZ_ASSERT(aPagePrintTimer);

  mPagePrintTimer = aPagePrintTimer;
}

void RemotePrintJobChild::SetPrintJob(nsPrintJob* aPrintJob) {
  MOZ_ASSERT(!mDestroyed);
  MOZ_ASSERT(aPrintJob);

  mPrintJob = aPrintJob;
}

// nsIWebProgressListener

NS_IMETHODIMP
RemotePrintJobChild::OnStateChange(nsIWebProgress* aProgress,
                                   nsIRequest* aRequest, uint32_t aStateFlags,
                                   nsresult aStatus) {
  // `RemotePrintJobParent` emits its own state change events based on its
  // own progress & the actor lifecycle, so any forwarded event here would get
  // ignored.
  return NS_OK;
}

NS_IMETHODIMP
RemotePrintJobChild::OnProgressChange(nsIWebProgress* aProgress,
                                      nsIRequest* aRequest,
                                      int32_t aCurSelfProgress,
                                      int32_t aMaxSelfProgress,
                                      int32_t aCurTotalProgress,
                                      int32_t aMaxTotalProgress) {
  if (!mDestroyed) {
    Unused << SendProgressChange(aCurSelfProgress, aMaxSelfProgress,
                                 aCurTotalProgress, aMaxTotalProgress);
  }

  return NS_OK;
}

NS_IMETHODIMP
RemotePrintJobChild::OnLocationChange(nsIWebProgress* aProgress,
                                      nsIRequest* aRequest, nsIURI* aURI,
                                      uint32_t aFlags) {
  return NS_OK;
}

NS_IMETHODIMP
RemotePrintJobChild::OnStatusChange(nsIWebProgress* aProgress,
                                    nsIRequest* aRequest, nsresult aStatus,
                                    const char16_t* aMessage) {
  if (NS_SUCCEEDED(mInitializationResult) && !mDestroyed) {
    Unused << SendStatusChange(aStatus);
  }

  return NS_OK;
}

NS_IMETHODIMP
RemotePrintJobChild::OnSecurityChange(nsIWebProgress* aProgress,
                                      nsIRequest* aRequest, uint32_t aState) {
  return NS_OK;
}

NS_IMETHODIMP
RemotePrintJobChild::OnContentBlockingEvent(nsIWebProgress* aProgress,
                                            nsIRequest* aRequest,
                                            uint32_t aEvent) {
  return NS_OK;
}

// End of nsIWebProgressListener

RemotePrintJobChild::~RemotePrintJobChild() = default;

void RemotePrintJobChild::ActorDestroy(ActorDestroyReason aWhy) {
  mPagePrintTimer = nullptr;
  mPrintJob = nullptr;

  mDestroyed = true;
}

}  // namespace layout
}  // namespace mozilla
