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

#include "nsCOMPtr.h"
#include "nsCRT.h"
#include "nsPermissions.h"

#include "nsIObserverService.h"
#include "nsIPrefBranch.h"
#include "nsIPrefBranchInternal.h"
#include "nsIPrefService.h"
#include "nsIServiceManagerUtils.h"
#include "nsIURI.h"

/*
 The Popup Window Manager maintains popup window permissions by website.
 */

#define POPUP_PREF  "dom.disable_open_during_load"
static const char sPopupDisablePref[] = POPUP_PREF;
static const char sPermissionChangeNotification[] = PPM_CHANGE_NOTIFICATION;
static const char sXPCOMShutdownTopic[] = NS_XPCOM_SHUTDOWN_OBSERVER_ID;
static const char sPrefChangedTopic[] = NS_PREFBRANCH_PREFCHANGE_TOPIC_ID;

//*****************************************************************************
//*** nsPopupWindowManager object management and nsISupports
//*****************************************************************************

nsPopupWindowManager::nsPopupWindowManager() :
  mPolicy(ALLOW_POPUP)
{
}

nsPopupWindowManager::~nsPopupWindowManager(void)
{
  StopObservingThings();
}

NS_IMPL_ISUPPORTS2(nsPopupWindowManager, 
                   nsIPopupWindowManager,
                   nsIObserver);

nsresult
nsPopupWindowManager::Init()
{
  mOS = do_GetService("@mozilla.org/observer-service;1");
  // we bypass the permission manager API's but it still owns the underlying
  // list--we need to let it do the initializing to avoid conflict.
  mPermManager = do_GetService(NS_PERMISSIONMANAGER_CONTRACTID);
  nsCOMPtr<nsIPrefService> prefs(do_GetService(NS_PREFSERVICE_CONTRACTID));
  if (prefs)
    prefs->GetBranch("", getter_AddRefs(mPopupPrefBranch));

  if (mOS && mPermManager && mPopupPrefBranch) {
     // initialize our local copy of the pref
    Observe(NS_STATIC_CAST(nsIPopupWindowManager *, this),
            sPrefChangedTopic, NS_LITERAL_STRING(POPUP_PREF).get());
    return ObserveThings();
  }
  return NS_ERROR_FAILURE;
}

//*****************************************************************************
//*** nsPopupWindowManager::nsIPopupWindowManager
//*****************************************************************************

NS_IMETHODIMP
nsPopupWindowManager::GetDefaultPermission(PRUint32 *aDefaultPermission)
{
  NS_ENSURE_ARG_POINTER(aDefaultPermission);
  *aDefaultPermission = mPolicy;
  return NS_OK;
}

NS_IMETHODIMP
nsPopupWindowManager::SetDefaultPermission(PRUint32 aDefaultPermission)
{
  mPolicy = (aDefaultPermission == DENY_POPUP) ? DENY_POPUP : ALLOW_POPUP;
  return NS_OK;
}

NS_IMETHODIMP
nsPopupWindowManager::Add(nsIURI *aURI, PRBool aPermit)
{
  NS_ENSURE_ARG_POINTER(aURI);
  if (!mPermManager)
    // if we couldn't initialize the permission manager Permission_AddHost()
    // will create a new list that could stomp an existing cookperm.txt
    return NS_ERROR_FAILURE;

  nsCAutoString uri;
  aURI->GetHostPort(uri);
  if (uri.IsEmpty())
    return NS_ERROR_FAILURE;

  if (NS_SUCCEEDED(Permission_AddHost(uri, aPermit, WINDOWPERMISSION, PR_TRUE)))
    return NotifyObservers(aURI);
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsPopupWindowManager::Remove(nsIURI *aURI)
{
  NS_ENSURE_ARG_POINTER(aURI);

  nsCAutoString uri;
  aURI->GetHostPort(uri);
  if (uri.IsEmpty())
    return NS_ERROR_FAILURE;

  PERMISSION_Remove(uri, WINDOWPERMISSION);
  return NotifyObservers(aURI);
}

NS_IMETHODIMP
nsPopupWindowManager::RemoveAll()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* The underlying manager, nsPermissionManager, won't store permissions
   for an url lacking a host. It's good to know these things up front.
*/
NS_IMETHODIMP
nsPopupWindowManager::TestSuitability(nsIURI *aURI, PRBool *_retval)
{
  NS_ENSURE_ARG_POINTER(aURI);
  NS_ENSURE_ARG_POINTER(_retval);

  nsCAutoString hostPort;
  aURI->GetHostPort(hostPort);
  *_retval = hostPort.IsEmpty() ? PR_FALSE : PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP
nsPopupWindowManager::TestPermission(nsIURI *aURI, PRUint32 *_retval)
{
  NS_ENSURE_ARG_POINTER(aURI);
  NS_ENSURE_ARG_POINTER(_retval);

  *_retval = mPolicy;

  nsCAutoString uri;
  aURI->GetHostPort(uri);
  if (uri.IsEmpty())
    return NS_OK;

  /* Look for the specific host, if not found check its domains */
  nsresult rv;
  PRBool permission;
  PRInt32 offset = 0;
  const char* host = uri.get();
  do {
    rv = permission_CheckFromList(host+offset, permission, WINDOWPERMISSION);
    if (NS_SUCCEEDED(rv)) {
      /* found a value for the host/domain */
      *_retval = permission ? ALLOW_POPUP : DENY_POPUP;
      break;
    }

    /* try the parent domain */
    offset = uri.FindChar('.', offset) + 1;
  } while (offset > 0 );

  return NS_OK;
}

NS_IMETHODIMP
nsPopupWindowManager::GetEnumerator(nsISimpleEnumerator **_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = 0;

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsPopupWindowManager::AddObserver(nsIObserver *aObserver)
{
  if (mOS)
    return mOS->AddObserver(aObserver, sPermissionChangeNotification, PR_FALSE);
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsPopupWindowManager::RemoveObserver(nsIObserver *aObserver)
{
  if (mOS)
    return mOS->RemoveObserver(aObserver, sPermissionChangeNotification);
  return NS_ERROR_FAILURE;
}

//*****************************************************************************
//*** nsPopupWindowManager::nsIObserver
//*****************************************************************************
NS_IMETHODIMP
nsPopupWindowManager::Observe(nsISupports *aSubject, const char *aTopic,
                              const PRUnichar *aData)
{
  if (nsCRT::strcmp(aTopic, sPrefChangedTopic) == 0 &&
      NS_LITERAL_STRING(POPUP_PREF).Equals(aData)) {
    // refresh our local copy of the "disable popups" pref
    PRBool permission = PR_FALSE;

    if (mPopupPrefBranch) {
      mPopupPrefBranch->GetBoolPref(sPopupDisablePref, &permission);
    }
    mPolicy = permission ? DENY_POPUP : ALLOW_POPUP;
  } else if (nsCRT::strcmp(aTopic, sXPCOMShutdownTopic) == 0) {
    // unhook cyclical references
    StopObservingThings();
    DeInitialize();
  }
  return NS_OK;
}

//*****************************************************************************
//*** nsPopupWindowManager private methods
//*****************************************************************************

/* Register for notifications of interest. That's a change in the pref
   we're watching and XPCOM shutdown. */
nsresult
nsPopupWindowManager::ObserveThings()
{
  nsresult rv = NS_ERROR_FAILURE;

  if (mOS)
    rv = mOS->AddObserver(this, sXPCOMShutdownTopic, PR_FALSE);

  if (NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsIPrefBranchInternal> ibranch(do_QueryInterface(mPopupPrefBranch));
    if (ibranch) {
      ibranch->AddObserver(sPopupDisablePref, this, PR_FALSE);
    }
  }

  return rv;
}

// undo ObserveThings
nsresult
nsPopupWindowManager::StopObservingThings()
{
  nsCOMPtr<nsIPrefBranchInternal> ibranch(do_QueryInterface(mPopupPrefBranch));
  if (ibranch) {
    ibranch->RemoveObserver(sPopupDisablePref, this);
  }

  if (mOS)
    mOS->RemoveObserver(this, sXPCOMShutdownTopic);

  return NS_OK;
}

// broadcast a notification that a popup pref has changed
nsresult
nsPopupWindowManager::NotifyObservers(nsIURI *aURI)
{
  if (mOS) {
    nsCAutoString uri;
    aURI->GetSpec(uri);
    return mOS->NotifyObservers(NS_STATIC_CAST(nsIPopupWindowManager *, this),
                sPermissionChangeNotification, NS_ConvertUTF8toUCS2(uri).get());
  }
  return NS_ERROR_FAILURE;
}

// unhook cyclical references so we can be properly shut down
void
nsPopupWindowManager::DeInitialize()
{
  mOS = 0;
  mPermManager = 0;
  mPopupPrefBranch = 0;
}

