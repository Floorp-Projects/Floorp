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
#include "nsPermission.h"
#include "nsCRT.h"
#include "nsIGenericFactory.h"
#include "nsXPIDLString.h"
#include "nsIScriptGlobalObject.h"
#include "nsIDOMWindowInternal.h"
#include "nsIPrompt.h"
#include "nsNetUtil.h"
#include "nsILineInputStream.h"
#include "nsIPrefBranch.h"
#include "nsIPrefBranchInternal.h"
#include "nsIPrefService.h"
#include "nsAppDirectoryServiceDefs.h"
#include "prprf.h"

static NS_NAMED_LITERAL_CSTRING(kPermissionsFileName, "cookperm.txt");
static const char kPermissionChangeNotification[] = PERM_CHANGE_NOTIFICATION;

typedef struct {
  nsCString     host;
  nsVoidArray   permissionList;
} permission_HostStruct;

typedef struct {
  PRUint32  type;
  PRUint32  permission;
} permission_TypeStruct;

////////////////////////////////////////////////////////////////////////////////

class nsPermissionEnumerator : public nsISimpleEnumerator
{
  public:
    NS_DECL_ISUPPORTS
    
    nsPermissionEnumerator(const nsVoidArray &aPermissionList)
      : mHostCount(aPermissionList.Count()),
        mHostIndex(0),
        mTypeIndex(0),
        mPermissionList(aPermissionList)
    {
    }
    
    NS_IMETHOD GetNext(nsISupports **result);

    NS_IMETHOD HasMoreElements(PRBool *aResult) 
    {
      *aResult = mHostCount > mHostIndex;
      return NS_OK;
    }

    virtual ~nsPermissionEnumerator() 
    {
    }

  protected:
    PRInt32 mHostCount;
    PRInt32 mHostIndex;
    PRInt32 mTypeIndex;
    const nsVoidArray &mPermissionList;
};

NS_IMPL_ISUPPORTS1(nsPermissionEnumerator, nsISimpleEnumerator);

NS_IMETHODIMP
nsPermissionEnumerator::GetNext(nsISupports **aResult) 
{
  if (mHostIndex >= mHostCount) {
    *aResult = nsnull;
    return NS_ERROR_FAILURE;
  }

  permission_HostStruct *hostStruct = NS_STATIC_CAST(permission_HostStruct*, mPermissionList.ElementAt(mHostIndex));
  NS_ASSERTION(hostStruct, "corrupt permission list");

  permission_TypeStruct *typeStruct = NS_STATIC_CAST(permission_TypeStruct*, hostStruct->permissionList.ElementAt(mTypeIndex++));
  if (mTypeIndex == hostStruct->permissionList.Count()) {
    mTypeIndex = 0;
    mHostIndex++;
  }

  nsIPermission *permission =
      new nsPermission(hostStruct->host, typeStruct->type, typeStruct->permission);
  if (!permission) {
    *aResult = nsnull;
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aResult = permission;
  NS_ADDREF(permission);
  return NS_OK;
}


////////////////////////////////////////////////////////////////////////////////
// nsPermissionManager Implementation

NS_IMPL_ISUPPORTS3(nsPermissionManager, nsIPermissionManager, nsIObserver, nsISupportsWeakReference);

nsPermissionManager::nsPermissionManager()
{
}

nsPermissionManager::~nsPermissionManager()
{
  RemoveAllFromMemory();
}

nsresult nsPermissionManager::Init()
{
  nsresult rv;

  // Cache the permissions file
  rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR, getter_AddRefs(mPermissionsFile));
  if (NS_SUCCEEDED(rv)) {
    rv = mPermissionsFile->AppendNative(kPermissionsFileName);
  }

  // Ignore an error. That is not a problem. No cookperm.txt usually.
  Read();

  mObserverService = do_GetService("@mozilla.org/observer-service;1", &rv);
  if (NS_SUCCEEDED(rv)) {
    mObserverService->AddObserver(this, "profile-before-change", PR_TRUE);
    mObserverService->AddObserver(this, "profile-do-change", PR_TRUE);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsPermissionManager::Add(nsIURI *aURI,
                         PRUint32 aType,
                         PRUint32 aPermission)
{
  NS_ASSERTION(aURI, "could not get uri");
  nsresult rv;

  nsCAutoString hostPort;
  aURI->GetHostPort(hostPort);
  if (hostPort.IsEmpty()) {
    // Nothing to add
    return NS_OK;
  }

  rv = AddInternal(hostPort, aType, aPermission);
  if (NS_FAILED(rv)) return rv;

  // Notify permission manager dialog to update its display
  //
  // This used to be conditional, but now we use AddInternal 
  // for cases when no notification is needed
  NotifyObservers(hostPort);

  mChangedList = PR_TRUE;
  return Write();
}

//Only add to memory, don't save. That's up to the caller.
nsresult
nsPermissionManager::AddInternal(const nsACString &aHost,
                                 PRUint32 aType,
                                 PRUint32 aPermission)
{
  // find existing entry for host
  permission_HostStruct *hostStruct;
  PRBool hostFound = PR_FALSE;
  PRInt32 hostCount = mPermissionList.Count();
  PRInt32 hostIndex;
  for (hostIndex = 0; hostIndex < hostCount; ++hostIndex) {
    hostStruct = NS_STATIC_CAST(permission_HostStruct*, mPermissionList.ElementAt(hostIndex));
    NS_ASSERTION(hostStruct, "corrupt permission list");
    if (aHost.Equals(hostStruct->host)) {
      // host found in list
      hostFound = PR_TRUE;
      break;
    } else if (aHost < hostStruct->host) {
      // Need to insert new entry here for alphabetized list
      break;
    }
  }

  if (!hostFound) {
    // create a host structure for the host
    hostStruct = new permission_HostStruct;
    if (!hostStruct) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    
    hostStruct->host = aHost;

    // Insert host structure into the list
    if (hostIndex < hostCount) {
      mPermissionList.InsertElementAt(hostStruct, hostIndex);
    } else {
      mPermissionList.AppendElement(hostStruct);
    }
  }

  // See if host already has an entry for this type
  permission_TypeStruct *typeStruct;
  PRInt32 typeCount = hostStruct->permissionList.Count();
  for (PRInt32 typeIndex=0; typeIndex < typeCount; ++typeIndex) {
    typeStruct = NS_STATIC_CAST(permission_TypeStruct*, hostStruct->permissionList.ElementAt(typeIndex));
    NS_ASSERTION(typeStruct, "corrupt permission list");
    if (typeStruct->type == aType) {
      // Type found. Modify the corresponding permission
      typeStruct->permission = aPermission;
      return NS_OK;
    }
  }

  // Create a type structure and attach it to the host structure
  typeStruct = new permission_TypeStruct;
  typeStruct->type = aType;
  typeStruct->permission = aPermission;
  hostStruct->permissionList.AppendElement(typeStruct);

  return NS_OK;
}

NS_IMETHODIMP
nsPermissionManager::Remove(const nsACString &aHost,
                            PRUint32 aType)
{
  nsresult rv;

  // Find existing entry for host
  permission_HostStruct *hostStruct;

  PRInt32 hostCount = mPermissionList.Count();
  for (PRInt32 hostIndex = 0; hostIndex < hostCount; ++hostIndex) {
    hostStruct = NS_STATIC_CAST(permission_HostStruct*, mPermissionList.ElementAt(hostIndex));
    NS_ASSERTION(hostStruct, "corrupt permission list");

    if (aHost.Equals(hostStruct->host)) {
      // Host found in list, see if it has an entry for this type
      permission_TypeStruct *typeStruct;

      PRInt32 typeCount = hostStruct->permissionList.Count();
      for (PRInt32 typeIndex = 0; typeIndex < typeCount; ++typeIndex) {
        typeStruct = NS_STATIC_CAST(permission_TypeStruct*, hostStruct->permissionList.ElementAt(typeIndex));
        NS_ASSERTION(typeStruct, "corrupt permission list");
        if (typeStruct->type == aType) {
          delete typeStruct;
          hostStruct->permissionList.RemoveElementAt(typeIndex);
          --typeCount;

          // If no more types are present, remove the entry
          if (typeCount == 0) {
            mPermissionList.RemoveElementAt(hostIndex);
            delete hostStruct;
          }
          mChangedList = PR_TRUE;
          Write();

          // Notify Observers
          NotifyObservers(aHost);

          return NS_OK;
        }
      }

      break;
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsPermissionManager::RemoveAll()
{
  RemoveAllFromMemory();
  Write();
  return NS_OK;
}

NS_IMETHODIMP
nsPermissionManager::TestPermission(nsIURI *aURI,
                                    PRUint32 aType,
                                    PRUint32 *aPermission)
{
  NS_ASSERTION(aURI, "could not get uri");
  NS_ASSERTION(aPermission, "no permission pointer");

  // set the default
  *aPermission = nsIPermissionManager::UNKNOWN_ACTION;

  nsCAutoString hostPort;
  aURI->GetHostPort(hostPort);
  // Don't error on no host. Just return UNKNOWN_ACTION as permission.
  if (hostPort.IsEmpty()) {
    return NS_OK;
  }
  
  permission_HostStruct *hostStruct;
  permission_TypeStruct *typeStruct;

  // find host name within list
  PRInt32 hostCount = mPermissionList.Count();
  PRUint32 offset = 0;

  do {
    nsDependentCSubstring hostTail(hostPort, offset, hostPort.Length() - offset);
    for (PRInt32 i = 0; i < hostCount; ++i) {
      hostStruct = NS_STATIC_CAST(permission_HostStruct*, mPermissionList.ElementAt(i));
      NS_ASSERTION(hostStruct, "corrupt permission list");

      if (hostStruct->host.Equals(hostTail)) {
        // search for type in the permission list for this host
        PRInt32 typeCount = hostStruct->permissionList.Count();
        for (PRInt32 typeIndex = 0; typeIndex < typeCount; ++typeIndex) {
          typeStruct = NS_STATIC_CAST(permission_TypeStruct*, hostStruct->permissionList.ElementAt(typeIndex));
          NS_ASSERTION(typeStruct, "corrupt permission list");

          if (typeStruct->type == aType) {
            // type found.  Obtain the corresponding permission
            *aPermission = typeStruct->permission;
            return NS_OK;
          }
        }
      }
    }
    offset = hostPort.FindChar('.', offset) + 1;

  // walk up the domaintree (we stop as soon as we find a match,
  // which will be the most specific domain we have an entry for).
  // this is new behavior; we only used to do this for popups.
  // see bug 176950.
  } while (offset > 0);

  return NS_OK;
}

NS_IMETHODIMP nsPermissionManager::GetEnumerator(nsISimpleEnumerator **aEnum)
{

  nsPermissionEnumerator* permissionEnum = new nsPermissionEnumerator(mPermissionList);
  if (!permissionEnum) {
    *aEnum = nsnull;
    return NS_ERROR_OUT_OF_MEMORY;
  }
  NS_ADDREF(permissionEnum);
  *aEnum = permissionEnum;
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

    RemoveAllFromMemory();
    if (!nsCRT::strcmp(someData, NS_LITERAL_STRING("shutdown-cleanse").get()))
      if (mPermissionsFile) {
        mPermissionsFile->Remove(PR_FALSE);
      }
  }  
  else if (!nsCRT::strcmp(aTopic, "profile-do-change")) {
    // The profile has aleady changed.    
    // Now just read them from the new profile location.

    // Re-get the permissions file
    rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR, getter_AddRefs(mPermissionsFile));
    if (NS_SUCCEEDED(rv)) {
      rv = mPermissionsFile->AppendNative(kPermissionsFileName);
    }
    Read();
  }

  return rv;
}

//*****************************************************************************
//*** nsPermissionManager private methods
//*****************************************************************************

nsresult
nsPermissionManager::RemoveAllFromMemory()
{
  permission_HostStruct *hostStruct;
  permission_TypeStruct *typeStruct;

  PRInt32 hostCount = mPermissionList.Count();
  for (PRInt32 hostIndex = hostCount-1; hostIndex >= 0; --hostIndex) {
    hostStruct = NS_STATIC_CAST(permission_HostStruct*, mPermissionList.ElementAt(hostIndex));
    NS_ASSERTION(hostStruct, "corrupt permission list");

    PRInt32 typeCount = hostStruct->permissionList.Count();
    for (PRInt32 typeIndex = typeCount-1; typeIndex >= 0; --typeIndex) {
      typeStruct = NS_STATIC_CAST(permission_TypeStruct*, hostStruct->permissionList.ElementAt(typeIndex));
      delete typeStruct;
      hostStruct->permissionList.RemoveElementAt(typeIndex);
    }
    // no more types are present, remove the entry
    mPermissionList.RemoveElementAt(hostIndex);
    delete hostStruct;
  }

  return NS_OK;
}

// broadcast a notification that a permission pref has changed
nsresult
nsPermissionManager::NotifyObservers(const nsACString &aHost)
{
  if (mObserverService) {
    return mObserverService->NotifyObservers(NS_STATIC_CAST(nsIPermissionManager *, this),
                                             kPermissionChangeNotification,
                                             NS_ConvertUTF8toUCS2(aHost).get());
  }
  return NS_ERROR_FAILURE;
}

// Note:
// We don't do checkbox states here anymore.
// When a consumer wants it back, that is up to the consumer, not this backend
// For cookies, it is now done with a persist in the dialog xul file.

nsresult
nsPermissionManager::Read()
{
  nsresult rv;
  
  if (!mPermissionsFile) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIInputStream> fileInputStream;
  rv = NS_NewLocalFileInputStream(getter_AddRefs(fileInputStream), mPermissionsFile);
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsILineInputStream> lineInputStream = do_QueryInterface(fileInputStream, &rv);
  if (NS_FAILED(rv)) return rv;

  /* format is:
   * host \t number permission \t number permission ... \n
   * if this format isn't respected we move onto the next line in the file.
   */

  nsAutoString bufferUnicode;
  nsCAutoString buffer;
  PRBool isMore = PR_TRUE;
  while (isMore && NS_SUCCEEDED(lineInputStream->ReadLine(bufferUnicode, &isMore))) {
    CopyUCS2toASCII(bufferUnicode, buffer);
    if (buffer.IsEmpty() || buffer.First() == '#') {
      continue;
    }

    PRInt32 hostIndex, permissionIndex;
    PRUint32 nextPermissionIndex = 0;
    hostIndex = 0;

    if ((permissionIndex = buffer.FindChar('\t', hostIndex) + 1) == 0) {
      continue;      
    }

    // ignore leading periods in host name
    while (hostIndex < permissionIndex && (buffer.CharAt(hostIndex) == '.')) {
      ++hostIndex;
    }

    nsDependentCSubstring host(buffer, hostIndex, permissionIndex - hostIndex - 1);

    for (;;) {
      if (nextPermissionIndex == buffer.Length()+1) {
        break;
      }
      if ((nextPermissionIndex = buffer.FindChar('\t', permissionIndex)+1) == 0) {
        nextPermissionIndex = buffer.Length()+1;
      }
      const nsASingleFragmentCString &permissionString = Substring(buffer, permissionIndex, nextPermissionIndex - permissionIndex - 1);
      permissionIndex = nextPermissionIndex;

      PRUint32 type = 0;
      PRUint32 index = 0;

      if (permissionString.IsEmpty()) {
        continue; // empty permission entry -- should never happen
      }
      // Parse "2T"
      char c = permissionString.CharAt(index);
      while (index < permissionString.Length() && c >= '0' && c <= '9') {
        type = 10*type + (c-'0');
        c = permissionString.CharAt(++index);
      }
      if (index >= permissionString.Length()) {
        continue; // bad format for this permission entry
      }
      PRUint32 permission = (permissionString.CharAt(index) == 'T') ? nsIPermissionManager::ALLOW_ACTION : nsIPermissionManager::DENY_ACTION;

      // Ignore @@@ as host. Old style checkbox status
      if (!permissionString.IsEmpty() && !host.Equals(NS_LITERAL_CSTRING("@@@@"))) {
        rv = AddInternal(host, type, permission);
        if (NS_FAILED(rv)) return rv;
      }

    }
  }

  mChangedList = PR_FALSE;

  return NS_OK;
}

nsresult
nsPermissionManager::Write()
{
  nsresult rv;

  if (!mChangedList) {
    return NS_OK;
  }

  if (!mPermissionsFile) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIOutputStream> fileOutputStream;
  rv = NS_NewLocalFileOutputStream(getter_AddRefs(fileOutputStream), mPermissionsFile);
  if (NS_FAILED(rv)) return rv;

  // get a buffered output stream 4096 bytes big, to optimize writes
  nsCOMPtr<nsIOutputStream> bufferedOutputStream;
  rv = NS_NewBufferedOutputStream(getter_AddRefs(bufferedOutputStream), fileOutputStream, 4096);
  if (NS_FAILED(rv)) return rv;

  permission_HostStruct *hostStruct;
  permission_TypeStruct *typeStruct;

  static const char kHeader[] = 
    "# HTTP Permission File\n"
    "# http://www.netscape.com/newsref/std/cookie_spec.html\n"
    "# This is a generated file!  Do not edit.\n\n";

  bufferedOutputStream->Write(kHeader, sizeof(kHeader) - 1, &rv);

  /* format shall be:
   * host \t permission \t permission ... \n
   */
  static const char kTab[] = "\t";
  static const char kNew[] = "\n";
  static const char kTrue[] = "T";
  static const char kFalse[] = "F";

  PRInt32 hostCount = mPermissionList.Count();
  for (PRInt32 i = 0; i < hostCount; ++i) {
    hostStruct = NS_STATIC_CAST(permission_HostStruct*, mPermissionList.ElementAt(i));
    NS_ASSERTION(hostStruct, "corrupt permission list");

    bufferedOutputStream->Write(hostStruct->host.get(), hostStruct->host.Length(), &rv);

    PRUint32 typeCount = hostStruct->permissionList.Count();
    for (PRUint32 typeIndex = 0; typeIndex < typeCount; ++typeIndex) {
      typeStruct = NS_STATIC_CAST(permission_TypeStruct*, hostStruct->permissionList.ElementAt(typeIndex));
      NS_ASSERTION(typeStruct, "corrupt permission list");

      bufferedOutputStream->Write(kTab, sizeof(kTab) - 1, &rv);

      char typeString[5];
      PRUint32 len = PR_snprintf(typeString, sizeof(typeString), "%u", typeStruct->type);
      bufferedOutputStream->Write(typeString, len, &rv);

      if (typeStruct->permission == nsIPermissionManager::ALLOW_ACTION) {
        bufferedOutputStream->Write(kTrue, sizeof(kTrue) - 1, &rv);
      } else if (typeStruct->permission == nsIPermissionManager::DENY_ACTION) {
        bufferedOutputStream->Write(kFalse, sizeof(kFalse) - 1, &rv);
      }
    }
    bufferedOutputStream->Write(kNew, sizeof(kNew) - 1, &rv);
  }

  mChangedList = PR_FALSE;

  return NS_OK;
}

