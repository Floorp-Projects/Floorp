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
    explicit PermissionKey(nsIPrincipal* aPrincipal);
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
   * Initialize the "clear-origin-data" observing.
   * Will create a nsPermissionManager instance if needed.
   * That way, we can prevent have nsPermissionManager created at startup just
   * to be able to clear data when an application is uninstalled.
   */
  static void ClearOriginDataObserverInit();

  nsresult
  RemovePermissionsWithAttributes(mozilla::OriginAttributesPattern& aAttrs);

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

  nsresult RemoveExpiredPermissionsForApp(uint32_t aAppId);

  /**
   * This method removes all permissions modified after the specified time.
   */
  nsresult
  RemoveAllModifiedSince(int64_t aModificationTime);

  /**
   * Retrieve permissions from chrome process.
   */
  nsresult
  FetchPermissions();

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

  // A list of struct for counting applications
  struct ApplicationCounter {
    uint32_t mAppId;
    uint32_t mCounter;
  };
  nsTArray<ApplicationCounter> mAppIdRefcounts;

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
