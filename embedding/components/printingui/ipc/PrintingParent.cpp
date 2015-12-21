/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/Element.h"
#include "mozilla/dom/TabParent.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIDOMWindow.h"
#include "nsIPrintingPromptService.h"
#include "nsIPrintOptions.h"
#include "nsIPrintProgressParams.h"
#include "nsIPrintSettingsService.h"
#include "nsIServiceManager.h"
#include "nsIWebProgressListener.h"
#include "PrintingParent.h"
#include "PrintDataUtils.h"
#include "PrintProgressDialogParent.h"
#include "PrintSettingsDialogParent.h"
#include "mozilla/layout/RemotePrintJobParent.h"

using namespace mozilla;
using namespace mozilla::dom;

namespace mozilla {
namespace embedding {
bool
PrintingParent::RecvShowProgress(PBrowserParent* parent,
                                 PPrintProgressDialogParent* printProgressDialog,
                                 const bool& isForPrinting,
                                 bool* notifyOnOpen,
                                 nsresult* result)
{
  *result = NS_ERROR_FAILURE;
  *notifyOnOpen = false;

  nsCOMPtr<nsIDOMWindow> parentWin = DOMWindowFromBrowserParent(parent);
  if (!parentWin) {
    return true;
  }

  nsCOMPtr<nsIPrintingPromptService> pps(do_GetService("@mozilla.org/embedcomp/printingprompt-service;1"));

  if (!pps) {
    return true;
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
  NS_ENSURE_SUCCESS(*result, true);

  dialogParent->SetWebProgressListener(printProgressListener);
  dialogParent->SetPrintProgressParams(printProgressParams);

  return true;
}

nsresult
PrintingParent::ShowPrintDialog(PBrowserParent* aParent,
                                const PrintData& aData,
                                PrintData* aResult)
{
  nsCOMPtr<nsIDOMWindow> parentWin = DOMWindowFromBrowserParent(aParent);
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

  nsresult rv;
  nsCOMPtr<nsIPrintOptions> po = do_GetService("@mozilla.org/gfx/printsettings-service;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIPrintSettings> settings;
  rv = po->CreatePrintSettings(getter_AddRefs(settings));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = po->DeserializeToPrintSettings(aData, settings);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = pps->ShowPrintDialog(parentWin, wbp, settings);
  NS_ENSURE_SUCCESS(rv, rv);

  // And send it back.
  rv = po->SerializeToPrintData(settings, nullptr, aResult);
  return rv;
}

bool
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
  return true;
}

bool
PrintingParent::RecvSavePrintSettings(const PrintData& aData,
                                      const bool& aUsePrinterNamePrefix,
                                      const uint32_t& aFlags,
                                      nsresult* aResult)
{
  nsCOMPtr<nsIPrintSettingsService> pss =
    do_GetService("@mozilla.org/gfx/printsettings-service;1", aResult);
  NS_ENSURE_SUCCESS(*aResult, true);

  nsCOMPtr<nsIPrintOptions> po = do_QueryInterface(pss, aResult);
  NS_ENSURE_SUCCESS(*aResult, true);

  nsCOMPtr<nsIPrintSettings> settings;
  *aResult = po->CreatePrintSettings(getter_AddRefs(settings));
  NS_ENSURE_SUCCESS(*aResult, true);

  *aResult = po->DeserializeToPrintSettings(aData, settings);
  NS_ENSURE_SUCCESS(*aResult, true);

  *aResult = pss->SavePrintSettingsToPrefs(settings, aUsePrinterNamePrefix, aFlags);

  return true;
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

nsIDOMWindow*
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

  nsCOMPtr<nsIDOMWindow> parentWin = do_QueryInterface(frame->OwnerDoc()->GetWindow());
  if (!parentWin) {
    return nullptr;
  }

  return parentWin;
}

PrintingParent::PrintingParent()
{
    MOZ_COUNT_CTOR(PrintingParent);
}

PrintingParent::~PrintingParent()
{
    MOZ_COUNT_DTOR(PrintingParent);
}

} // namespace embedding
} // namespace mozilla

