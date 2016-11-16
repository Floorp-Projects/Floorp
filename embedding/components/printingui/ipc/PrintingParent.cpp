/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/Element.h"
#include "mozilla/dom/TabParent.h"
#include "mozilla/Preferences.h"
#include "mozilla/Unused.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIDOMWindow.h"
#include "nsIPrintingPromptService.h"
#include "nsIPrintProgressParams.h"
#include "nsIPrintSettingsService.h"
#include "nsIServiceManager.h"
#include "nsServiceManagerUtils.h"
#include "nsIWebProgressListener.h"
#include "PrintingParent.h"
#include "PrintDataUtils.h"
#include "PrintProgressDialogParent.h"
#include "PrintSettingsDialogParent.h"
#include "mozilla/layout/RemotePrintJobParent.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::layout;

namespace mozilla {
namespace embedding {
mozilla::ipc::IPCResult
PrintingParent::RecvShowProgress(PBrowserParent* parent,
                                 PPrintProgressDialogParent* printProgressDialog,
                                 PRemotePrintJobParent* remotePrintJob,
                                 const bool& isForPrinting,
                                 bool* notifyOnOpen,
                                 nsresult* result)
{
  *result = NS_ERROR_FAILURE;
  *notifyOnOpen = false;

  nsCOMPtr<nsPIDOMWindowOuter> parentWin = DOMWindowFromBrowserParent(parent);
  if (!parentWin) {
    return IPC_OK();
  }

  nsCOMPtr<nsIPrintingPromptService> pps(do_GetService("@mozilla.org/embedcomp/printingprompt-service;1"));

  if (!pps) {
    return IPC_OK();
  }

  PrintProgressDialogParent* dialogParent =
    static_cast<PrintProgressDialogParent*>(printProgressDialog);
  nsCOMPtr<nsIObserver> observer = do_QueryInterface(dialogParent);

  nsCOMPtr<nsIWebProgressListener> printProgressListener;
  nsCOMPtr<nsIPrintProgressParams> printProgressParams;

  *result = pps->ShowProgress(parentWin, nullptr, nullptr, observer,
                              isForPrinting,
                              getter_AddRefs(printProgressListener),
                              getter_AddRefs(printProgressParams),
                              notifyOnOpen);
  NS_ENSURE_SUCCESS(*result, IPC_OK());

  if (remotePrintJob) {
    // If we have a RemotePrintJob use that as a more general forwarder for
    // print progress listeners.
    static_cast<RemotePrintJobParent*>(remotePrintJob)
      ->RegisterListener(printProgressListener);
  } else {
    dialogParent->SetWebProgressListener(printProgressListener);
  }

  dialogParent->SetPrintProgressParams(printProgressParams);

  return IPC_OK();
}

nsresult
PrintingParent::ShowPrintDialog(PBrowserParent* aParent,
                                const PrintData& aData,
                                PrintData* aResult)
{
  nsCOMPtr<nsPIDOMWindowOuter> parentWin = DOMWindowFromBrowserParent(aParent);
  if (!parentWin) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIPrintingPromptService> pps(do_GetService("@mozilla.org/embedcomp/printingprompt-service;1"));
  if (!pps) {
    return NS_ERROR_FAILURE;
  }

  // The initSettings we got can be wrapped using
  // PrintDataUtils' MockWebBrowserPrint, which implements enough of
  // nsIWebBrowserPrint to keep the dialogs happy.
  nsCOMPtr<nsIWebBrowserPrint> wbp = new MockWebBrowserPrint(aData);

  // Use the existing RemotePrintJob and its settings, if we have one, to make
  // sure they stay current.
  RemotePrintJobParent* remotePrintJob =
    static_cast<RemotePrintJobParent*>(aData.remotePrintJobParent());
  nsCOMPtr<nsIPrintSettings> settings;
  nsresult rv;
  if (remotePrintJob) {
    settings = remotePrintJob->GetPrintSettings();
  } else {
    rv = mPrintSettingsSvc->GetNewPrintSettings(getter_AddRefs(settings));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // We only want to use the print silently setting from the parent.
  bool printSilently;
  rv = settings->GetPrintSilent(&printSilently);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mPrintSettingsSvc->DeserializeToPrintSettings(aData, settings);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = settings->SetPrintSilent(printSilently);
  NS_ENSURE_SUCCESS(rv, rv);

  // If we are printing silently then we just need to initialize the print
  // settings with anything specific from the printer.
  if (printSilently ||
      Preferences::GetBool("print.always_print_silent", printSilently)) {
    nsXPIDLString printerName;
    rv = settings->GetPrinterName(getter_Copies(printerName));
    NS_ENSURE_SUCCESS(rv, rv);

    settings->SetIsInitializedFromPrinter(false);
    mPrintSettingsSvc->InitPrintSettingsFromPrinter(printerName, settings);
  } else {
    rv = pps->ShowPrintDialog(parentWin, wbp, settings);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  rv = SerializeAndEnsureRemotePrintJob(settings, nullptr, remotePrintJob,
                                        aResult);

  return rv;
}

mozilla::ipc::IPCResult
PrintingParent::RecvShowPrintDialog(PPrintSettingsDialogParent* aDialog,
                                    PBrowserParent* aParent,
                                    const PrintData& aData)
{
  PrintData resultData;
  nsresult rv = ShowPrintDialog(aParent, aData, &resultData);

  // The child has been spinning an event loop while waiting
  // to hear about the print settings. We return the results
  // with an async message which frees the child process from
  // its nested event loop.
  if (NS_FAILED(rv)) {
    mozilla::Unused << aDialog->Send__delete__(aDialog, rv);
  } else {
    mozilla::Unused << aDialog->Send__delete__(aDialog, resultData);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
PrintingParent::RecvSavePrintSettings(const PrintData& aData,
                                      const bool& aUsePrinterNamePrefix,
                                      const uint32_t& aFlags,
                                      nsresult* aResult)
{
  nsCOMPtr<nsIPrintSettings> settings;
  *aResult = mPrintSettingsSvc->GetNewPrintSettings(getter_AddRefs(settings));
  NS_ENSURE_SUCCESS(*aResult, IPC_OK());

  *aResult = mPrintSettingsSvc->DeserializeToPrintSettings(aData, settings);
  NS_ENSURE_SUCCESS(*aResult, IPC_OK());

  *aResult = mPrintSettingsSvc->SavePrintSettingsToPrefs(settings,
                                                        aUsePrinterNamePrefix,
                                                        aFlags);

  return IPC_OK();
}

PPrintProgressDialogParent*
PrintingParent::AllocPPrintProgressDialogParent()
{
  PrintProgressDialogParent* actor = new PrintProgressDialogParent();
  NS_ADDREF(actor); // De-ref'd in the __delete__ handler for
                    // PrintProgressDialogParent.
  return actor;
}

bool
PrintingParent::DeallocPPrintProgressDialogParent(PPrintProgressDialogParent* doomed)
{
  // We can't just delete the PrintProgressDialogParent since somebody might
  // still be holding a reference to it as nsIObserver, so just decrement the
  // refcount instead.
  PrintProgressDialogParent* actor = static_cast<PrintProgressDialogParent*>(doomed);
  NS_RELEASE(actor);
  return true;
}

PPrintSettingsDialogParent*
PrintingParent::AllocPPrintSettingsDialogParent()
{
  return new PrintSettingsDialogParent();
}

bool
PrintingParent::DeallocPPrintSettingsDialogParent(PPrintSettingsDialogParent* aDoomed)
{
  delete aDoomed;
  return true;
}

PRemotePrintJobParent*
PrintingParent::AllocPRemotePrintJobParent()
{
  MOZ_ASSERT_UNREACHABLE("No default constructors for implementations.");
  return nullptr;
}

bool
PrintingParent::DeallocPRemotePrintJobParent(PRemotePrintJobParent* aDoomed)
{
  delete aDoomed;
  return true;
}

void
PrintingParent::ActorDestroy(ActorDestroyReason aWhy)
{
}

nsPIDOMWindowOuter*
PrintingParent::DOMWindowFromBrowserParent(PBrowserParent* parent)
{
  if (!parent) {
    return nullptr;
  }

  TabParent* tabParent = TabParent::GetFrom(parent);
  if (!tabParent) {
    return nullptr;
  }

  nsCOMPtr<Element> frameElement = tabParent->GetOwnerElement();
  if (!frameElement) {
    return nullptr;
  }

  nsCOMPtr<nsIContent> frame(do_QueryInterface(frameElement));
  if (!frame) {
    return nullptr;
  }

  nsCOMPtr<nsPIDOMWindowOuter> parentWin = frame->OwnerDoc()->GetWindow();
  if (!parentWin) {
    return nullptr;
  }

  return parentWin;
}

nsresult
PrintingParent::SerializeAndEnsureRemotePrintJob(
  nsIPrintSettings* aPrintSettings, nsIWebProgressListener* aListener,
  layout::RemotePrintJobParent* aRemotePrintJob, PrintData* aPrintData)
{
  MOZ_ASSERT(aPrintData);

  nsresult rv;
  nsCOMPtr<nsIPrintSettings> printSettings;
  if (aPrintSettings) {
    printSettings = aPrintSettings;
  } else {
    rv = mPrintSettingsSvc->GetNewPrintSettings(getter_AddRefs(printSettings));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  rv = mPrintSettingsSvc->SerializeToPrintData(printSettings, nullptr,
                                               aPrintData);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  RemotePrintJobParent* remotePrintJob;
  if (aRemotePrintJob) {
    remotePrintJob = aRemotePrintJob;
    aPrintData->remotePrintJobParent() = remotePrintJob;
  } else {
    remotePrintJob = new RemotePrintJobParent(aPrintSettings);
    aPrintData->remotePrintJobParent() =
      SendPRemotePrintJobConstructor(remotePrintJob);
  }
  if (aListener) {
    remotePrintJob->RegisterListener(aListener);
  }

  return NS_OK;
}

PrintingParent::PrintingParent()
{
  MOZ_COUNT_CTOR(PrintingParent);

  mPrintSettingsSvc =
    do_GetService("@mozilla.org/gfx/printsettings-service;1");
  MOZ_ASSERT(mPrintSettingsSvc);
}

PrintingParent::~PrintingParent()
{
  MOZ_COUNT_DTOR(PrintingParent);
}

} // namespace embedding
} // namespace mozilla

