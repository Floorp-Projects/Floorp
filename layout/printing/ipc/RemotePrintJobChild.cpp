/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RemotePrintJobChild.h"

#include "mozilla/Unused.h"
#include "nsPagePrintTimer.h"
#include "nsPrintEngine.h"
#include "private/pprio.h"

namespace mozilla {
namespace layout {

NS_IMPL_ISUPPORTS(RemotePrintJobChild,
                  nsIWebProgressListener)

RemotePrintJobChild::RemotePrintJobChild()
{
}

nsresult
RemotePrintJobChild::InitializePrint(const nsString& aDocumentTitle,
                                     const nsString& aPrintToFile,
                                     const int32_t& aStartPage,
                                     const int32_t& aEndPage)
{
  // Print initialization can sometimes display a dialog in the parent, so we
  // need to spin a nested event loop until initialization completes.
  Unused << SendInitializePrint(aDocumentTitle, aPrintToFile, aStartPage,
                                aEndPage);
  mozilla::SpinEventLoopUntil([&]() { return mPrintInitialized; });

  return mInitializationResult;
}

mozilla::ipc::IPCResult
RemotePrintJobChild::RecvPrintInitializationResult(
  const nsresult& aRv,
  const mozilla::ipc::FileDescriptor& aFd)
{
  mPrintInitialized = true;
  mInitializationResult = aRv;
  if (NS_SUCCEEDED(aRv)) {
    SetNextPageFD(aFd);
  }
  return IPC_OK();
}

PRFileDesc*
RemotePrintJobChild::GetNextPageFD()
{
  MOZ_ASSERT(mNextPageFD);
  PRFileDesc* fd = mNextPageFD;
  mNextPageFD = nullptr;
  return fd;
}

void
RemotePrintJobChild::SetNextPageFD(const mozilla::ipc::FileDescriptor& aFd)
{
  auto handle = aFd.ClonePlatformHandle();
  mNextPageFD = PR_ImportFile(PROsfd(handle.release()));
}

void
RemotePrintJobChild::ProcessPage()
{
  MOZ_ASSERT(mPagePrintTimer);

  mPagePrintTimer->WaitForRemotePrint();
  if (!mDestroyed) {
    Unused << SendProcessPage();
  }
}

mozilla::ipc::IPCResult
RemotePrintJobChild::RecvPageProcessed(const mozilla::ipc::FileDescriptor& aFd)
{
  MOZ_ASSERT(mPagePrintTimer);
  SetNextPageFD(aFd);

  mPagePrintTimer->RemotePrintFinished();
  return IPC_OK();
}

mozilla::ipc::IPCResult
RemotePrintJobChild::RecvAbortPrint(const nsresult& aRv)
{
  MOZ_ASSERT(mPrintEngine);

  mPrintEngine->CleanupOnFailure(aRv, true);
  return IPC_OK();
}

void
RemotePrintJobChild::SetPagePrintTimer(nsPagePrintTimer* aPagePrintTimer)
{
  MOZ_ASSERT(aPagePrintTimer);

  mPagePrintTimer = aPagePrintTimer;
}

void
RemotePrintJobChild::SetPrintEngine(nsPrintEngine* aPrintEngine)
{
  MOZ_ASSERT(aPrintEngine);

  mPrintEngine = aPrintEngine;
}

// nsIWebProgressListener

NS_IMETHODIMP
RemotePrintJobChild::OnStateChange(nsIWebProgress* aProgress,
                                   nsIRequest* aRequest, uint32_t aStateFlags,
                                   nsresult aStatus)
{
  if (!mDestroyed) {
    Unused << SendStateChange(aStateFlags, aStatus);
  }

  return NS_OK;
}

NS_IMETHODIMP
RemotePrintJobChild::OnProgressChange(nsIWebProgress * aProgress,
                                      nsIRequest * aRequest,
                                      int32_t aCurSelfProgress,
                                      int32_t aMaxSelfProgress,
                                      int32_t aCurTotalProgress,
                                      int32_t aMaxTotalProgress)
{
  if (!mDestroyed) {
    Unused << SendProgressChange(aCurSelfProgress, aMaxSelfProgress,
                                 aCurTotalProgress, aMaxTotalProgress);
  }

  return NS_OK;
}

NS_IMETHODIMP
RemotePrintJobChild::OnLocationChange(nsIWebProgress* aProgress,
                                      nsIRequest* aRequest, nsIURI* aURI,
                                      uint32_t aFlags)
{
  return NS_OK;
}

NS_IMETHODIMP
RemotePrintJobChild::OnStatusChange(nsIWebProgress* aProgress,
                                    nsIRequest* aRequest, nsresult aStatus,
                                    const char16_t* aMessage)
{
  if (!mDestroyed) {
    Unused << SendStatusChange(aStatus);
  }

  return NS_OK;
}

NS_IMETHODIMP
RemotePrintJobChild::OnSecurityChange(nsIWebProgress* aProgress,
                                      nsIRequest* aRequest, uint32_t aState)
{
  return NS_OK;
}

// End of nsIWebProgressListener

RemotePrintJobChild::~RemotePrintJobChild()
{
}

void
RemotePrintJobChild::ActorDestroy(ActorDestroyReason aWhy)
{
  mPagePrintTimer = nullptr;
  mPrintEngine = nullptr;

  mDestroyed = true;
}

} // namespace layout
} // namespace mozilla
