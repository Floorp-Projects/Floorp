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
#include "nsIPermissionManager.h"
#include "nsIServiceManagerUtils.h"
#include "nsIURI.h"

/*
 The Popup Window Manager maintains popup window permissions by website.
 */

#define POLICYSTRING "policy"
#define CUSTOMSTRING "usecustom"

static const char sPopupPrefRoot[] = "privacy.popups.";
static const char sPopupPrefPolicyLeaf[] = POLICYSTRING;
static const char sPopupPrefCustomLeaf[] = CUSTOMSTRING;
static const char sPermissionChangeNotification[] = PPM_CHANGE_NOTIFICATION;
static const char sXPCOMShutdownTopic[] = NS_XPCOM_SHUTDOWN_OBSERVER_ID;
static const char sPrefChangedTopic[] = NS_PREFBRANCH_PREFCHANGE_TOPIC_ID;

//*****************************************************************************
//*** nsPopupWindowManager object management and nsISupports
//*****************************************************************************

nsPopupWindowManager::nsPopupWindowManager() :
  mPolicy(eAllow),
  mCustomPermissions(PR_FALSE)
{
  NS_INIT_ISUPPORTS();
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
  mPermManager = do_GetService(NS_PERMISSIONMANAGER_CONTRACTID);
  nsCOMPtr<nsIPrefService> prefs(do_GetService(NS_PREFSERVICE_CONTRACTID));
  if (prefs)
    prefs->GetBranch(sPopupPrefRoot, getter_AddRefs(mPopupPrefBranch));

  if (mOS && mPermManager && mPopupPrefBranch) {
     // initialize our local copy of the pref
    Observe(NS_STATIC_CAST(nsIPopupWindowManager *, this),
            sPrefChangedTopic, NS_LITERAL_STRING(POLICYSTRING).get());
    Observe(NS_STATIC_CAST(nsIPopupWindowManager *, this),
            sPrefChangedTopic, NS_LITERAL_STRING(CUSTOMSTRING).get());
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
  return eAllow;
}

NS_IMETHODIMP
nsPopupWindowManager::SetDefaultPermission(PRUint32 aDefaultPermission)
{
  NS_ASSERTION(aDefaultPermission == eAllow, "whitelist not supported");
  return aDefaultPermission == eAllow ? NS_OK : NS_ERROR_FAILURE;
}

/* Note: since we don't support whitelists, Add(uri, true) is the same thing
   as Remove(uri). However Add(uri, true) is not considered an error
   because the method is used that way. */
NS_IMETHODIMP
nsPopupWindowManager::Add(nsIURI *aURI, PRBool aPermit)
{
  NS_ENSURE_ARG_POINTER(aURI);
  if (!mPermManager)
    return NS_ERROR_UNEXPECTED;

  nsCAutoString uri;
  aURI->GetPrePath(uri);
  if (NS_SUCCEEDED(mPermManager->Add(uri, aPermit, WINDOWPERMISSION)))
    return NotifyObservers(aURI);
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsPopupWindowManager::Remove(nsIURI *aURI)
{
  NS_ENSURE_ARG_POINTER(aURI);
  if (!mPermManager)
    return NS_ERROR_UNEXPECTED;

  nsCAutoString uri;
  aURI->GetPrePath(uri);
  if (NS_SUCCEEDED(mPermManager->Add(uri, PR_TRUE, WINDOWPERMISSION)))
    return NotifyObservers(aURI);
  return NS_ERROR_FAILURE;
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

  if (mPolicy == eAllow && mCustomPermissions) {
    if (mPermManager) {
      /* Because of a bug/quirk/something in the PermissionManager
         the value of blockDomain will be left unchanged if the URI
         has no host port (like a local file URI, for instance).
         And the PermissionManager will refuse to save permissions
         for such URIs. A default of "false," then, allows local files
         to show popup windows, and this can't be stopped. ("true"
         would do just the opposite.) */
      PRBool blockDomain = PR_FALSE; // default to not blocked
      nsCAutoString uri;
      aURI->GetPrePath(uri);
      mPermManager->TestForBlocking(uri, WINDOWPERMISSION, &blockDomain);
      *_retval = blockDomain ? eDisallow : eAllowConditionally;
    }
  }
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
        (NS_LITERAL_STRING(POLICYSTRING).Equals(aData) ||
         NS_LITERAL_STRING(CUSTOMSTRING).Equals(aData))) {
    // refresh our local copy of the "allow popups" pref
    PRInt32 perm = eAllow;
    PRBool custom = PR_FALSE;

    if (mPopupPrefBranch) {
      mPopupPrefBranch->GetIntPref(sPopupPrefPolicyLeaf, &perm);
      mPopupPrefBranch->GetBoolPref(sPopupPrefCustomLeaf, &custom);
      NS_ASSERTION(perm >= 0, "popup pref value out of range");
    }
    mPolicy = NS_STATIC_CAST(PRUint32, perm);
    mCustomPermissions = custom;
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
      ibranch->AddObserver(sPopupPrefPolicyLeaf, this, PR_FALSE);
      rv = ibranch->AddObserver(sPopupPrefCustomLeaf, this, PR_FALSE);
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
    ibranch->RemoveObserver(sPopupPrefPolicyLeaf, this);
    ibranch->RemoveObserver(sPopupPrefCustomLeaf, this);
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

