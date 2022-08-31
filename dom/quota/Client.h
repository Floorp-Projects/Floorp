/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_quota_client_h__
#define mozilla_dom_quota_client_h__

#include "ErrorList.h"
#include "mozilla/Atomics.h"
#include "mozilla/Result.h"
#include "mozilla/dom/ipc/IdType.h"
#include "mozilla/dom/quota/PersistenceType.h"
#include "nsHashKeys.h"
#include "nsISupports.h"
#include "nsStringFwd.h"
#include "nsTHashSet.h"

// XXX Remove this dependency.
#include "mozilla/dom/LocalStorageCommon.h"

class nsIFile;

#define IDB_DIRECTORY_NAME "idb"
#define DOMCACHE_DIRECTORY_NAME "cache"
#define SDB_DIRECTORY_NAME "sdb"
#define FILESYSTEM_DIRECTORY_NAME "fs"
#define LS_DIRECTORY_NAME "ls"

// Deprecated
#define ASMJSCACHE_DIRECTORY_NAME "asmjs"

namespace mozilla::dom {
template <typename T>
struct Nullable;
}

namespace mozilla::dom::quota {

struct OriginMetadata;
class OriginScope;
class QuotaManager;
class UsageInfo;

// An abstract interface for quota manager clients.
// Each storage API must provide an implementation of this interface in order
// to participate in centralized quota and storage handling.
class Client {
 public:
  using AtomicBool = Atomic<bool>;

  enum Type {
    IDB = 0,
    // APPCACHE,
    DOMCACHE,
    SDB,
    FILESYSTEM,
    LS,
    TYPE_MAX
  };

  class DirectoryLockIdTable final {
    nsTHashSet<uint64_t> mIds;

   public:
    void Put(const int64_t aId) { mIds.Insert(aId); }

    bool Has(const int64_t aId) const { return mIds.Contains(aId); }

    bool Filled() const { return mIds.Count(); }
  };

  static Type TypeMax() {
    if (NextGenLocalStorageEnabled()) {
      return TYPE_MAX;
    }
    return LS;
  }

  static bool IsValidType(Type aType);

  static bool TypeToText(Type aType, nsAString& aText, const fallible_t&);

  static nsAutoCString TypeToText(Type aType);

  static bool TypeFromText(const nsAString& aText, Type& aType,
                           const fallible_t&);

  static Type TypeFromText(const nsACString& aText);

  static char TypeToPrefix(Type aType);

  static bool TypeFromPrefix(char aPrefix, Type& aType, const fallible_t&);

  static bool IsDeprecatedClient(const nsAString& aText) {
    return aText.EqualsLiteral(ASMJSCACHE_DIRECTORY_NAME);
  }

  template <typename T>
  static bool IsLockForObjectContainedInLockTable(
      const T& aObject, const DirectoryLockIdTable& aIds);

  template <typename T>
  static bool IsLockForObjectAcquiredAndContainedInLockTable(
      const T& aObject, const DirectoryLockIdTable& aIds);

  NS_INLINE_DECL_PURE_VIRTUAL_REFCOUNTING

  virtual Type GetType() = 0;

  // Methods which are called on the IO thread.
  virtual nsresult UpgradeStorageFrom1_0To2_0(nsIFile* aDirectory) {
    return NS_OK;
  }

  virtual nsresult UpgradeStorageFrom2_0To2_1(nsIFile* aDirectory) {
    return NS_OK;
  }

  virtual nsresult UpgradeStorageFrom2_1To2_2(nsIFile* aDirectory) {
    return NS_OK;
  }

  virtual Result<UsageInfo, nsresult> InitOrigin(
      PersistenceType aPersistenceType, const OriginMetadata& aOriginMetadata,
      const AtomicBool& aCanceled) = 0;

  virtual nsresult InitOriginWithoutTracking(
      PersistenceType aPersistenceType, const OriginMetadata& aOriginMetadata,
      const AtomicBool& aCanceled) = 0;

  virtual Result<UsageInfo, nsresult> GetUsageForOrigin(
      PersistenceType aPersistenceType, const OriginMetadata& aOriginMetadata,
      const AtomicBool& aCanceled) = 0;

  // This method is called when origins are about to be cleared
  // (except the case when clearing is triggered by the origin eviction).
  virtual nsresult AboutToClearOrigins(
      const Nullable<PersistenceType>& aPersistenceType,
      const OriginScope& aOriginScope) {
    return NS_OK;
  }

  virtual void OnOriginClearCompleted(PersistenceType aPersistenceType,
                                      const nsACString& aOrigin) = 0;

  virtual void ReleaseIOThreadObjects() = 0;

  // Methods which are called on the background thread.
  virtual void AbortOperationsForLocks(
      const DirectoryLockIdTable& aDirectoryLockIds) = 0;

  virtual void AbortOperationsForProcess(ContentParentId aContentParentId) = 0;

  virtual void AbortAllOperations() = 0;

  virtual void StartIdleMaintenance() = 0;

  virtual void StopIdleMaintenance() = 0;

  // Both variants just check for QuotaManager::IsShuttingDown()
  // but assert to be on the right thread.
  // They must not be used for re-entrance checks.
  // Deprecated: This distinction is not needed anymore.
  // QuotaClients should call QuotaManager::IsShuttingDown instead.
  static bool IsShuttingDownOnBackgroundThread();
  static bool IsShuttingDownOnNonBackgroundThread();

  // Returns true if there is work that needs to be waited for.
  bool InitiateShutdownWorkThreads();
  void FinalizeShutdownWorkThreads();

  virtual nsCString GetShutdownStatus() const = 0;
  virtual bool IsShutdownCompleted() const = 0;
  virtual void ForceKillActors() = 0;

 private:
  virtual void InitiateShutdown() = 0;
  virtual void FinalizeShutdown() = 0;

 protected:
  virtual ~Client() = default;
};

}  // namespace mozilla::dom::quota

#endif  // mozilla_dom_quota_client_h__
