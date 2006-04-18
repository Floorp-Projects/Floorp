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

nsWindowCollector *nsWindowCollector::sInstance = nsnull;

/* static */ nsresult
nsWindowCollector::SetEnabled(PRBool enabled)
{
  if (enabled) {
    if (!sInstance) {
      sInstance = new nsWindowCollector();
      NS_ENSURE_TRUE(sInstance, NS_ERROR_OUT_OF_MEMORY);
      NS_ADDREF(sInstance);

      nsresult rv = sInstance->Init();
      if (NS_FAILED(rv)) {
        MS_LOG(("Failed to initialize the window collector"));
        NS_RELEASE(sInstance);
        return rv;
      }
    }
  } else {
    // We want to release our reference to sInstance so that it can
    // be destroyed.  However, window destroy events can happen during xpcom
    // shutdown (after we've been notified), and we need to still be able to
    // access sInstance to log those correctly.  So, this releases the
    // reference but keeps sInstance around until the destructor runs.

    if (sInstance) {
      sInstance->Release();
    }
  }

  return NS_OK;
}

NS_IMPL_ISUPPORTS1(nsWindowCollector, nsIObserver)

nsWindowCollector::~nsWindowCollector()
{
  NS_ASSERTION(sInstance == this, "two window collectors created?");
  sInstance = nsnull;
}

nsresult
nsWindowCollector::Init()
{
  nsresult rv;
  nsCOMPtr<nsIObserverService> obsSvc =
    do_GetService("@mozilla.org/observer-service;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = obsSvc->AddObserver(this, NS_WEBNAVIGATION_CREATE, PR_FALSE);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = obsSvc->AddObserver(this, NS_CHROME_WEBNAVIGATION_CREATE, PR_FALSE);
  NS_ENSURE_SUCCESS(rv, rv);
  // toplevel-window-ready is similar to "domwindowopened", but is dispatched
  // after the window has been initialized (the opener is set, etc).
  rv = obsSvc->AddObserver(this, "toplevel-window-ready", PR_FALSE);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = obsSvc->AddObserver(this, "domwindowclosed", PR_FALSE);
  NS_ENSURE_SUCCESS(rv, rv);

  // We receive NS_WEBNAVIGATION_DESTROY and NS_CHROME_WEBNAVIGATION_DESTROY
  // directly from the MetricsService.  This way, we avoid a dependency
  // on the order in which observers fire, which is not guaranteed.

  return NS_OK;
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
  } else if (strcmp(topic, "toplevel-window-ready") == 0) {
    // Log a window open event.
    action.Assign("open");

    window = do_QueryInterface(subject);
    NS_ENSURE_STATE(window);

    nsCOMPtr<nsIDOMWindowInternal> opener;
    window->GetOpener(getter_AddRefs(opener));
    if (opener) {
      // Toplevel windows opened from native code have no opener.
      rv = properties->SetPropertyAsUint32(NS_LITERAL_STRING("opener"),
                                         nsMetricsService::GetWindowID(opener));
      NS_ENSURE_SUCCESS(rv, rv);
    }
  } else if (strcmp(topic, "domwindowclosed") == 0) {
    // Log a window close event.
    action.Assign("close");
    window = do_QueryInterface(subject);
  } else if (strcmp(topic, NS_WEBNAVIGATION_DESTROY) == 0 ||
             strcmp(topic, NS_CHROME_WEBNAVIGATION_DESTROY) == 0) {
    // Log a window destroy event.
    action.Assign("destroy");
    window = do_GetInterface(subject);
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
