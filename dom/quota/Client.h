/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_quota_client_h__
#define mozilla_dom_quota_client_h__

#include "mozilla/dom/quota/QuotaCommon.h"

#include "mozilla/dom/LocalStorageCommon.h"
#include "mozilla/dom/ipc/IdType.h"

#include "PersistenceType.h"

class nsIFile;
class nsIRunnable;

#define IDB_DIRECTORY_NAME "idb"
#define DOMCACHE_DIRECTORY_NAME "cache"
#define SDB_DIRECTORY_NAME "sdb"
#define LS_DIRECTORY_NAME "ls"

// Deprecated
#define ASMJSCACHE_DIRECTORY_NAME "asmjs"

BEGIN_QUOTA_NAMESPACE

class OriginScope;
class QuotaManager;
class UsageInfo;

// An abstract interface for quota manager clients.
// Each storage API must provide an implementation of this interface in order
// to participate in centralized quota and storage handling.
class Client {
 public:
  typedef Atomic<bool> AtomicBool;

  enum Type {
    IDB = 0,
    // APPCACHE,
    DOMCACHE,
    SDB,
    LS,
    TYPE_MAX
  };

  static Type TypeMax() {
    if (CachedNextGenLocalStorageEnabled()) {
      return TYPE_MAX;
    }
    return LS;
  }

  static bool TypeToText(Type aType, nsAString& aText, const fallible_t&);

  static void TypeToText(Type aType, nsAString& aText);

  static void TypeToText(Type aType, nsACString& aText);

  static bool TypeFromText(const nsAString& aText, Type& aType,
                           const fallible_t&);

  static Type TypeFromText(const nsACString& aText);

  static bool IsDeprecatedClient(const nsAString& aText) {
    return aText.EqualsLiteral(ASMJSCACHE_DIRECTORY_NAME);
  }

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

  virtual nsresult InitOrigin(PersistenceType aPersistenceType,
                              const nsACString& aGroup,
                              const nsACString& aOrigin,
                              const AtomicBool& aCanceled,
                              UsageInfo* aUsageInfo, bool aForGetUsage) = 0;

  virtual nsresult GetUsageForOrigin(PersistenceType aPersistenceType,
                                     const nsACString& aGroup,
                                     const nsACString& aOrigin,
                                     const AtomicBool& aCanceled,
                                     UsageInfo* aUsageInfo) = 0;

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
  virtual void AbortOperations(const nsACString& aOrigin) = 0;

  virtual void AbortOperationsForProcess(ContentParentId aContentParentId) = 0;

  virtual void StartIdleMaintenance() = 0;

  virtual void StopIdleMaintenance() = 0;

  virtual void ShutdownWorkThreads() = 0;

 protected:
  virtual ~Client() {}
};

END_QUOTA_NAMESPACE

#endif  // mozilla_dom_quota_client_h__
