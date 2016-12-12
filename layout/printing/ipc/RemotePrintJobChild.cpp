/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RemotePrintJobChild.h"

#include "mozilla/Unused.h"
#include "nsPagePrintTimer.h"
#include "nsPrintEngine.h"

namespace mozilla {
namespace layout {

NS_IMPL_ISUPPORTS(RemotePrintJobChild,
                  nsIWebProgressListener)

RemotePrintJobChild::RemotePrintJobChild()
{
  MOZ_COUNT_CTOR(RemotePrintJobChild);
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
  while (!mPrintInitialized) {
    Unused << NS_ProcessNextEvent();
  }

  return mInitializationResult;
}

mozilla::ipc::IPCResult
RemotePrintJobChild::RecvPrintInitializationResult(const nsresult& aRv)
{
  mPrintInitialized = true;
  mInitializationResult = aRv;
  return IPC_OK();
}

void
RemotePrintJobChild::ProcessPage(const nsCString& aPageFileName)
{
  MOZ_ASSERT(mPagePrintTimer);

  mPagePrintTimer->WaitForRemotePrint();
  Unused << SendProcessPage(aPageFileName);
}

mozilla::ipc::IPCResult
RemotePrintJobChild::RecvPageProcessed()
{
  MOZ_ASSERT(mPagePrintTimer);

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
  Unused << SendStateChange(aStateFlags, aStatus);
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
  Unused << SendProgressChange(aCurSelfProgress, aMaxSelfProgress,
                               aCurTotalProgress, aMaxTotalProgress);
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
  Unused << SendStatusChange(aStatus);
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
  MOZ_COUNT_DTOR(RemotePrintJobChild);
}

void
RemotePrintJobChild::ActorDestroy(ActorDestroyReason aWhy)
{
  mPagePrintTimer = nullptr;
  mPrintEngine = nullptr;
}

} // namespace layout
} // namespace mozilla
