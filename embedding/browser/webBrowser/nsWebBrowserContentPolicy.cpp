/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: ft=cpp tw=78 sw=4 et ts=8
 *
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
 * The Original Code is embedding content-control code.
 *
 * The Initial Developer of the Original Code is
 * Mike Shaver.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#include "nsWebBrowserContentPolicy.h"
#include "nsIScriptGlobalObject.h"
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
    PRBool allowed = PR_TRUE;

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
