/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
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
 * The Original Code is Indexed Database.
 *
 * The Initial Developer of the Original Code is
 * The Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ben Turner <bent.mozilla@gmail.com>
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

#include "CheckPermissionsHelper.h"

#include "nsIDOMWindow.h"
#include "nsIObserverService.h"
#include "nsIPermissionManager.h"
#include "nsIPrincipal.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsIURI.h"

#include "nsContentUtils.h"
#include "nsDOMStorage.h"
#include "nsNetUtil.h"
#include "nsThreadUtils.h"
#include "mozilla/Services.h"
#include "mozilla/Preferences.h"

#include "IndexedDatabaseManager.h"

#define PERMISSION_INDEXEDDB "indexedDB"
#define PREF_INDEXEDDB_ENABLED "dom.indexedDB.enabled"
#define TOPIC_PERMISSIONS_PROMPT "indexedDB-permissions-prompt"
#define TOPIC_PERMISSIONS_RESPONSE "indexedDB-permissions-response"

using namespace mozilla;
USING_INDEXEDDB_NAMESPACE
using namespace mozilla::services;

namespace {

inline
PRUint32
GetIndexedDBPermissions(const nsACString& aASCIIOrigin,
                        nsIDOMWindow* aWindow)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (!Preferences::GetBool(PREF_INDEXEDDB_ENABLED)) {
    return nsIPermissionManager::DENY_ACTION;
  }

  nsCOMPtr<nsIScriptObjectPrincipal> sop(do_QueryInterface(aWindow));
  NS_ENSURE_TRUE(sop, nsIPermissionManager::DENY_ACTION);

  if (nsContentUtils::IsSystemPrincipal(sop->GetPrincipal())) {
    return nsIPermissionManager::ALLOW_ACTION;
  }

  if (nsDOMStorageManager::gStorageManager->InPrivateBrowsingMode()) {
    // TODO Support private browsing indexedDB?
    return nsIPermissionManager::DENY_ACTION;
  }

  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), aASCIIOrigin);
  NS_ENSURE_SUCCESS(rv, nsIPermissionManager::DENY_ACTION);

  nsCOMPtr<nsIPermissionManager> permissionManager =
    do_GetService(NS_PERMISSIONMANAGER_CONTRACTID);
  NS_ENSURE_TRUE(permissionManager, nsIPermissionManager::DENY_ACTION);

  PRUint32 permission;
  rv = permissionManager->TestPermission(uri, PERMISSION_INDEXEDDB,
                                         &permission);
  NS_ENSURE_SUCCESS(rv, nsIPermissionManager::DENY_ACTION);

  return permission;
}

} // anonymous namespace

NS_IMPL_THREADSAFE_ISUPPORTS3(CheckPermissionsHelper, nsIRunnable,
                                                      nsIInterfaceRequestor,
                                                      nsIObserver)

NS_IMETHODIMP
CheckPermissionsHelper::Run()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  PRUint32 permission = mHasPrompted ?
                        mPromptResult :
                        GetIndexedDBPermissions(mASCIIOrigin, mWindow);

  nsresult rv;
  if (mHasPrompted) {
    // Add permissions to the database, but only if we are in the parent
    // process (if we are in the child process, we have already
    // set the permission when the prompt was shown in the parent, as
    // we cannot set the permission from the child).
    if (permission != nsIPermissionManager::UNKNOWN_ACTION &&
        XRE_GetProcessType() == GeckoProcessType_Default) {
      nsCOMPtr<nsIURI> uri;
      rv = NS_NewURI(getter_AddRefs(uri), mASCIIOrigin);
      NS_ENSURE_SUCCESS(rv, rv);
  
      nsCOMPtr<nsIPermissionManager> permissionManager =
        do_GetService(NS_PERMISSIONMANAGER_CONTRACTID);
      NS_ENSURE_STATE(permissionManager);
  
      rv = permissionManager->Add(uri, PERMISSION_INDEXEDDB, permission,
                                  nsIPermissionManager::EXPIRE_NEVER, 0);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }
  else if (permission == nsIPermissionManager::UNKNOWN_ACTION &&
           mPromptAllowed) {
    nsCOMPtr<nsIObserverService> obs = GetObserverService();
    rv = obs->NotifyObservers(static_cast<nsIRunnable*>(this),
                              TOPIC_PERMISSIONS_PROMPT, nsnull);
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
  }

  nsRefPtr<OpenDatabaseHelper> helper;
  helper.swap(mHelper);

  nsCOMPtr<nsIDOMWindow> window;
  window.swap(mWindow);

  if (permission == nsIPermissionManager::ALLOW_ACTION) {
    IndexedDatabaseManager* mgr = IndexedDatabaseManager::Get();
    NS_ASSERTION(mgr, "This should never be null!");

    return helper->Dispatch(mgr->IOThread());
  }

  NS_ASSERTION(permission == nsIPermissionManager::UNKNOWN_ACTION ||
               permission == nsIPermissionManager::DENY_ACTION,
               "Unknown permission!");

  helper->SetError(NS_ERROR_DOM_INDEXEDDB_NOT_ALLOWED_ERR);

  return helper->RunImmediately();
}

NS_IMETHODIMP
CheckPermissionsHelper::GetInterface(const nsIID& aIID,
                                     void** aResult)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  if (aIID.Equals(NS_GET_IID(nsIObserver))) {
    return QueryInterface(aIID, aResult);
  }

  if (aIID.Equals(NS_GET_IID(nsIDOMWindow))) {
    return mWindow->QueryInterface(aIID, aResult);
  }

  *aResult = nsnull;
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
CheckPermissionsHelper::Observe(nsISupports* aSubject,
                                const char* aTopic,
                                const PRUnichar* aData)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(!strcmp(aTopic, TOPIC_PERMISSIONS_RESPONSE), "Bad topic!");
  NS_ASSERTION(mPromptAllowed, "How did we get here?");

  mHasPrompted = true;

  nsresult rv;
  mPromptResult = nsDependentString(aData).ToInteger(&rv);
  NS_ENSURE_SUCCESS(rv, rv);

  IndexedDatabaseManager* mgr = IndexedDatabaseManager::Get();
  NS_ASSERTION(mgr, "This should never be null!");

  rv = NS_DispatchToCurrentThread(this);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}
