/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RemotePrintJobParent.h"

#include <fstream>

#include "gfxContext.h"
#include "mozilla/Attributes.h"
#include "mozilla/Unused.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsComponentManagerUtils.h"
#include "nsDirectoryServiceUtils.h"
#include "nsDeviceContext.h"
#include "nsIDeviceContextSpec.h"
#include "nsIPrintSettings.h"
#include "nsIWebProgressListener.h"
#include "PrintTranslator.h"

namespace mozilla {
namespace layout {

RemotePrintJobParent::RemotePrintJobParent(nsIPrintSettings* aPrintSettings)
  : mPrintSettings(aPrintSettings)
{
  MOZ_COUNT_CTOR(RemotePrintJobParent);
}

mozilla::ipc::IPCResult
RemotePrintJobParent::RecvInitializePrint(const nsString& aDocumentTitle,
                                          const nsString& aPrintToFile,
                                          const int32_t& aStartPage,
                                          const int32_t& aEndPage)
{
  nsresult rv = InitializePrintDevice(aDocumentTitle, aPrintToFile, aStartPage,
                                      aEndPage);
  if (NS_FAILED(rv)) {
    Unused << SendPrintInitializationResult(rv);
    Unused << Send__delete__(this);
    return IPC_OK();
  }

  mPrintTranslator.reset(new PrintTranslator(mPrintDeviceContext));
  Unused << SendPrintInitializationResult(NS_OK);

  return IPC_OK();
}

nsresult
RemotePrintJobParent::InitializePrintDevice(const nsString& aDocumentTitle,
                                            const nsString& aPrintToFile,
                                            const int32_t& aStartPage,
                                            const int32_t& aEndPage)
{
  nsresult rv;
  nsCOMPtr<nsIDeviceContextSpec> deviceContextSpec =
  do_CreateInstance("@mozilla.org/gfx/devicecontextspec;1", &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = deviceContextSpec->Init(nullptr, mPrintSettings, false);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  mPrintDeviceContext = new nsDeviceContext();
  rv = mPrintDeviceContext->InitForPrinting(deviceContextSpec);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = mPrintDeviceContext->BeginDocument(aDocumentTitle, aPrintToFile,
                                          aStartPage, aEndPage);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

mozilla::ipc::IPCResult
RemotePrintJobParent::RecvProcessPage(const nsCString& aPageFileName)
{
  nsresult rv = PrintPage(aPageFileName);

  if (NS_FAILED(rv)) {
    Unused << SendAbortPrint(rv);
  } else {
    Unused << SendPageProcessed();
  }

  return IPC_OK();
}

nsresult
RemotePrintJobParent::PrintPage(const nsCString& aPageFileName)
{
  MOZ_ASSERT(mPrintDeviceContext);

  nsresult rv = mPrintDeviceContext->BeginPage();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<nsIFile> recordingFile;
  rv = NS_GetSpecialDirectory(NS_APP_CONTENT_PROCESS_TEMP_DIR,
                              getter_AddRefs(recordingFile));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = recordingFile->AppendNative(aPageFileName);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsAutoCString recordingPath;
  rv = recordingFile->GetNativePath(recordingPath);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  std::ifstream recording(recordingPath.get(), std::ifstream::binary);
  if (!mPrintTranslator->TranslateRecording(recording)) {
    return NS_ERROR_FAILURE;
  }

  rv = mPrintDeviceContext->EndPage();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  recording.close();
  rv = recordingFile->Remove(/* recursive= */ false);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

mozilla::ipc::IPCResult
RemotePrintJobParent::RecvFinalizePrint()
{
  // EndDocument is sometimes called in the child even when BeginDocument has
  // not been called. See bug 1223332.
  if (mPrintDeviceContext) {
    DebugOnly<nsresult> rv = mPrintDeviceContext->EndDocument();

    // Too late to abort the child just log.
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "EndDocument failed");
  }


  Unused << Send__delete__(this);
  return IPC_OK();
}

mozilla::ipc::IPCResult
RemotePrintJobParent::RecvAbortPrint(const nsresult& aRv)
{
  if (mPrintDeviceContext) {
    Unused << mPrintDeviceContext->AbortDocument();
  }

  Unused << Send__delete__(this);
  return IPC_OK();
}

mozilla::ipc::IPCResult
RemotePrintJobParent::RecvStateChange(const long& aStateFlags,
                                      const nsresult& aStatus)
{
  uint32_t numberOfListeners = mPrintProgressListeners.Length();
  for (uint32_t i = 0; i < numberOfListeners; ++i) {
    nsIWebProgressListener* listener = mPrintProgressListeners.SafeElementAt(i);
    listener->OnStateChange(nullptr, nullptr, aStateFlags, aStatus);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult
RemotePrintJobParent::RecvProgressChange(const long& aCurSelfProgress,
                                         const long& aMaxSelfProgress,
                                         const long& aCurTotalProgress,
                                         const long& aMaxTotalProgress)
{
  uint32_t numberOfListeners = mPrintProgressListeners.Length();
  for (uint32_t i = 0; i < numberOfListeners; ++i) {
    nsIWebProgressListener* listener = mPrintProgressListeners.SafeElementAt(i);
    listener->OnProgressChange(nullptr, nullptr,
                               aCurSelfProgress, aMaxSelfProgress,
                               aCurTotalProgress, aMaxTotalProgress);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult
RemotePrintJobParent::RecvStatusChange(const nsresult& aStatus)
{
  uint32_t numberOfListeners = mPrintProgressListeners.Length();
  for (uint32_t i = 0; i < numberOfListeners; ++i) {
    nsIWebProgressListener* listener = mPrintProgressListeners.SafeElementAt(i);
    listener->OnStatusChange(nullptr, nullptr, aStatus, nullptr);
  }

  return IPC_OK();
}

void
RemotePrintJobParent::RegisterListener(nsIWebProgressListener* aListener)
{
  MOZ_ASSERT(aListener);

  mPrintProgressListeners.AppendElement(aListener);
}

already_AddRefed<nsIPrintSettings>
RemotePrintJobParent::GetPrintSettings()
{
  nsCOMPtr<nsIPrintSettings> printSettings = mPrintSettings;
  return printSettings.forget();
}

RemotePrintJobParent::~RemotePrintJobParent()
{
  MOZ_COUNT_DTOR(RemotePrintJobParent);
}

void
RemotePrintJobParent::ActorDestroy(ActorDestroyReason aWhy)
{
}

} // namespace layout
} // namespace mozilla


