/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#include "nsIServiceManager.h"
#include "nsPermissionManager.h"
#include "nsCRT.h"
#include "nsPermissions.h"
#include "nsIGenericFactory.h"
#include "nsXPIDLString.h"
#include "nsIScriptGlobalObject.h"
#include "nsIDOMWindowInternal.h"
#include "nsIPrompt.h"
#include "nsIObserverService.h"
#include "nsPermission.h"
#include "nsNetCID.h"

static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);

////////////////////////////////////////////////////////////////////////////////

class nsPermissionEnumerator : public nsISimpleEnumerator
{
    public:

        NS_DECL_ISUPPORTS

        nsPermissionEnumerator() : mHostCount(0), mTypeCount(0)
        {
            NS_INIT_REFCNT();
        }

        NS_IMETHOD HasMoreElements(PRBool *result) 
        {
            *result = PERMISSION_HostCount() > mHostCount;
            return NS_OK;
        }

        NS_IMETHOD GetNext(nsISupports **result) 
        {
          char *host;
          PRBool capability;
          PRInt32 type;
          nsresult rv = PERMISSION_Enumerate
            (mHostCount, mTypeCount++, &host, &type, &capability);
          if (NS_SUCCEEDED(rv)) {
            if (mTypeCount == PERMISSION_TypeCount(mHostCount)) {
              mTypeCount = 0;
              mHostCount++;
            }
            nsIPermission *permission =
              new nsPermission(host, type, capability);
            *result = permission;
            NS_ADDREF(*result);
          } else {
            *result = nsnull;
          }
          return rv;
        }

        virtual ~nsPermissionEnumerator() 
        {
        }

    protected:
        PRInt32 mHostCount;
        PRInt32 mTypeCount;
};

NS_IMPL_ISUPPORTS1(nsPermissionEnumerator, nsISimpleEnumerator);

////////////////////////////////////////////////////////////////////////////////
// nsPermissionManager Implementation

NS_IMPL_ISUPPORTS3(nsPermissionManager, nsIPermissionManager, nsIObserver, nsISupportsWeakReference);

nsPermissionManager::nsPermissionManager()
{
  NS_INIT_REFCNT();
}

nsPermissionManager::~nsPermissionManager(void)
{
  PERMISSION_RemoveAll();    
}

nsresult nsPermissionManager::Init()
{
  PERMISSION_Read();

  nsresult rv;
  nsCOMPtr<nsIObserverService> observerService = 
           do_GetService("@mozilla.org/observer-service;1", &rv);
  if (observerService) {
    observerService->AddObserver(this, "profile-before-change", PR_FALSE);
    observerService->AddObserver(this, "profile-do-change", PR_FALSE);
  }
  mIOService = do_GetService(NS_IOSERVICE_CONTRACTID, &rv);
  return rv;
}

NS_IMETHODIMP nsPermissionManager::Add
    (const char * objectURL, PRBool permission, PRInt32 type) {
  ::PERMISSION_Add(objectURL, permission, type, mIOService);
  return NS_OK;
}

NS_IMETHODIMP nsPermissionManager::RemoveAll(void) {
  ::PERMISSION_RemoveAll();
  return NS_OK;
}

NS_IMETHODIMP nsPermissionManager::GetEnumerator(nsISimpleEnumerator * *entries)
{
    *entries = nsnull;

    nsPermissionEnumerator* permissionEnum = new nsPermissionEnumerator();
    if (permissionEnum == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(permissionEnum);
    *entries = permissionEnum;
    return NS_OK;
}

NS_IMETHODIMP nsPermissionManager::Remove(const char* host, PRInt32 type) {
  ::PERMISSION_Remove(host, type);
  return NS_OK;
}

NS_IMETHODIMP nsPermissionManager::Observe(nsISupports *aSubject, const char *aTopic, const PRUnichar *someData)
{
  nsresult rv = NS_OK;

  if (!nsCRT::strcmp(aTopic, "profile-before-change")) {
    // The profile is about to change.
    
    // Dump current permission.  This will be done by calling 
    // PERMISSION_RemoveAll which clears the memory-resident
    // permission table.  The reason the permission file does not
    // need to be updated is because the file was updated every time
    // the memory-resident table changed (i.e., whenever a new permission
    // was accepted).  If this condition ever changes, the permission
    // file would need to be updated here.

    PERMISSION_RemoveAll();
    if (!nsCRT::strcmp(someData, NS_LITERAL_STRING("shutdown-cleanse").get()))
      PERMISSION_DeletePersistentUserData();
  }  
  else if (!nsCRT::strcmp(aTopic, "profile-do-change")) {
    // The profile has aleady changed.    
    // Now just read them from the new profile location.
    PERMISSION_Read();
  }

  return rv;
}
