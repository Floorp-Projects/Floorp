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
  typedef mozilla::Atomic<bool> AtomicBool;

  NS_INLINE_DECL_PURE_VIRTUAL_REFCOUNTING

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

  virtual Type GetType() = 0;

  static nsresult TypeToText(Type aType, nsAString& aText) {
    switch (aType) {
      case IDB:
        aText.AssignLiteral(IDB_DIRECTORY_NAME);
        break;

      case DOMCACHE:
        aText.AssignLiteral(DOMCACHE_DIRECTORY_NAME);
        break;

      case SDB:
        aText.AssignLiteral(SDB_DIRECTORY_NAME);
        break;

      case LS:
        if (CachedNextGenLocalStorageEnabled()) {
          aText.AssignLiteral(LS_DIRECTORY_NAME);
          break;
        }
        MOZ_FALLTHROUGH;

      case TYPE_MAX:
      default:
        MOZ_ASSERT_UNREACHABLE("Bad id value!");
        return NS_ERROR_UNEXPECTED;
    }

    return NS_OK;
  }

  static nsresult TypeFromText(const nsAString& aText, Type& aType) {
    if (aText.EqualsLiteral(IDB_DIRECTORY_NAME)) {
      aType = IDB;
    } else if (aText.EqualsLiteral(DOMCACHE_DIRECTORY_NAME)) {
      aType = DOMCACHE;
    } else if (aText.EqualsLiteral(SDB_DIRECTORY_NAME)) {
      aType = SDB;
    } else if (CachedNextGenLocalStorageEnabled() &&
               aText.EqualsLiteral(LS_DIRECTORY_NAME)) {
      aType = LS;
    } else {
      return NS_ERROR_FAILURE;
    }

    return NS_OK;
  }

  static nsresult NullableTypeFromText(const nsAString& aText,
                                       Nullable<Type>* aType) {
    if (aText.IsVoid()) {
      *aType = Nullable<Type>();
      return NS_OK;
    }

    Type type;
    nsresult rv = TypeFromText(aText, type);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    *aType = Nullable<Type>(type);
    return NS_OK;
  }

  static bool IsDeprecatedClient(const nsAString& aText) {
    return aText.EqualsLiteral(ASMJSCACHE_DIRECTORY_NAME);
  }

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

  virtual void OnStorageInitFailed(){};

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
