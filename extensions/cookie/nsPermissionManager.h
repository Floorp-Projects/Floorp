/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsPermissionManager_h__
#define nsPermissionManager_h__

#include "nsIPermissionManager.h"
#include "nsIObserver.h"
#include "nsWeakReference.h"
#include "nsCOMPtr.h"
#include "nsIInputStream.h"
#include "nsTHashtable.h"
#include "nsTArray.h"
#include "nsString.h"
#include "nsPermission.h"
#include "nsHashKeys.h"
#include "nsCOMArray.h"
#include "nsDataHashtable.h"
#include "nsIRunnable.h"
#include "nsRefPtrHashtable.h"
#include "mozilla/MozPromise.h"

namespace mozilla {
class OriginAttributesPattern;
}

class nsIPermission;
class mozIStorageConnection;
class mozIStorageAsyncStatement;

////////////////////////////////////////////////////////////////////////////////

class nsPermissionManager final : public nsIPermissionManager,
                                  public nsIObserver,
                                  public nsSupportsWeakReference
{
public:
  class PermissionEntry
  {
  public:
    PermissionEntry(int64_t aID, uint32_t aType, uint32_t aPermission,
                    uint32_t aExpireType, int64_t aExpireTime,
                    int64_t aModificationTime)
     : mID(aID)
     , mType(aType)
     , mPermission(aPermission)
     , mExpireType(aExpireType)
     , mExpireTime(aExpireTime)
     , mModificationTime(aModificationTime)
     , mNonSessionPermission(aPermission)
     , mNonSessionExpireType(aExpireType)
     , mNonSessionExpireTime(aExpireTime)
    {}

    int64_t  mID;
    uint32_t mType;
    uint32_t mPermission;
    uint32_t mExpireType;
    int64_t  mExpireTime;
    int64_t  mModificationTime;
    uint32_t mNonSessionPermission;
    uint32_t mNonSessionExpireType;
    uint32_t mNonSessionExpireTime;
  };

  /**
   * PermissionKey is the key used by PermissionHashKey hash table.
   *
   * NOTE: It could be implementing nsIHashable but there is no reason to worry
   * with XPCOM interfaces while we don't need to.
   */
  class PermissionKey
  {
  public:
    static PermissionKey* CreateFromPrincipal(nsIPrincipal* aPrincipal,
                                              nsresult& aResult);

    explicit PermissionKey(const nsACString& aOrigin)
      : mOrigin(aOrigin)
    {
    }

    bool operator==(const PermissionKey& aKey) const {
      return mOrigin.Equals(aKey.mOrigin);
    }

    PLDHashNumber GetHashCode() const {
      return mozilla::HashString(mOrigin);
    }

    NS_INLINE_DECL_THREADSAFE_REFCOUNTING(PermissionKey)

    nsCString mOrigin;

  private:
    // Default ctor shouldn't be used.
    PermissionKey() = delete;

    // Dtor shouldn't be used outside of the class.
    ~PermissionKey() {};
  };

  class PermissionHashKey : public nsRefPtrHashKey<PermissionKey>
  {
  public:
    explicit PermissionHashKey(const PermissionKey* aPermissionKey)
      : nsRefPtrHashKey<PermissionKey>(aPermissionKey)
    {}

    PermissionHashKey(const PermissionHashKey& toCopy)
      : nsRefPtrHashKey<PermissionKey>(toCopy)
      , mPermissions(toCopy.mPermissions)
    {}

    bool KeyEquals(const PermissionKey* aKey) const
    {
      return *aKey == *GetKey();
    }

    static PLDHashNumber HashKey(const PermissionKey* aKey)
    {
      return aKey->GetHashCode();
    }

    // Force the hashtable to use the copy constructor when shuffling entries
    // around, otherwise the Auto part of our AutoTArray won't be happy!
    enum { ALLOW_MEMMOVE = false };

    inline nsTArray<PermissionEntry> & GetPermissions()
    {
      return mPermissions;
    }

    inline int32_t GetPermissionIndex(uint32_t aType) const
    {
      for (uint32_t i = 0; i < mPermissions.Length(); ++i)
        if (mPermissions[i].mType == aType)
          return i;

      return -1;
    }

    inline PermissionEntry GetPermission(uint32_t aType) const
    {
      for (uint32_t i = 0; i < mPermissions.Length(); ++i)
        if (mPermissions[i].mType == aType)
          return mPermissions[i];

      // unknown permission... return relevant data
      return PermissionEntry(-1, aType, nsIPermissionManager::UNKNOWN_ACTION,
                             nsIPermissionManager::EXPIRE_NEVER, 0, 0);
    }

  private:
    AutoTArray<PermissionEntry, 1> mPermissions;
  };

  // nsISupports
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPERMISSIONMANAGER
  NS_DECL_NSIOBSERVER

  nsPermissionManager();
  static nsIPermissionManager* GetXPCOMSingleton();
  nsresult Init();

  // enums for AddInternal()
  enum OperationType {
    eOperationNone,
    eOperationAdding,
    eOperationRemoving,
    eOperationChanging,
    eOperationReplacingDefault
  };

  enum DBOperationType {
    eNoDBOperation,
    eWriteToDB
  };

  enum NotifyOperationType {
    eDontNotify,
    eNotify
  };

  // A special value for a permission ID that indicates the ID was loaded as
  // a default value.  These will never be written to the database, but may
  // be overridden with an explicit permission (including UNKNOWN_ACTION)
  static const int64_t cIDPermissionIsDefault = -1;

  nsresult AddInternal(nsIPrincipal* aPrincipal,
                       const nsAFlatCString &aType,
                       uint32_t aPermission,
                       int64_t aID,
                       uint32_t aExpireType,
                       int64_t  aExpireTime,
                       int64_t aModificationTime,
                       NotifyOperationType aNotifyOperation,
                       DBOperationType aDBOperation,
                       const bool aIgnoreSessionPermissions = false);

  /**
   * Initialize the "clear-origin-attributes-data" observing.
   * Will create a nsPermissionManager instance if needed.
   * That way, we can prevent have nsPermissionManager created at startup just
   * to be able to clear data when an application is uninstalled.
   */
  static void ClearOriginDataObserverInit();

  nsresult
  RemovePermissionsWithAttributes(mozilla::OriginAttributesPattern& aAttrs);

  /**
   * See `nsIPermissionManager::GetPermissionsWithKey` for more info on
   * permission keys.
   *
   * Get the permission key corresponding to the given Principal. This method is
   * intentionally infallible, as we want to provide an permission key to every
   * principal. Principals which don't have meaningful URIs with http://,
   * https://, or ftp:// schemes are given the default "" Permission Key.
   *
   * @param aPrincipal  The Principal which the key is to be extracted from.
   * @param aPermissionKey  A string which will be filled with the permission key.
   */
  static void GetKeyForPrincipal(nsIPrincipal* aPrincipal, nsACString& aPermissionKey);

  /**
   * See `nsIPermissionManager::GetPermissionsWithKey` for more info on
   * permission keys.
   *
   * Get the permission key corresponding to the given Principal and type. This
   * method is intentionally infallible, as we want to provide an permission key
   * to every principal. Principals which don't have meaningful URIs with
   * http://, https://, or ftp:// schemes are given the default "" Permission
   * Key.
   *
   * This method is different from GetKeyForPrincipal in that it also takes
   * permissions which must be sent down before loading a document into account.
   *
   * @param aPrincipal  The Principal which the key is to be extracted from.
   * @param aType  The type of the permission to get the key for.
   * @param aPermissionKey  A string which will be filled with the permission key.
   */
  static void GetKeyForPermission(nsIPrincipal* aPrincipal,
                                  const char* aType,
                                  nsACString& aPermissionKey);

  /**
   * See `nsIPermissionManager::GetPermissionsWithKey` for more info on
   * permission keys.
   *
   * Get all permissions keys which could correspond to the given principal.
   * This method, like GetKeyForPrincipal, is infallible and should always
   * produce at least one key.
   *
   * Unlike GetKeyForPrincipal, this method also gets the keys for base domains
   * of the given principal. All keys returned by this method must be avaliable
   * in the content process for a given URL to successfully have its permissions
   * checked in the `aExactHostMatch = false` situation.
   *
   * @param aPrincipal  The Principal which the key is to be extracted from.
   */
  static nsTArray<nsCString> GetAllKeysForPrincipal(nsIPrincipal* aPrincipal);

private:
  virtual ~nsPermissionManager();

  int32_t GetTypeIndex(const char *aTypeString,
                       bool        aAdd);

  PermissionHashKey* GetPermissionHashKey(nsIPrincipal* aPrincipal,
                                          uint32_t      aType,
                                          bool          aExactHostMatch);

  nsresult CommonTestPermission(nsIPrincipal* aPrincipal,
                                const char *aType,
                                uint32_t   *aPermission,
                                bool        aExactHostMatch,
                                bool        aIncludingSession);

  nsresult OpenDatabase(nsIFile* permissionsFile);
  nsresult InitDB(bool aRemoveFile);
  nsresult CreateTable();
  nsresult Import();
  nsresult ImportDefaults();
  nsresult _DoImport(nsIInputStream *inputStream, mozIStorageConnection *aConn);
  nsresult Read();
  void     NotifyObserversWithPermission(nsIPrincipal*     aPrincipal,
                                         const nsCString  &aType,
                                         uint32_t          aPermission,
                                         uint32_t          aExpireType,
                                         int64_t           aExpireTime,
                                         const char16_t  *aData);
  void     NotifyObservers(nsIPermission *aPermission, const char16_t *aData);

  // Finalize all statements, close the DB and null it.
  // if aRebuildOnSuccess, reinitialize database
  void     CloseDB(bool aRebuildOnSuccess = false);

  nsresult RemoveAllInternal(bool aNotifyObservers);
  nsresult RemoveAllFromMemory();
  static void UpdateDB(OperationType aOp,
                       mozIStorageAsyncStatement* aStmt,
                       int64_t aID,
                       const nsACString& aOrigin,
                       const nsACString& aType,
                       uint32_t aPermission,
                       uint32_t aExpireType,
                       int64_t aExpireTime,
                       int64_t aModificationTime);

  /**
   * This method removes all permissions modified after the specified time.
   */
  nsresult
  RemoveAllModifiedSince(int64_t aModificationTime);

  /**
   * Returns false if this permission manager wouldn't have the permission
   * requested avaliable.
   *
   * If aType is nullptr, checks that the permission manager would have all
   * permissions avaliable for the given principal.
   */
  bool PermissionAvaliable(nsIPrincipal* aPrincipal, const char* aType);

  nsRefPtrHashtable<nsCStringHashKey, mozilla::GenericPromise::Private> mPermissionKeyPromiseMap;

  nsCOMPtr<mozIStorageConnection> mDBConn;
  nsCOMPtr<mozIStorageAsyncStatement> mStmtInsert;
  nsCOMPtr<mozIStorageAsyncStatement> mStmtDelete;
  nsCOMPtr<mozIStorageAsyncStatement> mStmtUpdate;

  bool mMemoryOnlyDB;

  nsTHashtable<PermissionHashKey> mPermissionTable;
  // a unique, monotonically increasing id used to identify each database entry
  int64_t                      mLargestID;

  // An array to store the strings identifying the different types.
  nsTArray<nsCString>          mTypeArray;

  // Initially, |false|. Set to |true| once shutdown has started, to avoid
  // reopening the database.
  bool mIsShuttingDown;

  friend class DeleteFromMozHostListener;
  friend class CloseDatabaseListener;
};

// {4F6B5E00-0C36-11d5-A535-0010A401EB10}
#define NS_PERMISSIONMANAGER_CID \
{ 0x4f6b5e00, 0xc36, 0x11d5, { 0xa5, 0x35, 0x0, 0x10, 0xa4, 0x1, 0xeb, 0x10 } }

#endif /* nsPermissionManager_h__ */
