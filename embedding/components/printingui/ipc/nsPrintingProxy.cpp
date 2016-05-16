/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPrintingProxy.h"

#include "mozilla/ClearOnShutdown.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/TabChild.h"
#include "mozilla/layout/RemotePrintJobChild.h"
#include "mozilla/unused.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeOwner.h"
#include "nsIPrintingPromptService.h"
#include "nsPIDOMWindow.h"
#include "nsPrintOptionsImpl.h"
#include "nsServiceManagerUtils.h"
#include "PrintDataUtils.h"
#include "PrintProgressDialogChild.h"
#include "PrintSettingsDialogChild.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::embedding;
using namespace mozilla::layout;

static StaticRefPtr<nsPrintingProxy> sPrintingProxyInstance;

NS_IMPL_ISUPPORTS(nsPrintingProxy, nsIPrintingPromptService)

nsPrintingProxy::nsPrintingProxy()
{
}

nsPrintingProxy::~nsPrintingProxy()
{
}

/* static */
already_AddRefed<nsPrintingProxy>
nsPrintingProxy::GetInstance()
{
  if (!sPrintingProxyInstance) {
    sPrintingProxyInstance = new nsPrintingProxy();
    if (!sPrintingProxyInstance) {
      return nullptr;
    }
    nsresult rv = sPrintingProxyInstance->Init();
    if (NS_FAILED(rv)) {
      sPrintingProxyInstance = nullptr;
      return nullptr;
    }
    ClearOnShutdown(&sPrintingProxyInstance);
  }

  RefPtr<nsPrintingProxy> inst = sPrintingProxyInstance.get();
  return inst.forget();
}

nsresult
nsPrintingProxy::Init()
{
  mozilla::Unused << ContentChild::GetSingleton()->SendPPrintingConstructor(this);
  return NS_OK;
}

NS_IMETHODIMP
nsPrintingProxy::ShowPrintDialog(mozIDOMWindowProxy *parent,
                                 nsIWebBrowserPrint *webBrowserPrint,
                                 nsIPrintSettings *printSettings)
{
  NS_ENSURE_ARG(parent);
  NS_ENSURE_ARG(webBrowserPrint);
  NS_ENSURE_ARG(printSettings);

  // Get the TabChild for this nsIDOMWindow, which we can then pass up to
  // the parent.
  nsCOMPtr<nsPIDOMWindowOuter> pwin = nsPIDOMWindowOuter::From(parent);
  NS_ENSURE_STATE(pwin);
  nsCOMPtr<nsIDocShell> docShell = pwin->GetDocShell();
  NS_ENSURE_STATE(docShell);

  nsCOMPtr<nsITabChild> tabchild = docShell->GetTabChild();
  NS_ENSURE_STATE(tabchild);

  TabChild* pBrowser = static_cast<TabChild*>(tabchild.get());

  // Next, serialize the nsIWebBrowserPrint and nsIPrintSettings we were given.
  nsresult rv = NS_OK;
  nsCOMPtr<nsIPrintSettingsService> printSettingsSvc =
    do_GetService("@mozilla.org/gfx/printsettings-service;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  PrintData inSettings;
  rv = printSettingsSvc->SerializeToPrintData(printSettings, webBrowserPrint,
                                              &inSettings);
  NS_ENSURE_SUCCESS(rv, rv);

  // Now, the waiting game. The parent process should be showing
  // the printing dialog soon. In the meantime, we need to spin a
  // nested event loop while we wait for the results of the dialog
  // to be returned to us.

  RefPtr<PrintSettingsDialogChild> dialog = new PrintSettingsDialogChild();
  SendPPrintSettingsDialogConstructor(dialog);

  mozilla::Unused << SendShowPrintDialog(dialog, pBrowser, inSettings);

  while(!dialog->returned()) {
    NS_ProcessNextEvent(nullptr, true);
  }

  rv = dialog->result();
  NS_ENSURE_SUCCESS(rv, rv);

  rv = printSettingsSvc->DeserializeToPrintSettings(dialog->data(),
                                                    printSettings);
  return NS_OK;
}

NS_IMETHODIMP
nsPrintingProxy::ShowProgress(mozIDOMWindowProxy*      parent,
                              nsIWebBrowserPrint*      webBrowserPrint,    // ok to be null
                              nsIPrintSettings*        printSettings,      // ok to be null
                              nsIObserver*             openDialogObserver, // ok to be null
                              bool                     isForPrinting,
                              nsIWebProgressListener** webProgressListener,
                              nsIPrintProgressParams** printProgressParams,
                              bool*                  notifyOnOpen)
{
  NS_ENSURE_ARG(parent);
  NS_ENSURE_ARG(webProgressListener);
  NS_ENSURE_ARG(printProgressParams);
  NS_ENSURE_ARG(notifyOnOpen);

  // Get the TabChild for this nsIDOMWindow, which we can then pass up to
  // the parent.
  nsCOMPtr<nsPIDOMWindowOuter> pwin = nsPIDOMWindowOuter::From(parent);
  NS_ENSURE_STATE(pwin);
  nsCOMPtr<nsIDocShell> docShell = pwin->GetDocShell();
  NS_ENSURE_STATE(docShell);
  nsCOMPtr<nsITabChild> tabchild = docShell->GetTabChild();
  TabChild* pBrowser = static_cast<TabChild*>(tabchild.get());

  RefPtr<PrintProgressDialogChild> dialogChild =
    new PrintProgressDialogChild(openDialogObserver);

  SendPPrintProgressDialogConstructor(dialogChild);

  nsresult rv = NS_OK;
  mozilla::Unused << SendShowProgress(pBrowser, dialogChild,
                                      isForPrinting, notifyOnOpen, &rv);
  if (NS_FAILED(rv)) {
    return rv;
  }

  NS_ADDREF(*webProgressListener = dialogChild);
  NS_ADDREF(*printProgressParams = dialogChild);

  return NS_OK;
}

NS_IMETHODIMP
nsPrintingProxy::ShowPageSetup(mozIDOMWindowProxy *parent,
                               nsIPrintSettings *printSettings,
                               nsIObserver *aObs)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsPrintingProxy::ShowPrinterProperties(mozIDOMWindowProxy *parent,
                                       const char16_t *printerName,
                                       nsIPrintSettings *printSettings)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
nsPrintingProxy::SavePrintSettings(nsIPrintSettings* aPS,
                                   bool aUsePrinterNamePrefix,
                                   uint32_t aFlags)
{
  nsresult rv;
  nsCOMPtr<nsIPrintSettingsService> printSettingsSvc =
    do_GetService("@mozilla.org/gfx/printsettings-service;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  PrintData settings;
  rv = printSettingsSvc->SerializeToPrintData(aPS, nullptr, &settings);
  NS_ENSURE_SUCCESS(rv, rv);

  Unused << SendSavePrintSettings(settings, aUsePrinterNamePrefix, aFlags,
                                  &rv);
  return rv;
}

PPrintProgressDialogChild*
nsPrintingProxy::AllocPPrintProgressDialogChild()
{
  // The parent process will never initiate the PPrintProgressDialog
  // protocol connection, so no need to provide an allocator here.
  NS_NOTREACHED("Allocator for PPrintProgressDialogChild should not be "
                "called on nsPrintingProxy.");
  return nullptr;
}

bool
nsPrintingProxy::DeallocPPrintProgressDialogChild(PPrintProgressDialogChild* aActor)
{
  // The parent process will never initiate the PPrintProgressDialog
  // protocol connection, so no need to provide an deallocator here.
  NS_NOTREACHED("Deallocator for PPrintProgressDialogChild should not be "
                "called on nsPrintingProxy.");
  return false;
}

PPrintSettingsDialogChild*
nsPrintingProxy::AllocPPrintSettingsDialogChild()
{
  // The parent process will never initiate the PPrintSettingsDialog
  // protocol connection, so no need to provide an allocator here.
  NS_NOTREACHED("Allocator for PPrintSettingsDialogChild should not be "
                "called on nsPrintingProxy.");
  return nullptr;
}

bool
nsPrintingProxy::DeallocPPrintSettingsDialogChild(PPrintSettingsDialogChild* aActor)
{
  // The PrintSettingsDialogChild implements refcounting, and
  // will take itself out.
  return true;
}

PRemotePrintJobChild*
nsPrintingProxy::AllocPRemotePrintJobChild()
{
  RefPtr<RemotePrintJobChild> remotePrintJob = new RemotePrintJobChild();
  return remotePrintJob.forget().take();
}

bool
nsPrintingProxy::DeallocPRemotePrintJobChild(PRemotePrintJobChild* aDoomed)
{
  RemotePrintJobChild* remotePrintJob = static_cast<RemotePrintJobChild*>(aDoomed);
  NS_RELEASE(remotePrintJob);
  return true;
}
