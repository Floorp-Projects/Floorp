/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 sts=2
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Shawn Wilsher <me@shawnwilsher.com> (Original Author)
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

#include "nsDownloadHistory.h"
#include "nsCOMPtr.h"
#include "nsServiceManagerUtils.h"
#include "nsIGlobalHistory.h"
#include "nsIGlobalHistory2.h"
#include "nsIObserverService.h"
#include "nsIURI.h"
#include "nsIComponentRegistrar.h"
#include "nsDocShellCID.h"

////////////////////////////////////////////////////////////////////////////////
//// nsDownloadHistory

NS_IMPL_ISUPPORTS1(nsDownloadHistory, nsIDownloadHistory)

////////////////////////////////////////////////////////////////////////////////
//// nsIDownloadHistory

NS_IMETHODIMP
nsDownloadHistory::AddDownload(nsIURI *aSource,
                               nsIURI *aReferrer,
                               PRTime aStartTime,
                               nsIURI *aDestination)
{
  NS_ENSURE_ARG_POINTER(aSource);

  nsCOMPtr<nsIGlobalHistory2> history =
    do_GetService("@mozilla.org/browser/global-history;2");
  if (!history)
    return NS_ERROR_NOT_AVAILABLE;

  PRBool visited;
  nsresult rv = history->IsVisited(aSource, &visited);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = history->AddURI(aSource, PR_FALSE, PR_TRUE, aReferrer);
  NS_ENSURE_SUCCESS(rv, rv);
  
  if (!visited) {
    nsCOMPtr<nsIObserverService> os =
      do_GetService("@mozilla.org/observer-service;1");
    if (os)
      os->NotifyObservers(aSource, NS_LINK_VISITED_EVENT_TOPIC, nsnull);
  }

  return NS_OK;
}
