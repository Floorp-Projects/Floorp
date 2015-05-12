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
#include "nsIContentViewer.h"

nsWebBrowserContentPolicy::nsWebBrowserContentPolicy()
{
  MOZ_COUNT_CTOR(nsWebBrowserContentPolicy);
}

nsWebBrowserContentPolicy::~nsWebBrowserContentPolicy()
{
  MOZ_COUNT_DTOR(nsWebBrowserContentPolicy);
}

NS_IMPL_ISUPPORTS(nsWebBrowserContentPolicy, nsIContentPolicy)

NS_IMETHODIMP
nsWebBrowserContentPolicy::ShouldLoad(uint32_t aContentType,
                                      nsIURI* aContentLocation,
                                      nsIURI* aRequestingLocation,
                                      nsISupports* aRequestingContext,
                                      const nsACString& aMimeGuess,
                                      nsISupports* aExtra,
                                      nsIPrincipal* aRequestPrincipal,
                                      int16_t* aShouldLoad)
{
  NS_PRECONDITION(aShouldLoad, "Null out param");

  *aShouldLoad = nsIContentPolicy::ACCEPT;

  nsIDocShell* shell = NS_CP_GetDocShellFromContext(aRequestingContext);
  /* We're going to dereference shell, so make sure it isn't null */
  if (!shell) {
    return NS_OK;
  }

  nsresult rv;
  bool allowed = true;

  switch (aContentType) {
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
    case nsIContentPolicy::TYPE_IMAGESET:
      rv = shell->GetAllowImages(&allowed);
      break;
    default:
      return NS_OK;
  }

  if (NS_SUCCEEDED(rv) && !allowed) {
    *aShouldLoad = nsIContentPolicy::REJECT_TYPE;
  }
  return rv;
}

NS_IMETHODIMP
nsWebBrowserContentPolicy::ShouldProcess(uint32_t aContentType,
                                         nsIURI* aContentLocation,
                                         nsIURI* aRequestingLocation,
                                         nsISupports* aRequestingContext,
                                         const nsACString& aMimeGuess,
                                         nsISupports* aExtra,
                                         nsIPrincipal* aRequestPrincipal,
                                         int16_t* aShouldProcess)
{
  NS_PRECONDITION(aShouldProcess, "Null out param");

  *aShouldProcess = nsIContentPolicy::ACCEPT;

  // Object tags will always open channels with TYPE_OBJECT, but may end up
  // loading with TYPE_IMAGE or TYPE_DOCUMENT as their final type, so we block
  // actual-plugins at the process stage
  if (aContentType != nsIContentPolicy::TYPE_OBJECT) {
    return NS_OK;
  }

  nsIDocShell* shell = NS_CP_GetDocShellFromContext(aRequestingContext);
  if (shell && (!shell->PluginsAllowedInCurrentDoc())) {
    *aShouldProcess = nsIContentPolicy::REJECT_TYPE;
  }

  return NS_OK;
}
