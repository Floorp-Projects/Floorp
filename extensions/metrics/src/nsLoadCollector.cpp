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

#include "nsLoadCollector.h"
#include "nsWindowCollector.h"
#include "nsMetricsService.h"
#include "nsCOMPtr.h"
#include "nsCURILoader.h"
#include "nsIServiceManager.h"
#include "nsIWebProgress.h"
#include "nsDocShellLoadTypes.h"
#include "nsIChannel.h"
#include "nsPIDOMWindow.h"

static nsLoadCollector *gLoadCollector = nsnull;

NS_IMPL_ISUPPORTS2(nsLoadCollector,
                   nsIWebProgressListener, nsISupportsWeakReference)

NS_IMETHODIMP
nsLoadCollector::OnStateChange(nsIWebProgress *webProgress,
                               nsIRequest *request,
                               PRUint32 flags,
                               nsresult status)
{
  NS_ASSERTION(flags & STATE_IS_DOCUMENT,
               "incorrect state change notification");

#ifdef PR_LOGGING
  if (MS_LOG_ENABLED()) {
    nsCAutoString name;
    request->GetName(name);

    MS_LOG(("LoadCollector: progress = %p, request = %p [%s], flags = %x, status = %x",
            webProgress, request, name.get(), flags, status));
  }
#endif

  nsCOMPtr<nsIChannel> channel = do_QueryInterface(request);
  if (!channel) {
    // We don't care about non-channel requests
    return NS_OK;
  }

  nsresult rv;
  if (flags & STATE_START) {
    RequestEntry entry;
    NS_ASSERTION(!mRequestMap.Get(request, &entry), "duplicate STATE_START");

    nsCOMPtr<nsIDOMWindow> window;
    webProgress->GetDOMWindow(getter_AddRefs(window));
    if (!window) {
      // We don't really care about windowless loads
      return NS_OK;
    }

    rv = nsMetricsUtils::NewPropertyBag(getter_AddRefs(entry.properties));
    NS_ENSURE_SUCCESS(rv, rv);
    nsHashPropertyBag *props = entry.properties;

    rv = nsMetricsUtils::PutUint16(props, NS_LITERAL_STRING("window"),
                                   nsWindowCollector::GetWindowID(window));
    NS_ENSURE_SUCCESS(rv, rv);

    if (flags & STATE_RESTORING) {
      rv = props->SetPropertyAsBool(NS_LITERAL_STRING("bfCacheHit"), PR_TRUE);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    nsCOMPtr<nsPIDOMWindow> winPriv = do_QueryInterface(window);
    NS_ENSURE_STATE(winPriv);

    nsIDocShell *docShell = winPriv->GetDocShell();
    nsAutoString origin;
    if (docShell) {
      PRUint32 loadType;
      docShell->GetLoadType(&loadType);
      switch (loadType) {
        case LOAD_NORMAL:
        case LOAD_NORMAL_REPLACE:
        case LOAD_BYPASS_HISTORY:
          origin.AssignLiteral("typed");
          break;
        case LOAD_NORMAL_EXTERNAL:
          origin.AssignLiteral("external");
          break;
        case LOAD_HISTORY:
          origin.AssignLiteral("session-history");
          break;
        case LOAD_RELOAD_NORMAL:
        case LOAD_RELOAD_BYPASS_CACHE:
        case LOAD_RELOAD_BYPASS_PROXY:
        case LOAD_RELOAD_BYPASS_PROXY_AND_CACHE:
        case LOAD_RELOAD_CHARSET_CHANGE:
          origin.AssignLiteral("reload");
          break;
        case LOAD_LINK:
          origin.AssignLiteral("link");
          break;
        case LOAD_REFRESH:
          origin.AssignLiteral("refresh");
          break;
        default:
          origin.AssignLiteral("other");
          break;
      }
    }
    rv = props->SetPropertyAsAString(NS_LITERAL_STRING("origin"), origin);
    NS_ENSURE_SUCCESS(rv, rv);
    entry.startTime = PR_Now();
    NS_ENSURE_TRUE(mRequestMap.Put(request, entry), NS_ERROR_OUT_OF_MEMORY);
  } else if (flags & STATE_STOP) {
    RequestEntry entry;
    if (mRequestMap.Get(request, &entry)) {
      nsHashPropertyBag *props = entry.properties;

      // Compute the load time now that we have the end time.
      PRInt64 loadTime = (PR_Now() - entry.startTime) / PR_USEC_PER_MSEC;
      rv = props->SetPropertyAsUint64(NS_LITERAL_STRING("loadtime"), loadTime);
      NS_ENSURE_SUCCESS(rv, rv);

      nsMetricsService *ms = nsMetricsService::get();
      rv = ms->LogEvent(NS_LITERAL_STRING("load"), props);

      mRequestMap.Remove(request);
      NS_ENSURE_SUCCESS(rv, rv);
    } else {
      NS_WARNING("STATE_STOP without STATE_START");
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsLoadCollector::OnProgressChange(nsIWebProgress *webProgress,
                                  nsIRequest *request,
                                  PRInt32 curSelfProgress,
                                  PRInt32 maxSelfProgress,
                                  PRInt32 curTotalProgress,
                                  PRInt32 maxTotalProgress)
{
  return NS_OK;
}

NS_IMETHODIMP
nsLoadCollector::OnLocationChange(nsIWebProgress *webProgress,
                                  nsIRequest *request, nsIURI *location)
{
  return NS_OK;
}

NS_IMETHODIMP
nsLoadCollector::OnStatusChange(nsIWebProgress *webProgress,
                                nsIRequest *request,
                                nsresult status, const PRUnichar *messaage)
{
  return NS_OK;
}

NS_IMETHODIMP
nsLoadCollector::OnSecurityChange(nsIWebProgress *webProgress,
                                  nsIRequest *request, PRUint32 state)
{
  return NS_OK;
}

/* static */ nsresult
nsLoadCollector::Startup()
{
  NS_ASSERTION(!gLoadCollector, "nsLoadCollector::Startup called twice");

  gLoadCollector = new nsLoadCollector();
  NS_ENSURE_TRUE(gLoadCollector, NS_ERROR_OUT_OF_MEMORY);
  NS_ADDREF(gLoadCollector);

  nsresult rv = gLoadCollector->Init();
  if (NS_FAILED(rv)) {
    NS_RELEASE(gLoadCollector);
    return rv;
  }

  return NS_OK;
}

/* static */ void
nsLoadCollector::Shutdown()
{
  // See comments in nsWindowCollector::Shutdown about why we don't
  // null out gLoadCollector here.
  gLoadCollector->Release();
}

nsLoadCollector::~nsLoadCollector()
{
  NS_ASSERTION(gLoadCollector == this, "two load collectors created?");
  gLoadCollector = nsnull;
}

nsresult
nsLoadCollector::Init()
{
  NS_ENSURE_TRUE(mRequestMap.Init(32), NS_ERROR_OUT_OF_MEMORY);

  // Attach the LoadCollector as a global web progress listener
  nsresult rv;
  nsCOMPtr<nsIWebProgress> progress =
    do_GetService(NS_DOCUMENTLOADER_SERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  
  rv = progress->AddProgressListener(this,
                                     nsIWebProgress::NOTIFY_STATE_DOCUMENT);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}
