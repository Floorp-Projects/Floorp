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
#include "nsDocShellCID.h"
#include "nsAutoPtr.h"

static nsWindowCollector *gWindowCollector = nsnull;

/* static */ nsresult
nsWindowCollector::Startup()
{
  NS_ASSERTION(!gWindowCollector, "nsWindowCollector::Startup called twice");

  gWindowCollector = new nsWindowCollector();
  NS_ENSURE_TRUE(gWindowCollector, NS_ERROR_OUT_OF_MEMORY);
  NS_ADDREF(gWindowCollector);

  nsresult rv = gWindowCollector->Init();
  if (NS_FAILED(rv)) {
    NS_RELEASE(gWindowCollector);
    return rv;
  }

  return NS_OK;
}

/* static */ void
nsWindowCollector::Shutdown()
{
  // We want to release our reference to gWindowCollector so that it can
  // be destroyed.  However, window destroy events can happen during xpcom
  // shutdown (after we've been notified), and we need to still be able to
  // access gWindowCollector to log those correctly.  So, this releases the
  // reference but keeps gWindowCollector around until the destructor runs.

  gWindowCollector->Release();
}

NS_IMPL_ISUPPORTS1(nsWindowCollector, nsIObserver)

nsWindowCollector::~nsWindowCollector()
{
  NS_ASSERTION(gWindowCollector == this, "two window collectors created?");
  gWindowCollector = nsnull;
}

nsresult
nsWindowCollector::Init()
{
  NS_ENSURE_TRUE(mWindowMap.Init(32), NS_ERROR_OUT_OF_MEMORY);

  nsresult rv;
  nsCOMPtr<nsIObserverService> obsSvc =
    do_GetService("@mozilla.org/observer-service;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = obsSvc->AddObserver(this, "domwindowcreated", PR_FALSE);
  NS_ENSURE_SUCCESS(rv, rv);
  // toplevel-window-ready is similar to "domwindowopened", but is dispatched
  // after the window has been initialized (the opener is set, etc).
  rv = obsSvc->AddObserver(this, "toplevel-window-ready", PR_FALSE);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = obsSvc->AddObserver(this, "domwindowclosed", PR_FALSE);
  NS_ENSURE_SUCCESS(rv, rv);
  // domwindowdestroyed happens too late during shutdown (inside the final gc)
  // for us to be able to construct XML elements.  NS_WEBNAVIGATION_DESTROY and
  // NS_CHROME_WEBNAVIGATION_DESTROY happen at a more convenient time.
  rv = obsSvc->AddObserver(this, NS_WEBNAVIGATION_DESTROY, PR_FALSE);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = obsSvc->AddObserver(this, NS_CHROME_WEBNAVIGATION_DESTROY, PR_FALSE);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP
nsWindowCollector::Observe(nsISupports *subject,
                           const char *topic,
                           const PRUnichar *data)
{
  nsresult rv;
  if (strcmp(topic, "domwindowcreated") == 0) {
    // Log a window creation event.
    nsRefPtr<nsHashPropertyBag> properties;
    rv = nsMetricsUtils::NewPropertyBag(getter_AddRefs(properties));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(subject);
    NS_ENSURE_STATE(window);

    rv = nsMetricsUtils::PutUint16(properties,
                                   NS_LITERAL_STRING("window"),
                                   GetWindowID(window));
    NS_ENSURE_SUCCESS(rv, rv);

    // We want the window's real parent, even if it crosses a chrome/content
    // boundary.  This requires going up the docshell tree.
    nsCOMPtr<nsIDocShellTreeItem> item =
      do_QueryInterface(window->GetDocShell());
    if (item) {
      nsCOMPtr<nsIDocShellTreeItem> parentItem;
      item->GetParent(getter_AddRefs(parentItem));
      nsCOMPtr<nsPIDOMWindow> parentWindow = do_GetInterface(parentItem);
      if (parentWindow) {
        rv = nsMetricsUtils::PutUint16(properties,
                                       NS_LITERAL_STRING("parent"),
                                       GetWindowID(parentWindow));
        NS_ENSURE_SUCCESS(rv, rv);
      }
    }

    PRInt32 itemType;
    item->GetItemType(&itemType);
    if (itemType == nsIDocShellTreeItem::typeChrome) {
      rv = properties->SetPropertyAsBool(NS_LITERAL_STRING("chrome"), PR_TRUE);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    nsMetricsService *ms = nsMetricsService::get();
    rv = ms->LogEvent(NS_LITERAL_STRING("windowcreate"), properties);
    NS_ENSURE_SUCCESS(rv, rv);
  } else if (strcmp(topic, "toplevel-window-ready") == 0) {
    // Log a window open event.
    nsRefPtr<nsHashPropertyBag> properties;
    rv = nsMetricsUtils::NewPropertyBag(getter_AddRefs(properties));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(subject);
    NS_ENSURE_STATE(window);

    rv = nsMetricsUtils::PutUint16(properties,
                                   NS_LITERAL_STRING("window"),
                                   GetWindowID(window));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIDOMWindowInternal> opener;
    window->GetOpener(getter_AddRefs(opener));
    if (opener) {
      // Toplevel windows opened from native code have no opener.
      rv = nsMetricsUtils::PutUint16(properties,
                                     NS_LITERAL_STRING("opener"),
                                     GetWindowID(opener));
      NS_ENSURE_SUCCESS(rv, rv);
    }

    nsMetricsService *ms = nsMetricsService::get();
    rv = ms->LogEvent(NS_LITERAL_STRING("windowopen"), properties);
    NS_ENSURE_SUCCESS(rv, rv);
  } else if (strcmp(topic, "domwindowclosed") == 0) {
    // Log a window close event.
    nsRefPtr<nsHashPropertyBag> properties;
    rv = nsMetricsUtils::NewPropertyBag(getter_AddRefs(properties));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(subject);
    NS_ENSURE_STATE(window);

    rv = nsMetricsUtils::PutUint16(properties,
                                   NS_LITERAL_STRING("window"),
                                   GetWindowID(window));
    NS_ENSURE_SUCCESS(rv, rv);

    nsMetricsService *ms = nsMetricsService::get();
    rv = ms->LogEvent(NS_LITERAL_STRING("windowclose"), properties);
    NS_ENSURE_SUCCESS(rv, rv);
  } else if (strcmp(topic, NS_WEBNAVIGATION_DESTROY) == 0 ||
             strcmp(topic, NS_CHROME_WEBNAVIGATION_DESTROY) == 0) {
    // Log a window destroy event.
    nsRefPtr<nsHashPropertyBag> properties;
    rv = nsMetricsUtils::NewPropertyBag(getter_AddRefs(properties));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsPIDOMWindow> window = do_GetInterface(subject);
    NS_ENSURE_STATE(window);

    rv = nsMetricsUtils::PutUint16(properties,
                                   NS_LITERAL_STRING("window"),
                                   GetWindowID(window));
    NS_ENSURE_SUCCESS(rv, rv);

    nsMetricsService *ms = nsMetricsService::get();
    rv = ms->LogEvent(NS_LITERAL_STRING("windowdestroy"), properties);

    // Remove the window from our map.
    mWindowMap.Remove(subject);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

/* static */ PRUint16
nsWindowCollector::GetWindowID(nsIDOMWindow *window)
{
  if (!gWindowCollector) {
    NS_NOTREACHED("window collector not created");
    return PR_UINT16_MAX;
  }

  PRUint16 id;
  if (!gWindowCollector->mWindowMap.Get(window, &id)) {
    id = gWindowCollector->mNextWindowID++;
    gWindowCollector->mWindowMap.Put(window, id);
  }

  return id;
}
