/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
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
#include "nsIPrefBranch.h"
#include "nsHashKeys.h"
#include "nsCOMArray.h"
#include "nsDataHashtable.h"
#include "nsRefPtrHashtable.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/ExpandedPrincipal.h"
#include "mozilla/Permission.h"
#include "mozilla/MozPromise.h"
#include "mozilla/Unused.h"
#include "mozilla/Variant.h"
#include "mozilla/Vector.h"

#include <utility>

namespace IPC {
struct Permission;
}

namespace mozilla {
class OriginAttributesPattern;
}

class nsIPermission;
class mozIStorageConnection;
class mozIStorageAsyncStatement;

////////////////////////////////////////////////////////////////////////////////

class nsPermissionManager final : public nsIPermissionManager,
                                  public nsIObserver,
                                  public nsSupportsWeakReference {
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
        nsIURI* aURI, const mozilla::OriginAttributes* aOriginAttributes,
        bool aForceStripOA, nsresult& aResult);

    explicit PermissionKey(const nsACString& aOrigin)
        : mOrigin(aOrigin), mHashCode(mozilla::HashString(aOrigin)) {}

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
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPERMISSIONMANAGER
  NS_DECL_NSIOBSERVER

  nsPermissionManager();
  static already_AddRefed<nsIPermissionManager> GetXPCOMSingleton();
  static nsPermissionManager* GetInstance();
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

  // A special value for a permission ID that indicates the ID was loaded as
  // a default value.  These will never be written to the database, but may
  // be overridden with an explicit permission (including UNKNOWN_ACTION)
  static const int64_t cIDPermissionIsDefault = -1;

  nsresult AddInternal(nsIPrincipal* aPrincipal, const nsACString& aType,
                       uint32_t aPermission, int64_t aID, uint32_t aExpireType,
                       int64_t aExpireTime, int64_t aModificationTime,
                       NotifyOperationType aNotifyOperation,
                       DBOperationType aDBOperation,
                       const bool aIgnoreSessionPermissions = false,
                       const nsACString* aOriginString = nullptr);

  // Similar to TestPermissionFromPrincipal, except that it is used only for
  // permissions which can never have default values.
  nsresult TestPermissionWithoutDefaultsFromPrincipal(nsIPrincipal* aPrincipal,
                                                      const nsACString& aType,
                                                      uint32_t* aPermission) {
    MOZ_ASSERT(!HasDefaultPref(aType));

    return CommonTestPermission(aPrincipal, -1, aType, aPermission,
                                nsIPermissionManager::UNKNOWN_ACTION, true,
                                false, true);
  }

  nsresult LegacyTestPermissionFromURI(
      nsIURI* aURI, const mozilla::OriginAttributes* aOriginAttributes,
      const nsACString& aType, uint32_t* aPermission);

  /**
   * Initialize the permission-manager service.
   * The permission manager is always initialized at startup because when it
   * was lazy-initialized on demand, it was possible for it to be created
   * once shutdown had begun, resulting in the manager failing to correctly
   * shutdown because it missed its shutdown observer notification.
   */
  static void Startup();

  nsresult RemovePermissionsWithAttributes(
      mozilla::OriginAttributesPattern& aAttrs);

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
   * `nsPermissionManager::GetKeyForPrincipal` provides the mechanism for
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
   * See `nsPermissionManager::GetPermissionsWithKey` for more info on
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
  virtual ~nsPermissionManager();

  /**
   * Get all permissions for a given principal, which should not be isolated
   * by user context or private browsing. The principal has its origin
   * attributes stripped before perm db lookup. This is currently only affects
   * the "cookie" permission.
   * @param aPrincipal Used for creating the permission key.
   */
  nsresult GetStripPermsForPrincipal(nsIPrincipal* aPrincipal,
                                     nsTArray<PermissionEntry>& aResult);

  // NOTE: nullptr can be passed as aType - if it is this function will return
  // "false" unconditionally.
  static bool HasDefaultPref(const nsACString& aType) {
    // A list of permissions that can have a fallback default permission
    // set under the permissions.default.* pref.
    static const nsLiteralCString kPermissionsWithDefaults[] = {
        NS_LITERAL_CSTRING("camera"), NS_LITERAL_CSTRING("microphone"),
        NS_LITERAL_CSTRING("geo"), NS_LITERAL_CSTRING("desktop-notification"),
        NS_LITERAL_CSTRING("shortcuts")};

    if (!aType.IsEmpty()) {
      for (const auto& perm : kPermissionsWithDefaults) {
        if (perm.Equals(aType)) {
          return true;
        }
      }
    }

    return false;
  }

  // Returns -1 on failure
  int32_t GetTypeIndex(const nsACString& aType, bool aAdd) {
    for (uint32_t i = 0; i < mTypeArray.length(); ++i) {
      if (mTypeArray[i].Equals(aType)) {
        return i;
      }
    }

    if (!aAdd) {
      // Not found, but that is ok - we were just looking.
      return -1;
    }

    // This type was not registered before.
    // append it to the array, without copy-constructing the string
    if (!mTypeArray.emplaceBack(aType)) {
      return -1;
    }

    return mTypeArray.length() - 1;
  }

  PermissionHashKey* GetPermissionHashKey(nsIPrincipal* aPrincipal,
                                          uint32_t aType, bool aExactHostMatch);
  PermissionHashKey* GetPermissionHashKey(
      nsIURI* aURI, const mozilla::OriginAttributes* aOriginAttributes,
      uint32_t aType, bool aExactHostMatch);

  // The int32_t is the type index, the nsresult is an early bail-out return
  // code.
  typedef mozilla::Variant<int32_t, nsresult> TestPreparationResult;
  /**
   * Perform the early steps of a permission check and determine whether we need
   * to call CommonTestPermissionInternal() for the actual permission check.
   *
   * @param aPrincipal optional principal argument to check the permission for,
   *                   can be nullptr if we aren't performing a principal-based
   *                   check.
   * @param aTypeIndex if the caller isn't sure what the index of the permission
   *                   type to check for is in the mTypeArray member variable,
   *                   it should pass -1, otherwise this would be the index of
   *                   the type inside mTypeArray.  This would only be something
   *                   other than -1 in recursive invocations of this function.
   * @param aType      the permission type to test.
   * @param aPermission out argument which will be a permission type that we
   *                    will return from this function once the function is
   *                    done.
   * @param aDefaultPermission the default permission to be used if we can't
   *                           determine the result of the permission check.
   * @param aDefaultPermissionIsValid whether the previous argument contains a
   *                                  valid value.
   * @param aExactHostMatch whether to look for the exact host name or also for
   *                        subdomains that can have the same permission.
   * @param aIncludingSession whether to include session permissions when
   *                          testing for the permission.
   */
  TestPreparationResult CommonPrepareToTestPermission(
      nsIPrincipal* aPrincipal, int32_t aTypeIndex, const nsACString& aType,
      uint32_t* aPermission, uint32_t aDefaultPermission,
      bool aDefaultPermissionIsValid, bool aExactHostMatch,
      bool aIncludingSession) {
    using mozilla::AsVariant;

    auto* basePrin = mozilla::BasePrincipal::Cast(aPrincipal);
    if (basePrin && basePrin->IsSystemPrincipal()) {
      *aPermission = ALLOW_ACTION;
      return AsVariant(NS_OK);
    }

    // For some permissions, query the default from a pref. We want to avoid
    // doing this for all permissions so that permissions can opt into having
    // the pref lookup overhead on each call.
    int32_t defaultPermission =
        aDefaultPermissionIsValid ? aDefaultPermission : UNKNOWN_ACTION;
    if (!aDefaultPermissionIsValid && HasDefaultPref(aType)) {
      mozilla::Unused << mDefaultPrefBranch->GetIntPref(
          PromiseFlatCString(aType).get(), &defaultPermission);
    }

    // Set the default.
    *aPermission = defaultPermission;

    int32_t typeIndex =
        aTypeIndex == -1 ? GetTypeIndex(aType, false) : aTypeIndex;

    // For expanded principals, we want to iterate over the allowlist and see
    // if the permission is granted for any of them.
    if (basePrin && basePrin->Is<ExpandedPrincipal>()) {
      auto ep = basePrin->As<ExpandedPrincipal>();
      for (auto& prin : ep->AllowList()) {
        uint32_t perm;
        nsresult rv = CommonTestPermission(prin, typeIndex, aType, &perm,
                                           defaultPermission, true,
                                           aExactHostMatch, aIncludingSession);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return AsVariant(rv);
        }

        if (perm == nsIPermissionManager::ALLOW_ACTION) {
          *aPermission = perm;
          return AsVariant(NS_OK);
        }
        if (perm == nsIPermissionManager::PROMPT_ACTION) {
          // Store it, but keep going to see if we can do better.
          *aPermission = perm;
        }
      }

      return AsVariant(NS_OK);
    }

    // If type == -1, the type isn't known, just signal that we are done.
    if (typeIndex == -1) {
      return AsVariant(NS_OK);
    }

    return AsVariant(typeIndex);
  }

  // If aTypeIndex is passed -1, we try to inder the type index from aType.
  nsresult CommonTestPermission(nsIPrincipal* aPrincipal, int32_t aTypeIndex,
                                const nsACString& aType, uint32_t* aPermission,
                                uint32_t aDefaultPermission,
                                bool aDefaultPermissionIsValid,
                                bool aExactHostMatch, bool aIncludingSession) {
    auto preparationResult = CommonPrepareToTestPermission(
        aPrincipal, aTypeIndex, aType, aPermission, aDefaultPermission,
        aDefaultPermissionIsValid, aExactHostMatch, aIncludingSession);
    if (preparationResult.is<nsresult>()) {
      return preparationResult.as<nsresult>();
    }

    return CommonTestPermissionInternal(
        aPrincipal, nullptr, nullptr, preparationResult.as<int32_t>(), aType,
        aPermission, aExactHostMatch, aIncludingSession);
  }
  // If aTypeIndex is passed -1, we try to inder the type index from aType.
  nsresult CommonTestPermission(nsIURI* aURI, int32_t aTypeIndex,
                                const nsACString& aType, uint32_t* aPermission,
                                uint32_t aDefaultPermission,
                                bool aDefaultPermissionIsValid,
                                bool aExactHostMatch, bool aIncludingSession) {
    auto preparationResult = CommonPrepareToTestPermission(
        nullptr, aTypeIndex, aType, aPermission, aDefaultPermission,
        aDefaultPermissionIsValid, aExactHostMatch, aIncludingSession);
    if (preparationResult.is<nsresult>()) {
      return preparationResult.as<nsresult>();
    }

    return CommonTestPermissionInternal(
        nullptr, aURI, nullptr, preparationResult.as<int32_t>(), aType,
        aPermission, aExactHostMatch, aIncludingSession);
  }
  nsresult CommonTestPermission(
      nsIURI* aURI, const mozilla::OriginAttributes* aOriginAttributes,
      int32_t aTypeIndex, const nsACString& aType, uint32_t* aPermission,
      uint32_t aDefaultPermission, bool aDefaultPermissionIsValid,
      bool aExactHostMatch, bool aIncludingSession) {
    auto preparationResult = CommonPrepareToTestPermission(
        nullptr, aTypeIndex, aType, aPermission, aDefaultPermission,
        aDefaultPermissionIsValid, aExactHostMatch, aIncludingSession);
    if (preparationResult.is<nsresult>()) {
      return preparationResult.as<nsresult>();
    }

    return CommonTestPermissionInternal(
        nullptr, aURI, aOriginAttributes, preparationResult.as<int32_t>(),
        aType, aPermission, aExactHostMatch, aIncludingSession);
  }
  // Only one of aPrincipal or aURI is allowed to be passed in.
  nsresult CommonTestPermissionInternal(
      nsIPrincipal* aPrincipal, nsIURI* aURI,
      const mozilla::OriginAttributes* aOriginAttributes, int32_t aTypeIndex,
      const nsACString& aType, uint32_t* aPermission, bool aExactHostMatch,
      bool aIncludingSession);

  nsresult OpenDatabase(nsIFile* permissionsFile);
  nsresult InitDB(bool aRemoveFile);
  void AddIdleDailyMaintenanceJob();
  void RemoveIdleDailyMaintenanceJob();
  void PerformIdleDailyMaintenance();

  nsresult CreateTable();
  nsresult ImportDefaults();
  nsresult _DoImport(nsIInputStream* inputStream, mozIStorageConnection* aConn);
  nsresult Read();
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
  static void UpdateDB(OperationType aOp, mozIStorageAsyncStatement* aStmt,
                       int64_t aID, const nsACString& aOrigin,
                       const nsACString& aType, uint32_t aPermission,
                       uint32_t aExpireType, int64_t aExpireTime,
                       int64_t aModificationTime);

  /**
   * This method removes all permissions modified after the specified time.
   */
  nsresult RemoveAllModifiedSince(int64_t aModificationTime);

  template <class T>
  nsresult RemovePermissionEntries(T aCondition);

  nsRefPtrHashtable<nsCStringHashKey,
                    mozilla::GenericNonExclusivePromise::Private>
      mPermissionKeyPromiseMap;

  nsCOMPtr<mozIStorageConnection> mDBConn;
  nsCOMPtr<mozIStorageAsyncStatement> mStmtInsert;
  nsCOMPtr<mozIStorageAsyncStatement> mStmtDelete;
  nsCOMPtr<mozIStorageAsyncStatement> mStmtUpdate;

  bool mMemoryOnlyDB;

  nsTHashtable<PermissionHashKey> mPermissionTable;
  // a unique, monotonically increasing id used to identify each database entry
  int64_t mLargestID;

  nsCOMPtr<nsIPrefBranch> mDefaultPrefBranch;

  // NOTE: Ensure this is the last member since it has a large inline buffer.
  // An array to store the strings identifying the different types.
  mozilla::Vector<nsCString, 512> mTypeArray;

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

#endif /* nsPermissionManager_h__ */
