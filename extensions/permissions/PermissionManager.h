/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_PermissionManager_h
#define mozilla_PermissionManager_h

#include "nsIPermissionManager.h"
#include "nsIObserver.h"
#include "nsWeakReference.h"
#include "nsCOMPtr.h"
#include "nsTHashtable.h"
#include "nsTArray.h"
#include "nsString.h"
#include "nsHashKeys.h"
#include "nsRefPtrHashtable.h"
#include "mozilla/Atomics.h"
#include "mozilla/Monitor.h"
#include "mozilla/MozPromise.h"
#include "mozilla/ThreadBound.h"
#include "mozilla/Variant.h"
#include "mozilla/Vector.h"

#include <utility>

class mozIStorageConnection;
class mozIStorageStatement;
class nsIInputStream;
class nsIPermission;
class nsIPrefBranch;

namespace IPC {
struct Permission;
}

namespace mozilla {
class OriginAttributesPattern;

namespace dom {
class ContentChild;
}  // namespace dom

////////////////////////////////////////////////////////////////////////////////

class PermissionManager final : public nsIPermissionManager,
                                public nsIObserver,
                                public nsSupportsWeakReference {
  friend class dom::ContentChild;

 public:
  class PermissionEntry {
   public:
    PermissionEntry(int64_t aID, uint32_t aType, uint32_t aPermission,
                    uint32_t aExpireType, int64_t aExpireTime,
                    int64_t aModificationTime)
        : mID(aID),
          mExpireTime(aExpireTime),
          mModificationTime(aModificationTime),
          mType(aType),
          mPermission(aPermission),
          mExpireType(aExpireType),
          mNonSessionPermission(aPermission),
          mNonSessionExpireType(aExpireType),
          mNonSessionExpireTime(aExpireTime) {}

    int64_t mID;
    int64_t mExpireTime;
    int64_t mModificationTime;
    uint32_t mType;
    uint32_t mPermission;
    uint32_t mExpireType;
    uint32_t mNonSessionPermission;
    uint32_t mNonSessionExpireType;
    uint32_t mNonSessionExpireTime;
  };

  /**
   * PermissionKey is the key used by PermissionHashKey hash table.
   */
  class PermissionKey {
   public:
    static PermissionKey* CreateFromPrincipal(nsIPrincipal* aPrincipal,
                                              bool aForceStripOA,
                                              nsresult& aResult);
    static PermissionKey* CreateFromURI(nsIURI* aURI, nsresult& aResult);
    static PermissionKey* CreateFromURIAndOriginAttributes(
        nsIURI* aURI, const OriginAttributes* aOriginAttributes,
        bool aForceStripOA, nsresult& aResult);

    explicit PermissionKey(const nsACString& aOrigin)
        : mOrigin(aOrigin), mHashCode(HashString(aOrigin)) {}

    bool operator==(const PermissionKey& aKey) const {
      return mOrigin.Equals(aKey.mOrigin);
    }

    PLDHashNumber GetHashCode() const { return mHashCode; }

    NS_INLINE_DECL_THREADSAFE_REFCOUNTING(PermissionKey)

    const nsCString mOrigin;
    const PLDHashNumber mHashCode;

   private:
    // Default ctor shouldn't be used.
    PermissionKey() = delete;

    // Dtor shouldn't be used outside of the class.
    ~PermissionKey(){};
  };

  class PermissionHashKey : public nsRefPtrHashKey<PermissionKey> {
   public:
    explicit PermissionHashKey(const PermissionKey* aPermissionKey)
        : nsRefPtrHashKey<PermissionKey>(aPermissionKey) {}

    PermissionHashKey(PermissionHashKey&& toCopy)
        : nsRefPtrHashKey<PermissionKey>(std::move(toCopy)),
          mPermissions(std::move(toCopy.mPermissions)) {}

    bool KeyEquals(const PermissionKey* aKey) const {
      return *aKey == *GetKey();
    }

    static PLDHashNumber HashKey(const PermissionKey* aKey) {
      return aKey->GetHashCode();
    }

    // Force the hashtable to use the copy constructor when shuffling entries
    // around, otherwise the Auto part of our AutoTArray won't be happy!
    enum { ALLOW_MEMMOVE = false };

    inline nsTArray<PermissionEntry>& GetPermissions() { return mPermissions; }

    inline int32_t GetPermissionIndex(uint32_t aType) const {
      for (uint32_t i = 0; i < mPermissions.Length(); ++i)
        if (mPermissions[i].mType == aType) return i;

      return -1;
    }

    inline PermissionEntry GetPermission(uint32_t aType) const {
      for (uint32_t i = 0; i < mPermissions.Length(); ++i)
        if (mPermissions[i].mType == aType) return mPermissions[i];

      // unknown permission... return relevant data
      return PermissionEntry(-1, aType, nsIPermissionManager::UNKNOWN_ACTION,
                             nsIPermissionManager::EXPIRE_NEVER, 0, 0);
    }

   private:
    AutoTArray<PermissionEntry, 1> mPermissions;
  };

  // nsISupports
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIPERMISSIONMANAGER
  NS_DECL_NSIOBSERVER

  PermissionManager();
  static already_AddRefed<nsIPermissionManager> GetXPCOMSingleton();
  static PermissionManager* GetInstance();
  nsresult Init();

  // enums for AddInternal()
  enum OperationType {
    eOperationNone,
    eOperationAdding,
    eOperationRemoving,
    eOperationChanging,
    eOperationReplacingDefault
  };

  enum DBOperationType { eNoDBOperation, eWriteToDB };

  enum NotifyOperationType { eDontNotify, eNotify };

  // Similar to TestPermissionFromPrincipal, except that it is used only for
  // permissions which can never have default values.
  nsresult TestPermissionWithoutDefaultsFromPrincipal(nsIPrincipal* aPrincipal,
                                                      const nsACString& aType,
                                                      uint32_t* aPermission);

  nsresult LegacyTestPermissionFromURI(
      nsIURI* aURI, const OriginAttributes* aOriginAttributes,
      const nsACString& aType, uint32_t* aPermission);

  /**
   * Initialize the permission-manager service.
   * The permission manager is always initialized at startup because when it
   * was lazy-initialized on demand, it was possible for it to be created
   * once shutdown had begun, resulting in the manager failing to correctly
   * shutdown because it missed its shutdown observer notification.
   */
  static void Startup();

  nsresult RemovePermissionsWithAttributes(OriginAttributesPattern& aAttrs);

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
   * @param aForceStripOA Whether to force stripping the principals origin
   *        attributes prior to generating the key.
   * @param aKey  A string which will be filled with the permission
   * key.
   */
  static void GetKeyForPrincipal(nsIPrincipal* aPrincipal, bool aForceStripOA,
                                 nsACString& aKey);

  /**
   * See `nsIPermissionManager::GetPermissionsWithKey` for more info on
   * permission keys.
   *
   * Get the permission key corresponding to the given Origin. This method is
   * like GetKeyForPrincipal, except that it avoids creating a nsIPrincipal
   * object when you already have access to an origin string.
   *
   * If this method is passed a nonsensical origin string it may produce a
   * nonsensical permission key result.
   *
   * @param aOrigin  The origin which the key is to be extracted from.
   * @param aForceStripOA Whether to force stripping the origins attributes
   *        prior to generating the key.
   * @param aKey  A string which will be filled with the permission
   * key.
   */
  static void GetKeyForOrigin(const nsACString& aOrigin, bool aForceStripOA,
                              nsACString& aKey);

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
   * @param aPermissionKey  A string which will be filled with the permission
   * key.
   */
  static void GetKeyForPermission(nsIPrincipal* aPrincipal,
                                  const nsACString& aType, nsACString& aKey);

  /**
   * See `nsIPermissionManager::GetPermissionsWithKey` for more info on
   * permission keys.
   *
   * Get all permissions keys which could correspond to the given principal.
   * This method, like GetKeyForPrincipal, is infallible and should always
   * produce at least one (key, origin) pair.
   *
   * Unlike GetKeyForPrincipal, this method also gets the keys for base domains
   * of the given principal. All keys returned by this method must be available
   * in the content process for a given URL to successfully have its permissions
   * checked in the `aExactHostMatch = false` situation.
   *
   * @param aPrincipal  The Principal which the key is to be extracted from.
   * @return returns an array of (key, origin) pairs.
   */
  static nsTArray<std::pair<nsCString, nsCString>> GetAllKeysForPrincipal(
      nsIPrincipal* aPrincipal);

  // From ContentChild.
  nsresult RemoveAllFromIPC();

  /**
   * Returns false if this permission manager wouldn't have the permission
   * requested available.
   *
   * If aType is empty, checks that the permission manager would have all
   * permissions available for the given principal.
   */
  bool PermissionAvailable(nsIPrincipal* aPrincipal, const nsACString& aType);

  /**
   * The content process doesn't have access to every permission. Instead, when
   * LOAD_DOCUMENT_URI channels for http://, https://, and ftp:// URIs are
   * opened, the permissions for those channels are sent down to the content
   * process before the OnStartRequest message. Permissions for principals with
   * other schemes are sent down at process startup.
   *
   * Permissions are keyed and grouped by "Permission Key"s.
   * `PermissionManager::GetKeyForPrincipal` provides the mechanism for
   * determining the permission key for a given principal.
   *
   * This method may only be called in the parent process. It fills the nsTArray
   * argument with the IPC::Permission objects which have a matching origin.
   *
   * @param origin The origin to use to find the permissions of interest.
   * @param key The key to use to find the permissions of interest. Only used
   *            when the origin argument is empty.
   * @param perms  An array which will be filled with the permissions which
   *               match the given origin.
   */
  bool GetPermissionsFromOriginOrKey(const nsACString& aOrigin,
                                     const nsACString& aKey,
                                     nsTArray<IPC::Permission>& aPerms);

  /**
   * See `PermissionManager::GetPermissionsWithKey` for more info on
   * Permission keys.
   *
   * `SetPermissionsWithKey` may only be called in the Child process, and
   * initializes the permission manager with the permissions for a given
   * Permission key. marking permissions with that key as available.
   *
   * @param permissionKey  The key for the permissions which have been sent
   * over.
   * @param perms  An array with the permissions which match the given key.
   */
  void SetPermissionsWithKey(const nsACString& aPermissionKey,
                             nsTArray<IPC::Permission>& aPerms);

  /**
   * Add a callback which should be run when all permissions are available for
   * the given nsIPrincipal. This method invokes the callback runnable
   * synchronously when the permissions are already available. Otherwise the
   * callback will be run asynchronously in SystemGroup when all permissions
   * are available in the future.
   *
   * NOTE: This method will not request the permissions be sent by the parent
   * process. This should only be used to wait for permissions which may not
   * have arrived yet in order to ensure they are present.
   *
   * @param aPrincipal The principal to wait for permissions to be available
   * for.
   * @param aRunnable  The runnable to run when permissions are available for
   * the given principal.
   */
  void WhenPermissionsAvailable(nsIPrincipal* aPrincipal,
                                nsIRunnable* aRunnable);

 private:
  ~PermissionManager();

  /**
   * Get all permissions for a given principal, which should not be isolated
   * by user context or private browsing. The principal has its origin
   * attributes stripped before perm db lookup. This is currently only affects
   * the "cookie" permission.
   * @param aPrincipal Used for creating the permission key.
   */
  nsresult GetStripPermsForPrincipal(nsIPrincipal* aPrincipal,
                                     nsTArray<PermissionEntry>& aResult);

  // Returns -1 on failure
  int32_t GetTypeIndex(const nsACString& aType, bool aAdd);

  // Returns PermissionHashKey for a given { host, isInBrowserElement } tuple.
  // This is not simply using PermissionKey because we will walk-up domains in
  // case of |host| contains sub-domains. Returns null if nothing found. Also
  // accepts host on the format "<foo>". This will perform an exact match lookup
  // as the string doesn't contain any dots.
  PermissionHashKey* GetPermissionHashKey(nsIPrincipal* aPrincipal,
                                          uint32_t aType, bool aExactHostMatch);

  // Returns PermissionHashKey for a given { host, isInBrowserElement } tuple.
  // This is not simply using PermissionKey because we will walk-up domains in
  // case of |host| contains sub-domains. Returns null if nothing found. Also
  // accepts host on the format "<foo>". This will perform an exact match lookup
  // as the string doesn't contain any dots.
  PermissionHashKey* GetPermissionHashKey(
      nsIURI* aURI, const OriginAttributes* aOriginAttributes, uint32_t aType,
      bool aExactHostMatch);

  // The int32_t is the type index, the nsresult is an early bail-out return
  // code.
  typedef Variant<int32_t, nsresult> TestPreparationResult;
  TestPreparationResult CommonPrepareToTestPermission(
      nsIPrincipal* aPrincipal, int32_t aTypeIndex, const nsACString& aType,
      uint32_t* aPermission, uint32_t aDefaultPermission,
      bool aDefaultPermissionIsValid, bool aExactHostMatch,
      bool aIncludingSession);

  // If aTypeIndex is passed -1, we try to inder the type index from aType.
  nsresult CommonTestPermission(nsIPrincipal* aPrincipal, int32_t aTypeIndex,
                                const nsACString& aType, uint32_t* aPermission,
                                uint32_t aDefaultPermission,
                                bool aDefaultPermissionIsValid,
                                bool aExactHostMatch, bool aIncludingSession);

  // If aTypeIndex is passed -1, we try to inder the type index from aType.
  nsresult CommonTestPermission(nsIURI* aURI, int32_t aTypeIndex,
                                const nsACString& aType, uint32_t* aPermission,
                                uint32_t aDefaultPermission,
                                bool aDefaultPermissionIsValid,
                                bool aExactHostMatch, bool aIncludingSession);

  nsresult CommonTestPermission(nsIURI* aURI,
                                const OriginAttributes* aOriginAttributes,
                                int32_t aTypeIndex, const nsACString& aType,
                                uint32_t* aPermission,
                                uint32_t aDefaultPermission,
                                bool aDefaultPermissionIsValid,
                                bool aExactHostMatch, bool aIncludingSession);

  // Only one of aPrincipal or aURI is allowed to be passed in.
  nsresult CommonTestPermissionInternal(
      nsIPrincipal* aPrincipal, nsIURI* aURI,
      const OriginAttributes* aOriginAttributes, int32_t aTypeIndex,
      const nsACString& aType, uint32_t* aPermission, bool aExactHostMatch,
      bool aIncludingSession);

  nsresult OpenDatabase(nsIFile* permissionsFile);

  void InitDB(bool aRemoveFile);
  nsresult TryInitDB(bool aRemoveFile, nsIInputStream* aDefaultsInputStream);

  void AddIdleDailyMaintenanceJob();
  void RemoveIdleDailyMaintenanceJob();
  void PerformIdleDailyMaintenance();

  nsresult ImportLatestDefaults();
  already_AddRefed<nsIInputStream> GetDefaultsInputStream();
  void ConsumeDefaultsInputStream(nsIInputStream* aDefaultsInputStream,
                                  const MonitorAutoLock& aProofOfLock);

  nsresult CreateTable();
  void NotifyObserversWithPermission(nsIPrincipal* aPrincipal,
                                     const nsACString& aType,
                                     uint32_t aPermission, uint32_t aExpireType,
                                     int64_t aExpireTime,
                                     int64_t aModificationTime,
                                     const char16_t* aData);
  void NotifyObservers(nsIPermission* aPermission, const char16_t* aData);

  // Finalize all statements, close the DB and null it.
  // if aRebuildOnSuccess, reinitialize database
  void CloseDB(bool aRebuildOnSuccess = false);

  nsresult RemoveAllInternal(bool aNotifyObservers);
  nsresult RemoveAllFromMemory();

  void UpdateDB(OperationType aOp, int64_t aID, const nsACString& aOrigin,
                const nsACString& aType, uint32_t aPermission,
                uint32_t aExpireType, int64_t aExpireTime,
                int64_t aModificationTime);

  /**
   * This method removes all permissions modified after the specified time.
   */
  nsresult RemoveAllModifiedSince(int64_t aModificationTime);

  template <class T>
  nsresult RemovePermissionEntries(T aCondition);

  // This method must be called before doing any operation to be sure that the
  // DB reading has been completed. This method is also in charge to complete
  // the migrations if needed.
  void EnsureReadCompleted();

  nsresult AddInternal(nsIPrincipal* aPrincipal, const nsACString& aType,
                       uint32_t aPermission, int64_t aID, uint32_t aExpireType,
                       int64_t aExpireTime, int64_t aModificationTime,
                       NotifyOperationType aNotifyOperation,
                       DBOperationType aDBOperation,
                       const bool aIgnoreSessionPermissions = false,
                       const nsACString* aOriginString = nullptr);

  void MaybeAddReadEntryFromMigration(const nsACString& aOrigin,
                                      const nsCString& aType,
                                      uint32_t aPermission,
                                      uint32_t aExpireType, int64_t aExpireTime,
                                      int64_t aModificationTime, int64_t aId);

  nsRefPtrHashtable<nsCStringHashKey, GenericNonExclusivePromise::Private>
      mPermissionKeyPromiseMap;

  nsCOMPtr<nsIFile> mPermissionsFile;

  // This monitor is used to ensure the database reading before any other
  // operation. The reading of the database happens OMT. See |State| to know the
  // steps of the database reading.
  Monitor mMonitor;

  enum State {
    // Initial state. The database has not been read yet.
    // |TryInitDB| is called at startup time to read the database OMT.
    // During the reading, |mReadEntries| will be populated with all the
    // existing permissions.
    eInitializing,

    // At the end of the database reading, we are in this state. A runnable is
    // executed to call |EnsureReadCompleted| on the main thread.
    // |EnsureReadCompleted| processes |mReadEntries| and goes to the next
    // state.
    eDBInitialized,

    // The permissions are fully read and any pending operation can proceed.
    eReady,

    // The permission manager has been terminated. No extra database operations
    // will be allowed.
    eClosed,
  };
  Atomic<State> mState;

  // A single entry, from the database.
  struct ReadEntry {
    ReadEntry()
        : mId(0),
          mPermission(0),
          mExpireType(0),
          mExpireTime(0),
          mModificationTime(0) {}

    nsCString mOrigin;
    nsCString mType;
    int64_t mId;
    uint32_t mPermission;
    uint32_t mExpireType;
    int64_t mExpireTime;
    int64_t mModificationTime;

    // true if this entry is the result of a migration.
    bool mFromMigration;
  };

  // List of entries read from the database. It will be populated OMT and
  // consumed on the main-thread.
  // This array is protected by the monitor.
  nsTArray<ReadEntry> mReadEntries;

  // A single entry, from the database.
  struct MigrationEntry {
    MigrationEntry()
        : mId(0),
          mPermission(0),
          mExpireType(0),
          mExpireTime(0),
          mModificationTime(0),
          mIsInBrowserElement(false) {}

    nsCString mHost;
    nsCString mType;
    int64_t mId;
    uint32_t mPermission;
    uint32_t mExpireType;
    int64_t mExpireTime;
    int64_t mModificationTime;

    // Legacy, for migration.
    bool mIsInBrowserElement;
  };

  // List of entries read from the database. It will be populated OMT and
  // consumed on the main-thread. The migration entries will be converted to
  // ReadEntry in |CompleteMigrations|.
  // This array is protected by the monitor.
  nsTArray<MigrationEntry> mMigrationEntries;

  // A single entry from the defaults URL.
  struct DefaultEntry {
    DefaultEntry() : mOp(eImportMatchTypeHost), mPermission(0) {}

    enum Op {
      eImportMatchTypeHost,
      eImportMatchTypeOrigin,
    };

    Op mOp;

    nsCString mHostOrOrigin;
    nsCString mType;
    uint32_t mPermission;
  };

  // List of entries read from the default settings.
  // This array is protected by the monitor.
  nsTArray<DefaultEntry> mDefaultEntries;

  nsresult Read(const MonitorAutoLock& aProofOfLock);
  void CompleteRead();

  void CompleteMigrations();

  bool mMemoryOnlyDB;

  nsTHashtable<PermissionHashKey> mPermissionTable;
  // a unique, monotonically increasing id used to identify each database entry
  int64_t mLargestID;

  nsCOMPtr<nsIPrefBranch> mDefaultPrefBranch;

  // NOTE: Ensure this is the last member since it has a large inline buffer.
  // An array to store the strings identifying the different types.
  Vector<nsCString, 512> mTypeArray;

  nsCOMPtr<nsIThread> mThread;

  struct ThreadBoundData {
    nsCOMPtr<mozIStorageConnection> mDBConn;

    nsCOMPtr<mozIStorageStatement> mStmtInsert;
    nsCOMPtr<mozIStorageStatement> mStmtDelete;
    nsCOMPtr<mozIStorageStatement> mStmtUpdate;
  };
  ThreadBound<ThreadBoundData> mThreadBoundData;

  friend class DeleteFromMozHostListener;
  friend class CloseDatabaseListener;
};

// {4F6B5E00-0C36-11d5-A535-0010A401EB10}
#define NS_PERMISSIONMANAGER_CID                   \
  {                                                \
    0x4f6b5e00, 0xc36, 0x11d5, {                   \
      0xa5, 0x35, 0x0, 0x10, 0xa4, 0x1, 0xeb, 0x10 \
    }                                              \
  }

}  // namespace mozilla

#endif  // mozilla_PermissionManager_h
