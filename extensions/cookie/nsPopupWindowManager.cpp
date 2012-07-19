/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPopupWindowManager.h"

#include "nsCRT.h"
#include "nsIServiceManager.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsIPrincipal.h"
#include "nsIURI.h"

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

  nsCOMPtr<nsIPrefBranch> prefBranch =
    do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
  if (NS_SUCCEEDED(rv)) {
    bool permission;
    rv = prefBranch->GetBoolPref(kPopupDisablePref, &permission);
    if (NS_FAILED(rv)) {
      permission = true;
    }
    mPolicy = permission ? (PRUint32) DENY_POPUP : (PRUint32) ALLOW_POPUP;

    prefBranch->AddObserver(kPopupDisablePref, this, true);
  } 

  return NS_OK;
}

//*****************************************************************************
//*** nsPopupWindowManager::nsIPopupWindowManager
//*****************************************************************************

NS_IMETHODIMP
nsPopupWindowManager::TestPermission(nsIPrincipal* aPrincipal,
                                     PRUint32 *aPermission)
{
  NS_ENSURE_ARG_POINTER(aPrincipal);
  NS_ENSURE_ARG_POINTER(aPermission);

  PRUint32 permit;
  *aPermission = mPolicy;

  if (mPermissionManager) {
    if (NS_SUCCEEDED(mPermissionManager->TestPermissionFromPrincipal(aPrincipal, "popup", &permit))) {
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
