/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RemotePrintJobParent.h"

#include <istream>

#include "gfxContext.h"
#include "mozilla/Attributes.h"
#include "mozilla/unused.h"
#include "nsComponentManagerUtils.h"
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

bool
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
    return true;
  }

  mPrintTranslator.reset(new PrintTranslator(mPrintDeviceContext));
  Unused << SendPrintInitializationResult(NS_OK);

  return true;
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

bool
RemotePrintJobParent::RecvProcessPage(Shmem&& aStoredPage)
{
  nsresult rv = PrintPage(aStoredPage);

  // Always deallocate the shared memory no matter what the result.
  if (!DeallocShmem(aStoredPage)) {
    NS_WARNING("Failed to deallocated shared memory, remote print will abort.");
    rv = NS_ERROR_FAILURE;
  }

  if (NS_FAILED(rv)) {
    Unused << SendAbortPrint(rv);
  } else {
    Unused << SendPageProcessed();
  }

  return true;
}

nsresult
RemotePrintJobParent::PrintPage(const Shmem& aStoredPage)
{
  MOZ_ASSERT(mPrintDeviceContext);

  nsresult rv = mPrintDeviceContext->BeginPage();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  std::istringstream recording(std::string(aStoredPage.get<char>(),
                                           aStoredPage.Size<char>()));
  if (!mPrintTranslator->TranslateRecording(recording)) {
    return NS_ERROR_FAILURE;
  }

  rv = mPrintDeviceContext->EndPage();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

bool
RemotePrintJobParent::RecvFinalizePrint()
{
  // EndDocument is sometimes called in the child even when BeginDocument has
  // not been called. See bug 1223332.
  if (mPrintDeviceContext) {
    nsresult rv = mPrintDeviceContext->EndDocument();

    // Too late to abort the child just log.
    NS_WARN_IF(NS_FAILED(rv));
  }


  Unused << Send__delete__(this);
  return true;
}

bool
RemotePrintJobParent::RecvAbortPrint(const nsresult& aRv)
{
  if (mPrintDeviceContext) {
    Unused << mPrintDeviceContext->AbortDocument();
  }

  Unused << Send__delete__(this);
  return true;
}

bool
RemotePrintJobParent::RecvStateChange(const long& aStateFlags,
                                      const nsresult& aStatus)
{
  uint32_t numberOfListeners = mPrintProgressListeners.Length();
  for (uint32_t i = 0; i < numberOfListeners; ++i) {
    nsIWebProgressListener* listener = mPrintProgressListeners.SafeElementAt(i);
    listener->OnStateChange(nullptr, nullptr, aStateFlags, aStatus);
  }

  return true;
}

bool
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

  return true;
}

bool
RemotePrintJobParent::RecvStatusChange(const nsresult& aStatus)
{
  uint32_t numberOfListeners = mPrintProgressListeners.Length();
  for (uint32_t i = 0; i < numberOfListeners; ++i) {
    nsIWebProgressListener* listener = mPrintProgressListeners.SafeElementAt(i);
    listener->OnStatusChange(nullptr, nullptr, aStatus, nullptr);
  }

  return true;
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


