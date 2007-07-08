/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Metrics extension.
 *
 * The Initial Developer of the Original Code is Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Brian Ryner <bryner@brianryner.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsWindowCollector.h"
#include "nsMetricsService.h"
#include "nsIObserverService.h"
#include "nsIServiceManager.h"
#include "nsPIDOMWindow.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeItem.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsServiceManagerUtils.h"
#include "nsDocShellCID.h"
#include "nsAutoPtr.h"
#include "nsITimer.h"
#include "nsComponentManagerUtils.h"

nsWindowCollector::nsWindowCollector()
{
}

nsWindowCollector::~nsWindowCollector()
{
}

NS_IMPL_ISUPPORTS2(nsWindowCollector, nsIMetricsCollector, nsIObserver)

NS_IMETHODIMP
nsWindowCollector::OnAttach()
{
  nsCOMPtr<nsIObserverService> obsSvc =
    do_GetService("@mozilla.org/observer-service;1");
  NS_ENSURE_STATE(obsSvc);

  nsresult rv = obsSvc->AddObserver(this, NS_WEBNAVIGATION_CREATE, PR_FALSE);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = obsSvc->AddObserver(this, NS_CHROME_WEBNAVIGATION_CREATE, PR_FALSE);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = obsSvc->AddObserver(this, "domwindowopened", PR_FALSE);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = obsSvc->AddObserver(this, "domwindowclosed", PR_FALSE);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = obsSvc->AddObserver(this, NS_METRICS_WEBNAVIGATION_DESTROY, PR_FALSE);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = obsSvc->AddObserver(this,
                           NS_METRICS_CHROME_WEBNAVIGATION_DESTROY, PR_FALSE);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP
nsWindowCollector::OnDetach()
{
  nsCOMPtr<nsIObserverService> obsSvc =
    do_GetService("@mozilla.org/observer-service;1");
  NS_ENSURE_STATE(obsSvc);

  nsresult rv = obsSvc->RemoveObserver(this, NS_WEBNAVIGATION_CREATE);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = obsSvc->RemoveObserver(this, NS_CHROME_WEBNAVIGATION_CREATE);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = obsSvc->RemoveObserver(this, "domwindowopened");
  NS_ENSURE_SUCCESS(rv, rv);
  rv = obsSvc->RemoveObserver(this, "domwindowclosed");
  NS_ENSURE_SUCCESS(rv, rv);
  rv = obsSvc->RemoveObserver(this, NS_METRICS_WEBNAVIGATION_DESTROY);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = obsSvc->RemoveObserver(this, NS_METRICS_CHROME_WEBNAVIGATION_DESTROY);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP
nsWindowCollector::OnNewLog()
{
  return NS_OK;
}

struct WindowOpenClosure
{
  WindowOpenClosure(nsISupports *subj, nsWindowCollector *coll)
      : subject(subj), collector(coll) { }

  nsCOMPtr<nsISupports> subject;
  nsRefPtr<nsWindowCollector> collector;
};

/* static */ void
nsWindowCollector::WindowOpenCallback(nsITimer *timer, void *closure)
{
  WindowOpenClosure *wc = static_cast<WindowOpenClosure *>(closure);
  wc->collector->LogWindowOpen(timer, wc->subject);

  delete wc;
}

void
nsWindowCollector::LogWindowOpen(nsITimer *timer, nsISupports *subject)
{
  mWindowOpenTimers.RemoveElement(timer);
  nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(subject);

  if (!window) {
    return;
  }

  nsCOMPtr<nsIDOMWindowInternal> opener;
  window->GetOpener(getter_AddRefs(opener));

  nsCOMPtr<nsIWritablePropertyBag2> properties;
  nsMetricsUtils::NewPropertyBag(getter_AddRefs(properties));
  if (!properties) {
    return;
  }

  if (opener) {
    properties->SetPropertyAsUint32(NS_LITERAL_STRING("opener"),
                                    nsMetricsService::GetWindowID(opener));
  }

  properties->SetPropertyAsUint32(NS_LITERAL_STRING("windowid"),
                                  nsMetricsService::GetWindowID(window));

  properties->SetPropertyAsACString(NS_LITERAL_STRING("action"),
                                    NS_LITERAL_CSTRING("open"));

  nsMetricsService *ms = nsMetricsService::get();
  if (ms) {
    ms->LogEvent(NS_LITERAL_STRING("window"), properties);
  }
}

NS_IMETHODIMP
nsWindowCollector::Observe(nsISupports *subject,
                           const char *topic,
                           const PRUnichar *data)
{
  nsCOMPtr<nsIWritablePropertyBag2> properties;
  nsresult rv = nsMetricsUtils::NewPropertyBag(getter_AddRefs(properties));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsPIDOMWindow> window;
  nsCString action;

  if (strcmp(topic, NS_WEBNAVIGATION_CREATE) == 0 ||
      strcmp(topic, NS_CHROME_WEBNAVIGATION_CREATE) == 0) {
    // Log a window creation event.
    action.Assign("create");

    nsCOMPtr<nsIDocShellTreeItem> item = do_QueryInterface(subject);
    NS_ENSURE_STATE(item);

    window = do_GetInterface(subject);
    NS_ENSURE_STATE(window);

    // We want the window's real parent, even if it crosses a chrome/content
    // boundary.  This requires going up the docshell tree.
    nsCOMPtr<nsIDocShellTreeItem> parentItem;
    item->GetParent(getter_AddRefs(parentItem));
    nsCOMPtr<nsPIDOMWindow> parentWindow = do_GetInterface(parentItem);
    if (parentWindow) {
      PRUint32 id = nsMetricsService::GetWindowID(parentWindow);
      rv = properties->SetPropertyAsUint32(NS_LITERAL_STRING("parent"), id);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    if (strcmp(topic, NS_CHROME_WEBNAVIGATION_CREATE) == 0) {
      rv = properties->SetPropertyAsBool(NS_LITERAL_STRING("chrome"), PR_TRUE);
      NS_ENSURE_SUCCESS(rv, rv);
    }
    if (nsMetricsUtils::IsSubframe(item)) {
      rv = properties->SetPropertyAsBool(NS_LITERAL_STRING("subframe"),
                                         PR_TRUE);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  } else if (strcmp(topic, "domwindowopened") == 0) {
    // We'd like to log a window open event now, but the window opener
    // has not yet been set when we receive the domwindowopened notification.

    nsCOMPtr<nsITimer> timer = do_CreateInstance(NS_TIMER_CONTRACTID);
    NS_ENSURE_STATE(timer);

    WindowOpenClosure *wc = new WindowOpenClosure(subject, this);
    NS_ENSURE_TRUE(wc, NS_ERROR_OUT_OF_MEMORY);

    rv = timer->InitWithFuncCallback(nsWindowCollector::WindowOpenCallback,
                                     wc, 0, nsITimer::TYPE_ONE_SHOT);
    NS_ENSURE_SUCCESS(rv, rv);

    mWindowOpenTimers.AppendElement(timer);
  } else if (strcmp(topic, "domwindowclosed") == 0) {
    // Log a window close event.
    action.Assign("close");
    window = do_QueryInterface(subject);
  } else if (strcmp(topic, NS_METRICS_WEBNAVIGATION_DESTROY) == 0 ||
             strcmp(topic, NS_METRICS_CHROME_WEBNAVIGATION_DESTROY) == 0) {
    // Log a window destroy event.
    action.Assign("destroy");
    window = do_GetInterface(subject);

    nsCOMPtr<nsIDocShellTreeItem> item = do_QueryInterface(subject);
    NS_ENSURE_STATE(item);
    if (nsMetricsUtils::IsSubframe(item)) {
      rv = properties->SetPropertyAsBool(NS_LITERAL_STRING("subframe"),
                                         PR_TRUE);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  if (window) {
    rv = properties->SetPropertyAsUint32(NS_LITERAL_STRING("windowid"),
                                         nsMetricsService::GetWindowID(window));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = properties->SetPropertyAsACString(NS_LITERAL_STRING("action"),
                                           action);
    NS_ENSURE_SUCCESS(rv, rv);

    nsMetricsService *ms = nsMetricsService::get();
    NS_ENSURE_STATE(ms);
    rv = ms->LogEvent(NS_LITERAL_STRING("window"), properties);
  }

  return rv;
}
