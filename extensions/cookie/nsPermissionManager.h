/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Michiel van Leeuwen (mvl@exedo.nl)
 *   Daniel Witte (dwitte@stanford.edu)
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

#ifndef nsPermissionManager_h__
#define nsPermissionManager_h__

#include "nsIPermissionManager.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsWeakReference.h"
#include "nsCOMPtr.h"
#include "nsIFile.h"
#include "nsTHashtable.h"
#include "nsTArray.h"
#include "nsString.h"
#include "nsPermission.h"

class nsIPermission;
class nsIIDNService;
class mozIStorageConnection;
class mozIStorageStatement;

////////////////////////////////////////////////////////////////////////////////

class nsPermissionEntry
{
public:
  nsPermissionEntry(PRUint32 aType, PRUint32 aPermission, PRInt64 aID, 
                    PRUint32 aExpireType, PRInt64 aExpireTime)
   : mType(aType)
   , mPermission(aPermission)
   , mID(aID)
   , mExpireType(aExpireType)
   , mExpireTime(aExpireTime) {}

  PRUint32 mType;
  PRUint32 mPermission;
  PRInt64  mID;
  PRUint32 mExpireType;
  PRInt64  mExpireTime;
};

class nsHostEntry : public PLDHashEntryHdr
{
public:
  // Hash methods
  typedef const char* KeyType;
  typedef const char* KeyTypePointer;

  nsHostEntry(const char* aHost);
  nsHostEntry(const nsHostEntry& toCopy);

  ~nsHostEntry()
  {
  }

  KeyType GetKey() const
  {
    return mHost;
  }

  PRBool KeyEquals(KeyTypePointer aKey) const
  {
    return !strcmp(mHost, aKey);
  }

  static KeyTypePointer KeyToPointer(KeyType aKey)
  {
    return aKey;
  }

  static PLDHashNumber HashKey(KeyTypePointer aKey)
  {
    // PL_DHashStringKey doesn't use the table parameter, so we can safely
    // pass nsnull
    return PL_DHashStringKey(nsnull, aKey);
  }

  // force the hashtable to use the copy constructor when shuffling entries
  // around, otherwise the Auto part of our nsAutoTArray won't be happy!
  enum { ALLOW_MEMMOVE = PR_FALSE };

  // Permissions methods
  inline const nsDependentCString GetHost() const
  {
    return nsDependentCString(mHost);
  }

  inline nsTArray<nsPermissionEntry> & GetPermissions()
  {
    return mPermissions;
  }

  inline PRInt32 GetPermissionIndex(PRUint32 aType) const
  {
    for (PRUint32 i = 0; i < mPermissions.Length(); ++i)
      if (mPermissions[i].mType == aType)
        return i;

    return -1;
  }

  inline nsPermissionEntry GetPermission(PRUint32 aType) const
  {
    for (PRUint32 i = 0; i < mPermissions.Length(); ++i)
      if (mPermissions[i].mType == aType)
        return mPermissions[i];

    // unknown permission... return relevant data 
    nsPermissionEntry unk = nsPermissionEntry(aType, nsIPermissionManager::UNKNOWN_ACTION,
                                              -1, nsIPermissionManager::EXPIRE_NEVER, 0);
    return unk;
  }

private:
  const char *mHost;
  nsAutoTArray<nsPermissionEntry, 1> mPermissions;
};


class nsPermissionManager : public nsIPermissionManager,
                            public nsIObserver,
                            public nsSupportsWeakReference
{
public:

  // nsISupports
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPERMISSIONMANAGER
  NS_DECL_NSIOBSERVER

  nsPermissionManager();
  virtual ~nsPermissionManager();
  static nsIPermissionManager* GetXPCOMSingleton();
  static already_AddRefed<nsPermissionManager> GetSingleton();
  nsresult Init();

  // enums for AddInternal()
  enum OperationType {
    eOperationNone,
    eOperationAdding,
    eOperationRemoving,
    eOperationChanging
  };

  enum DBOperationType {
    eNoDBOperation,
    eWriteToDB
  };

  enum NotifyOperationType {
    eDontNotify,
    eNotify
  };

  nsresult AddInternal(const nsAFlatCString &aHost,
                       const nsAFlatCString &aType,
                       PRUint32 aPermission,
                       PRInt64 aID,
                       PRUint32 aExpireType,
                       PRInt64  aExpireTime,
                       NotifyOperationType aNotifyOperation,
                       DBOperationType aDBOperation);

private:

  PRInt32 GetTypeIndex(const char *aTypeString,
                       PRBool      aAdd);

  nsHostEntry *GetHostEntry(const nsAFlatCString &aHost,
                            PRUint32              aType,
                            PRBool                aExactHostMatch);

  nsresult CommonTestPermission(nsIURI     *aURI,
                                const char *aType,
                                PRUint32   *aPermission,
                                PRBool      aExactHostMatch);

  nsresult InitDB(PRBool aRemoveFile);
  nsresult CreateTable();
  nsresult Import();
  nsresult Read();
  void     NotifyObserversWithPermission(const nsACString &aHost,
                                         const nsCString  &aType,
                                         PRUint32          aPermission,
                                         PRUint32          aExpireType,
                                         PRInt64           aExpireTime,
                                         const PRUnichar  *aData);
  void     NotifyObservers(nsIPermission *aPermission, const PRUnichar *aData);
  nsresult RemoveAllInternal();
  nsresult RemoveAllFromMemory();
  nsresult NormalizeToACE(nsCString &aHost);
  nsresult GetHost(nsIURI *aURI, nsACString &aResult);
  static void UpdateDB(OperationType         aOp,
                       mozIStorageStatement* aStmt,
                       PRInt64               aID,
                       const nsACString     &aHost,
                       const nsACString     &aType,
                       PRUint32              aPermission,
                       PRUint32              aExpireType,
                       PRInt64               aExpireTime);

  nsCOMPtr<nsIObserverService> mObserverService;
  nsCOMPtr<nsIIDNService>      mIDNService;

  nsCOMPtr<mozIStorageConnection> mDBConn;
  nsCOMPtr<mozIStorageStatement> mStmtInsert;
  nsCOMPtr<mozIStorageStatement> mStmtDelete;
  nsCOMPtr<mozIStorageStatement> mStmtUpdate;

  nsTHashtable<nsHostEntry>    mHostTable;
  // a unique, monotonically increasing id used to identify each database entry
  PRInt64                      mLargestID;

  // An array to store the strings identifying the different types.
  nsTArray<nsCString>          mTypeArray;
};

// {4F6B5E00-0C36-11d5-A535-0010A401EB10}
#define NS_PERMISSIONMANAGER_CID \
{ 0x4f6b5e00, 0xc36, 0x11d5, { 0xa5, 0x35, 0x0, 0x10, 0xa4, 0x1, 0xeb, 0x10 } }

#endif /* nsPermissionManager_h__ */
