/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
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

#include "nsPopupWindowManager.h"

#include "nsCRT.h"
#include "nsIServiceManager.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsIPrefBranch2.h"

/**
 * The Popup Window Manager maintains popup window permissions by website.
 */

static const char kPopupDisablePref[] = "dom.disable_open_during_load";

//*****************************************************************************
//*** nsPopupWindowManager object management and nsISupports
//*****************************************************************************

nsPopupWindowManager::nsPopupWindowManager() :
  mPolicy(ALLOW_POPUP)
{
}

nsPopupWindowManager::~nsPopupWindowManager()
{
}

NS_IMPL_ISUPPORTS3(nsPopupWindowManager, 
                   nsIPopupWindowManager,
                   nsIObserver,
                   nsSupportsWeakReference)

nsresult
nsPopupWindowManager::Init()
{
  nsresult rv;
  mPermissionManager = do_GetService(NS_PERMISSIONMANAGER_CONTRACTID);

  nsCOMPtr<nsIPrefBranch2> prefBranch =
    do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
  if (NS_SUCCEEDED(rv)) {
    bool permission;
    rv = prefBranch->GetBoolPref(kPopupDisablePref, &permission);
    if (NS_FAILED(rv)) {
      permission = PR_TRUE;
    }
    mPolicy = permission ? (PRUint32) DENY_POPUP : (PRUint32) ALLOW_POPUP;

    prefBranch->AddObserver(kPopupDisablePref, this, PR_TRUE);
  } 

  return NS_OK;
}

//*****************************************************************************
//*** nsPopupWindowManager::nsIPopupWindowManager
//*****************************************************************************

NS_IMETHODIMP
nsPopupWindowManager::TestPermission(nsIURI *aURI, PRUint32 *aPermission)
{
  NS_ENSURE_ARG_POINTER(aURI);
  NS_ENSURE_ARG_POINTER(aPermission);

  nsresult rv;
  PRUint32 permit;

  *aPermission = mPolicy;

  if (mPermissionManager) {
    rv = mPermissionManager->TestPermission(aURI, "popup", &permit);

    if (NS_SUCCEEDED(rv)) {
      // Share some constants between interfaces?
      if (permit == nsIPermissionManager::ALLOW_ACTION) {
        *aPermission = ALLOW_POPUP;
      } else if (permit == nsIPermissionManager::DENY_ACTION) {
        *aPermission = DENY_POPUP;
      }
    }
  }

  return NS_OK;
}

//*****************************************************************************
//*** nsPopupWindowManager::nsIObserver
//*****************************************************************************
NS_IMETHODIMP
nsPopupWindowManager::Observe(nsISupports *aSubject, 
                              const char *aTopic,
                              const PRUnichar *aData)
{
  nsCOMPtr<nsIPrefBranch> prefBranch = do_QueryInterface(aSubject);
  NS_ASSERTION(!nsCRT::strcmp(NS_PREFBRANCH_PREFCHANGE_TOPIC_ID, aTopic),
               "unexpected topic - we only deal with pref changes!");

  if (prefBranch) {
    // refresh our local copy of the "disable popups" pref
    bool permission = true;
    prefBranch->GetBoolPref(kPopupDisablePref, &permission);

    mPolicy = permission ? (PRUint32) DENY_POPUP : (PRUint32) ALLOW_POPUP;
  }

  return NS_OK;
}
