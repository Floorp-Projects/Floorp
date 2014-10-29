/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set sw=4 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/Element.h"
#include "mozilla/dom/TabParent.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIDOMWindow.h"
#include "nsIPrintingPromptService.h"
#include "nsIPrintProgressParams.h"
#include "nsIServiceManager.h"
#include "nsIWebProgressListener.h"
#include "PrintingParent.h"
#include "nsIPrintOptions.h"
#include "PrintDataUtils.h"

using namespace mozilla;
using namespace mozilla::dom;

namespace mozilla {
namespace embedding {
bool
PrintingParent::RecvShowProgress(PBrowserParent* parent,
                                 const bool& isForPrinting)
{
  TabParent* tabParent = static_cast<TabParent*>(parent);
  if (!tabParent) {
    return true;
  }

  nsCOMPtr<Element> frameElement = tabParent->GetOwnerElement();
  if (!frameElement) {
    return true;
  }

  nsCOMPtr<nsIContent> frame(do_QueryInterface(frameElement));
  if (!frame) {
    return true;
  }

  nsCOMPtr<nsIDOMWindow> parentWin = do_QueryInterface(frame->OwnerDoc()->GetWindow());
  if (!parentWin) {
    return true;
  }

  nsCOMPtr<nsIPrintingPromptService> pps(do_GetService("@mozilla.org/embedcomp/printingprompt-service;1"));

  if (!pps) {
    return true;
  }

  nsCOMPtr<nsIWebProgressListener> printProgressListener;
  nsCOMPtr<nsIPrintProgressParams> printProgressParams;

  // TODO: What do I do with this thing?
  bool doNotify = false;

  pps->ShowProgress(parentWin, nullptr, nullptr, nullptr,
                    isForPrinting,
                    getter_AddRefs(printProgressListener),
                    getter_AddRefs(printProgressParams),
                    &doNotify);

  return true;
}

bool
PrintingParent::RecvShowPrintDialog(PBrowserParent* parent,
                                    const PrintData& data,
                                    PrintData* retVal,
                                    bool* success)
{
  *success = false;

  TabParent* tabParent = static_cast<TabParent*>(parent);
  if (!tabParent) {
    return true;
  }

  nsCOMPtr<Element> frameElement = tabParent->GetOwnerElement();
  if (!frameElement) {
    return true;
  }

  nsCOMPtr<nsIContent> frame(do_QueryInterface(frameElement));
  if (!frame) {
    return true;
  }

  nsCOMPtr<nsIDOMWindow> parentWin = do_QueryInterface(frame->OwnerDoc()->GetWindow());
  if (!parentWin) {
    return true;
  }

  nsCOMPtr<nsIPrintingPromptService> pps(do_GetService("@mozilla.org/embedcomp/printingprompt-service;1"));

  if (!pps) {
    return true;
  }

  // The initSettings we got can be wrapped using
  // PrintDataUtils' MockWebBrowserPrint, which implements enough of
  // nsIWebBrowserPrint to keep the dialogs happy.
  nsCOMPtr<nsIWebBrowserPrint> wbp = new MockWebBrowserPrint(data);

  nsresult rv;
  nsCOMPtr<nsIPrintOptions> po = do_GetService("@mozilla.org/gfx/printsettings-service;1", &rv);
  NS_ENSURE_SUCCESS(rv, true);

  nsCOMPtr<nsIPrintSettings> settings;
  rv = po->CreatePrintSettings(getter_AddRefs(settings));
  NS_ENSURE_SUCCESS(rv, true);

  rv = po->DeserializeToPrintSettings(data, settings);
  NS_ENSURE_SUCCESS(rv, true);

  rv = pps->ShowPrintDialog(parentWin, wbp, settings);
  NS_ENSURE_SUCCESS(rv, true);

  // And send it back.
  PrintData result;
  rv = po->SerializeToPrintData(settings, nullptr, &result);
  NS_ENSURE_SUCCESS(rv, true);

  *retVal = result;
  *success = true;
  return true;
}

void
PrintingParent::ActorDestroy(ActorDestroyReason aWhy)
{
}

MOZ_IMPLICIT PrintingParent::PrintingParent()
{
    MOZ_COUNT_CTOR(PrintingParent);
}

MOZ_IMPLICIT PrintingParent::~PrintingParent()
{
    MOZ_COUNT_DTOR(PrintingParent);
}

} // namespace embedding
} // namespace mozilla

