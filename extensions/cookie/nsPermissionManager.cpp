/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

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
           do_GetService(NS_OBSERVERSERVICE_CONTRACTID, &rv);
  if (observerService) {
    observerService->AddObserver(this, NS_LITERAL_STRING("profile-before-change").get());
    observerService->AddObserver(this, NS_LITERAL_STRING("profile-do-change").get());
  }
  
  return NS_OK;
}

NS_IMETHODIMP nsPermissionManager::Add
    (const char * objectURL, PRBool permission, PRInt32 type) {
  ::PERMISSION_Add(objectURL, permission, type);
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

NS_IMETHODIMP nsPermissionManager::Observe(nsISupports *aSubject, const PRUnichar *aTopic, const PRUnichar *someData)
{
  nsresult rv = NS_OK;

  if (!nsCRT::strcmp(aTopic, NS_LITERAL_STRING("profile-before-change").get())) {
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
  else if (!nsCRT::strcmp(aTopic, NS_LITERAL_STRING("profile-do-change").get())) {
    // The profile has aleady changed.    
    // Now just read them from the new profile location.
    PERMISSION_Read();
  }

  return rv;
}
