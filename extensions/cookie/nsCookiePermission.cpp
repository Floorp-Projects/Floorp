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
 * The Original Code is cookie manager code.
 *
 * The Initial Developer of the Original Code is
 * Michiel van Leeuwen (mvl@exedo.nl).
 * Portions created by the Initial Developer are Copyright (C) 2003
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


#include "nsCookiePermission.h"
#include "nsCookie.h"
#include "nsICookieAcceptDialog.h"
#include "nsIWindowWatcher.h"
#include "nsIServiceManager.h"
#include "nsString.h"
#include "nsIDialogParamBlock.h"
#include "nsArray.h"
#include "nsCookiePromptService.h"

/****************************************************************
 ************************ nsCookiePermission ********************
 ****************************************************************/

static const PRUint32 kDefaultPolicy = nsIPermissionManager::ALLOW_ACTION;

NS_IMPL_ISUPPORTS1(nsCookiePermission, nsICookiePermission);

nsCookiePermission::nsCookiePermission()
 : mPermissionManager(nsnull)
{
}

nsCookiePermission::~nsCookiePermission()
{
}

nsresult
nsCookiePermission::Init()
{
  // Continue on an error. We can do a few things without a permission manager
  mPermissionManager = do_GetService(NS_PERMISSIONMANAGER_CONTRACTID);

  return NS_OK;
}

NS_IMETHODIMP 
nsCookiePermission::TestPermission(nsIURI *aURI,
                                   nsICookie *aCookie,
                                   nsIDOMWindow *aParent,
                                   PRInt32 aCookiesFromHost,
                                   PRBool aChangingCookie,
                                   PRBool aShowDialog,
                                   PRBool *aPermission) 
{
  NS_ASSERTION(aURI, "could not get uri");

  nsresult rv;
  *aPermission = kDefaultPolicy;
  
  nsCAutoString hostPort;
  aURI->GetHostPort(hostPort);
  if (hostPort.IsEmpty())
    return NS_OK;

  PRUint32 listPermission = nsIPermissionManager::UNKNOWN_ACTION;
  if (mPermissionManager) {
    mPermissionManager->TestPermission(aURI, "cookie", &listPermission);
  }

  if (listPermission == nsIPermissionManager::UNKNOWN_ACTION) {
    if (aShowDialog && aCookie) {
      // we don't cache the cookiePromptService - it's not used often, so not worth the memory
      nsCOMPtr<nsICookiePromptService> cookiePromptService = do_GetService(NS_COOKIEPROMPTSERVICE_CONTRACTID, &rv);
      if (NS_FAILED(rv)) return rv;

      PRBool rememberDecision = PR_FALSE;

      rv = cookiePromptService->
             CookieDialog(nsnull, aCookie, hostPort, 
                          aCookiesFromHost, aChangingCookie, &rememberDecision, 
                          aPermission);
      if (NS_FAILED(rv)) return rv;

      if (rememberDecision && mPermissionManager) {
        // Remember this decision
        mPermissionManager->Add(aURI, "cookie", 
                                *aPermission ? nsIPermissionManager::ALLOW_ACTION : nsIPermissionManager::DENY_ACTION);
      }
    }
  } else {
    *aPermission = (listPermission == nsIPermissionManager::ALLOW_ACTION);
  }

  return NS_OK;
}
