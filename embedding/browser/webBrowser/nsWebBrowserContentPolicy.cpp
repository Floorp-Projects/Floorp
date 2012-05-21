/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: ft=cpp tw=78 sw=4 et ts=8
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsWebBrowserContentPolicy.h"
#include "nsIDocShell.h"
#include "nsCOMPtr.h"
#include "nsContentPolicyUtils.h"

nsWebBrowserContentPolicy::nsWebBrowserContentPolicy()
{
    MOZ_COUNT_CTOR(nsWebBrowserContentPolicy);
}

nsWebBrowserContentPolicy::~nsWebBrowserContentPolicy()
{
    MOZ_COUNT_DTOR(nsWebBrowserContentPolicy);
}

NS_IMPL_ISUPPORTS1(nsWebBrowserContentPolicy, nsIContentPolicy)

static nsresult
PerformPolicyCheck(PRUint32     contentType,
                   nsISupports *requestingContext,
                   PRInt16     *decision)
{
    NS_PRECONDITION(decision, "Null out param");

    *decision = nsIContentPolicy::ACCEPT;

    nsIDocShell *shell = NS_CP_GetDocShellFromContext(requestingContext);
    /* We're going to dereference shell, so make sure it isn't null */
    if (!shell)
        return NS_OK;

    nsresult rv;
    bool allowed = true;

    switch (contentType) {
      case nsIContentPolicy::TYPE_OBJECT:
        rv = shell->GetAllowPlugins(&allowed);
        break;
      case nsIContentPolicy::TYPE_SCRIPT:
        rv = shell->GetAllowJavascript(&allowed);
        break;
      case nsIContentPolicy::TYPE_SUBDOCUMENT:
        rv = shell->GetAllowSubframes(&allowed);
        break;
#if 0
      /* XXXtw: commented out in old code; add during conpol phase 2 */
      case nsIContentPolicy::TYPE_REFRESH:
        rv = shell->GetAllowMetaRedirects(&allowed); /* meta _refresh_ */
        break;
#endif
      case nsIContentPolicy::TYPE_IMAGE:
        rv = shell->GetAllowImages(&allowed);
        break;
      default:
        return NS_OK;
    }

    if (NS_SUCCEEDED(rv) && !allowed) {
        *decision = nsIContentPolicy::REJECT_TYPE;
    }
    return rv;
}

NS_IMETHODIMP
nsWebBrowserContentPolicy::ShouldLoad(PRUint32          contentType,
                                      nsIURI           *contentLocation,
                                      nsIURI           *requestingLocation,
                                      nsISupports      *requestingContext,
                                      const nsACString &mimeGuess,
                                      nsISupports      *extra,
                                      PRInt16          *shouldLoad)
{
    return PerformPolicyCheck(contentType, requestingContext, shouldLoad);
}

NS_IMETHODIMP
nsWebBrowserContentPolicy::ShouldProcess(PRUint32          contentType,
                                         nsIURI           *contentLocation,
                                         nsIURI           *requestingLocation,
                                         nsISupports      *requestingContext,
                                         const nsACString &mimeGuess,
                                         nsISupports      *extra,
                                         PRInt16          *shouldProcess)
{
    *shouldProcess = nsIContentPolicy::ACCEPT;
    return NS_OK;
    //LATER:
    //  return PerformPolicyCheck(contentType, requestingContext, shouldProcess);
}
