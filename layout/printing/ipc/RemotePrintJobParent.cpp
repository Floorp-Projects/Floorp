/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RemotePrintJobParent.h"

#include <fstream>

#include "gfxContext.h"
#include "mozilla/Attributes.h"
#include "mozilla/ProfilerMarkers.h"
#include "mozilla/Unused.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsComponentManagerUtils.h"
#include "nsDirectoryServiceUtils.h"
#include "nsDeviceContext.h"
#include "nsIDeviceContextSpec.h"
#include "nsIPrintSettings.h"
#include "nsIWebProgressListener.h"
#include "PrintTranslator.h"
#include "private/pprio.h"
#include "nsAnonymousTemporaryFile.h"

namespace mozilla::layout {

RemotePrintJobParent::RemotePrintJobParent(nsIPrintSettings* aPrintSettings)
    : mPrintSettings(aPrintSettings),
      mIsDoingPrinting(false),
      mStatus(NS_ERROR_UNEXPECTED) {
  MOZ_COUNT_CTOR(RemotePrintJobParent);
}

mozilla::ipc::IPCResult RemotePrintJobParent::RecvInitializePrint(
    const nsAString& aDocumentTitle, const int32_t& aStartPage,
    const int32_t& aEndPage) {
  PROFILER_MARKER_TEXT("RemotePrintJobParent", LAYOUT_Printing, {},
                       "RemotePrintJobParent::RecvInitializePrint"_ns);

  nsresult rv = InitializePrintDevice(aDocumentTitle, aStartPage, aEndPage);
  if (NS_FAILED(rv)) {
    Unused << SendPrintInitializationResult(rv, FileDescriptor());
    mStatus = rv;
    Unused << Send__delete__(this);
    return IPC_OK();
  }

  mPrintTranslator.reset(new PrintTranslator(mPrintDeviceContext));
  FileDescriptor fd;
  rv = PrepareNextPageFD(&fd);
  if (NS_FAILED(rv)) {
    Unused << SendPrintInitializationResult(rv, FileDescriptor());
    mStatus = rv;
    Unused << Send__delete__(this);
    return IPC_OK();
  }

  Unused << SendPrintInitializationResult(NS_OK, fd);
  return IPC_OK();
}

nsresult RemotePrintJobParent::InitializePrintDevice(
    const nsAString& aDocumentTitle, const int32_t& aStartPage,
    const int32_t& aEndPage) {
  AUTO_PROFILER_MARKER_TEXT("RemotePrintJobParent", LAYOUT_Printing, {},
                            "RemotePrintJobParent::InitializePrintDevice"_ns);

  nsresult rv;
  nsCOMPtr<nsIDeviceContextSpec> deviceContextSpec =
      do_CreateInstance("@mozilla.org/gfx/devicecontextspec;1", &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = deviceContextSpec->Init(mPrintSettings, false);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  mPrintDeviceContext = new nsDeviceContext();
  rv = mPrintDeviceContext->InitForPrinting(deviceContextSpec);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsAutoString fileName;
  mPrintSettings->GetToFileName(fileName);

  rv = mPrintDeviceContext->BeginDocument(aDocumentTitle, fileName, aStartPage,
                                          aEndPage);
  if (NS_FAILED(rv)) {
    NS_WARNING_ASSERTION(rv == NS_ERROR_ABORT,
                         "Failed to initialize print device");
    return rv;
  }

  mIsDoingPrinting = true;

  return NS_OK;
}

nsresult RemotePrintJobParent::PrepareNextPageFD(FileDescriptor* aFd) {
  AUTO_PROFILER_MARKER_TEXT("RemotePrintJobParent", LAYOUT_Printing, {},
                            "RemotePrintJobParent::PrepareNextPageFD"_ns);

  PRFileDesc* prFd = nullptr;
  nsresult rv = NS_OpenAnonymousTemporaryFile(&prFd);
  if (NS_FAILED(rv)) {
    return rv;
  }
  *aFd = FileDescriptor(
      FileDescriptor::PlatformHandleType(PR_FileDesc2NativeHandle(prFd)));
  mCurrentPageStream.OpenFD(prFd);
  return NS_OK;
}

mozilla::ipc::IPCResult RemotePrintJobParent::RecvProcessPage(
    const int32_t& aWidthInPoints, const int32_t& aHeightInPoints,
    nsTArray<uint64_t>&& aDeps) {
  PROFILER_MARKER_TEXT("RemotePrintJobParent", LAYOUT_Printing, {},
                       "RemotePrintJobParent::RecvProcessPage"_ns);
  if (!mCurrentPageStream.IsOpen()) {
    Unused << SendAbortPrint(NS_ERROR_FAILURE);
    return IPC_OK();
  }
  mCurrentPageStream.Seek(0, PR_SEEK_SET);

  gfx::IntSize pageSizeInPoints(aWidthInPoints, aHeightInPoints);

  if (aDeps.IsEmpty()) {
    FinishProcessingPage(pageSizeInPoints);
    return IPC_OK();
  }

  nsTHashSet<uint64_t> deps;
  for (auto i : aDeps) {
    deps.Insert(i);
  }

  gfx::CrossProcessPaint::Start(std::move(deps))
      ->Then(
          GetCurrentSerialEventTarget(), __func__,
          [self = RefPtr{this}, pageSizeInPoints](
              gfx::CrossProcessPaint::ResolvedFragmentMap&& aFragments) {
            self->FinishProcessingPage(pageSizeInPoints, &aFragments);
          },
          [self = RefPtr{this}, pageSizeInPoints](const nsresult& aRv) {
            self->FinishProcessingPage(pageSizeInPoints);
          });

  return IPC_OK();
}

void RemotePrintJobParent::FinishProcessingPage(
    const gfx::IntSize& aSizeInPoints,
    gfx::CrossProcessPaint::ResolvedFragmentMap* aFragments) {
  nsresult rv = PrintPage(aSizeInPoints, mCurrentPageStream, aFragments);

  mCurrentPageStream.Close();

  PageDone(rv);
}

nsresult RemotePrintJobParent::PrintPage(
    const gfx::IntSize& aSizeInPoints, PRFileDescStream& aRecording,
    gfx::CrossProcessPaint::ResolvedFragmentMap* aFragments) {
  MOZ_ASSERT(mPrintDeviceContext);
  AUTO_PROFILER_MARKER_TEXT("RemotePrintJobParent", LAYOUT_Printing, {},
                            "RemotePrintJobParent::PrintPage"_ns);

  nsresult rv = mPrintDeviceContext->BeginPage(aSizeInPoints);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  if (aFragments) {
    mPrintTranslator->SetDependentSurfaces(aFragments);
  }
  if (!mPrintTranslator->TranslateRecording(aRecording)) {
    mPrintTranslator->SetDependentSurfaces(nullptr);
    return NS_ERROR_FAILURE;
  }
  mPrintTranslator->SetDependentSurfaces(nullptr);

  rv = mPrintDeviceContext->EndPage();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

void RemotePrintJobParent::PageDone(nsresult aResult) {
  MOZ_ASSERT(mIsDoingPrinting);

  if (NS_FAILED(aResult)) {
    Unused << SendAbortPrint(aResult);
  } else {
    FileDescriptor fd;
    aResult = PrepareNextPageFD(&fd);
    if (NS_FAILED(aResult)) {
      Unused << SendAbortPrint(aResult);
    }

    Unused << SendPageProcessed(fd);
  }
}

static void NotifyStatusChange(
    const nsCOMArray<nsIWebProgressListener>& aListeners, nsresult aStatus) {
  uint32_t numberOfListeners = aListeners.Length();
  for (uint32_t i = 0; i < numberOfListeners; ++i) {
    nsIWebProgressListener* listener = aListeners[static_cast<int32_t>(i)];
    listener->OnStatusChange(nullptr, nullptr, aStatus, nullptr);
  }
}

static void NotifyStateChange(
    const nsCOMArray<nsIWebProgressListener>& aListeners, long aStateFlags,
    nsresult aStatus) {
  uint32_t numberOfListeners = aListeners.Length();
  for (uint32_t i = 0; i < numberOfListeners; ++i) {
    nsIWebProgressListener* listener = aListeners[static_cast<int32_t>(i)];
    listener->OnStateChange(nullptr, nullptr, aStateFlags, aStatus);
  }
}

static void Cleanup(const nsCOMArray<nsIWebProgressListener>& aListeners,
                    RefPtr<nsDeviceContext>& aAbortContext,
                    const bool aPrintingInterrupted, const nsresult aResult) {
  auto result = aResult;
  if (MOZ_UNLIKELY(aPrintingInterrupted && NS_SUCCEEDED(result))) {
    result = NS_ERROR_UNEXPECTED;
  }
  if (NS_FAILED(result)) {
    NotifyStatusChange(aListeners, result);
  }
  if (aPrintingInterrupted && aAbortContext) {
    // Abort any started print.
    Unused << aAbortContext->AbortDocument();
  }
  // However the print went, let the listeners know that we're done.
  NotifyStateChange(aListeners,
                    nsIWebProgressListener::STATE_STOP |
                        nsIWebProgressListener::STATE_IS_DOCUMENT,
                    result);
}

mozilla::ipc::IPCResult RemotePrintJobParent::RecvFinalizePrint() {
  PROFILER_MARKER_TEXT("RemotePrintJobParent", LAYOUT_Printing, {},
                       "RemotePrintJobParent::RecvFinalizePrint"_ns);

  // EndDocument is sometimes called in the child even when BeginDocument has
  // not been called. See bug 1223332.
  if (mPrintDeviceContext) {
    mPrintDeviceContext->EndDocument()->Then(
        GetMainThreadSerialEventTarget(), __func__,
        [listeners = std::move(mPrintProgressListeners)](
            const mozilla::gfx::PrintEndDocumentPromise::ResolveOrRejectValue&
                aResult) {
          // Printing isn't interrupted, so we don't need the device context
          // here.
          RefPtr<nsDeviceContext> empty;
          if (aResult.IsResolve()) {
            Cleanup(listeners, empty, /* aPrintingInterrupted = */ false,
                    NS_OK);
          } else {
            Cleanup(listeners, empty, /* aPrintingInterrupted = */ false,
                    aResult.RejectValue());
          }
        });
    mStatus = NS_OK;
  }

  mIsDoingPrinting = false;

  Unused << Send__delete__(this);
  return IPC_OK();
}

mozilla::ipc::IPCResult RemotePrintJobParent::RecvAbortPrint(
    const nsresult& aRv) {
  PROFILER_MARKER_TEXT("RemotePrintJobParent", LAYOUT_Printing, {},
                       "RemotePrintJobParent::RecvAbortPrint"_ns);

  // Leave the cleanup to `ActorDestroy()`.
  Unused << Send__delete__(this);
  return IPC_OK();
}

mozilla::ipc::IPCResult RemotePrintJobParent::RecvProgressChange(
    const long& aCurSelfProgress, const long& aMaxSelfProgress,
    const long& aCurTotalProgress, const long& aMaxTotalProgress) {
  PROFILER_MARKER_TEXT("RemotePrintJobParent", LAYOUT_Printing, {},
                       "RemotePrintJobParent::RecvProgressChange"_ns);
  // Our progress follows that of `RemotePrintJobChild` closely enough - forward
  // it instead of keeping more state variables here.
  for (auto* listener : mPrintProgressListeners) {
    listener->OnProgressChange(nullptr, nullptr, aCurSelfProgress,
                               aMaxSelfProgress, aCurTotalProgress,
                               aMaxTotalProgress);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult RemotePrintJobParent::RecvStatusChange(
    const nsresult& aStatus) {
  PROFILER_MARKER_TEXT("RemotePrintJobParent", LAYOUT_Printing, {},
                       "RemotePrintJobParent::RecvProgressChange"_ns);
  if (NS_FAILED(aStatus)) {
    // Remember the failure status for cleanup to forward to listeners.
    mStatus = aStatus;
  }

  return IPC_OK();
}

void RemotePrintJobParent::RegisterListener(nsIWebProgressListener* aListener) {
  MOZ_ASSERT(aListener);

  // Our listener is a Promise created by CanonicalBrowsingContext::Print
  mPrintProgressListeners.AppendElement(aListener);
}

already_AddRefed<nsIPrintSettings> RemotePrintJobParent::GetPrintSettings() {
  nsCOMPtr<nsIPrintSettings> printSettings = mPrintSettings;
  return printSettings.forget();
}

RemotePrintJobParent::~RemotePrintJobParent() {
  MOZ_COUNT_DTOR(RemotePrintJobParent);
}

void RemotePrintJobParent::ActorDestroy(ActorDestroyReason aWhy) {
  if (MOZ_UNLIKELY(mIsDoingPrinting && NS_SUCCEEDED(mStatus))) {
    mStatus = NS_ERROR_UNEXPECTED;
  }
  Cleanup(mPrintProgressListeners, mPrintDeviceContext, mIsDoingPrinting,
          mStatus);
  // At any rate, this actor is done and cleaned up.
  mIsDoingPrinting = false;
}

}  // namespace mozilla::layout
