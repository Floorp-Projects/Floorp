/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * Contributor(s):
 *   Scott MacGregor <scott@scott-macgregor.org>
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsMsgContentPolicy.h"
#include "nsIScriptGlobalObject.h"
#include "nsIDocShell.h"
#include "nsCOMPtr.h"
#include "nsIDOMWindow.h"
#include "nsIURI.h"

nsMsgContentPolicy::nsMsgContentPolicy()
{
}

nsMsgContentPolicy::~nsMsgContentPolicy()
{
}

NS_IMPL_ISUPPORTS1(nsMsgContentPolicy, nsIContentPolicy)

NS_IMETHODIMP
nsMsgContentPolicy::ShouldLoad(PRInt32 aContentType, nsIURI *aContentLocation, nsISupports *aContext,
                               nsIDOMWindow *aWindow, PRBool *shouldLoad)
{
  nsresult rv = NS_OK;
  *shouldLoad = PR_TRUE;

  if (!aContentLocation || !aContext) 
      return rv;

  if (aContentType == nsIContentPolicy::OBJECT)
  {
      // currently, mozilla thunderbird does not allow plugins at all. If someone later writes an extension to enable plugins,
      // we can read a pref here to figure out if we should allow the plugin to load or not.
  }
  else if (aContentType == nsIContentPolicy::IMAGE)
  {
    PRBool isFtp = PR_FALSE;
    rv = aContentLocation->SchemeIs("ftp", &isFtp);

    if (isFtp) 
    {
      // never allow ftp for mail messages, 
      // because we don't want to send the users email address
      // as the anonymous password
      *shouldLoad = PR_FALSE;
    }
    else
    {
      PRBool needToCheck = PR_FALSE;
      rv = aContentLocation->SchemeIs("http", &needToCheck);
      NS_ENSURE_SUCCESS(rv,rv);

      if (!needToCheck) {
        rv = aContentLocation->SchemeIs("https", &needToCheck);
        NS_ENSURE_SUCCESS(rv,rv);
      }

      if (needToCheck)
      {
        // check the 'disable remote images pref' and block the image if appropriate
        *shouldLoad = PR_FALSE;

        
        // optionally, if the message is junk, and the user does not want images to load for messages that are junk, then 
        // block the load.
      }
    }
  }

/*
    nsCOMPtr<nsIScriptGlobalObject> scriptGlobal = 
        do_QueryInterface(window);
    if (!scriptGlobal)
        return NS_OK;

    nsCOMPtr<nsIDocShell> shell;
    if (NS_FAILED(scriptGlobal->GetDocShell(getter_AddRefs(shell))))
        return NS_OK;
    
    switch (contentType) {
      case nsIContentPolicy::OBJECT:
        return shell->GetAllowPlugins(shouldLoad);
      case nsIContentPolicy::SCRIPT:
        return shell->GetAllowJavascript(shouldLoad);
      case nsIContentPolicy::SUBDOCUMENT:
        return shell->GetAllowSubframes(shouldLoad);
      case nsIContentPolicy::IMAGE:
        return shell->GetAllowImages(shouldLoad);
      default:
        return NS_OK;
    }
*/
  return rv;
}

NS_IMETHODIMP nsMsgContentPolicy::ShouldProcess(PRInt32 contentType,
                                                nsIURI *contentLocation,
                                                nsISupports *context,
                                                nsIDOMWindow *window,
                                                PRBool *shouldProcess)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

