/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
#include "nsIDOMWindow.h"

nsWebBrowserContentPolicy::nsWebBrowserContentPolicy()
{
    MOZ_COUNT_CTOR(nsWebBrowserContentPolicy);
}

nsWebBrowserContentPolicy::~nsWebBrowserContentPolicy()
{
    MOZ_COUNT_DTOR(nsWebBrowserContentPolicy);
}

NS_IMPL_ISUPPORTS1(nsWebBrowserContentPolicy, nsIContentPolicy)

NS_IMETHODIMP
nsWebBrowserContentPolicy::ShouldLoad(PRInt32 contentType,
                                      nsIURI *contentLocation,
                                      nsISupports *context,
                                      nsIDOMWindow *window, PRBool *shouldLoad)
{
    *shouldLoad = PR_TRUE;

    nsCOMPtr<nsIScriptGlobalObject> scriptGlobal = 
        do_QueryInterface(window);
    if (!scriptGlobal)
        return NS_OK;

    nsIDocShell *shell = scriptGlobal->GetDocShell();
    /* We're going to dereference shell, so make sure it isn't null */
    if (!shell)
      return NS_OK;
    
    switch (contentType) {
      case nsIContentPolicy::OBJECT:
        return shell->GetAllowPlugins(shouldLoad);
      case nsIContentPolicy::SCRIPT:
        return shell->GetAllowJavascript(shouldLoad);
      case nsIContentPolicy::SUBDOCUMENT:
        return shell->GetAllowSubframes(shouldLoad);
#if 0       
        /* need to actually check that it's a meta tag, maybe */
      case nsIContentPolicy::CONTROL_TAG:
        return shell->GetAllowMetaRedirects(shouldLoad); /* meta _refresh_ */
#endif
      case nsIContentPolicy::IMAGE:
        return shell->GetAllowImages(shouldLoad);
      default:
        return NS_OK;
    }
    
}

NS_IMETHODIMP
nsWebBrowserContentPolicy::ShouldProcess(PRInt32 contentType,
                                         nsIURI *contentLocation,
                                         nsISupports *context,
                                         nsIDOMWindow *window,
                                         PRBool *shouldProcess)
{
    /* XXX use this for AllowJavascript (sic) control? */
    *shouldProcess = PR_TRUE;
    return NS_OK;
}

