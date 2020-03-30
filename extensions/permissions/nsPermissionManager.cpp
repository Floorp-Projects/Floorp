/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Attributes.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/ContentBlockingUserInteraction.h"
#include "mozilla/ContentPrincipal.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/Services.h"
#include "mozilla/SystemGroup.h"
#include "nsPermissionManager.h"
#include "nsPermission.h"
#include "nsCRT.h"
#include "nsNetUtil.h"
#include "nsTArray.h"
#include "nsReadableUtils.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsDirectoryServiceDefs.h"
#include "mozIStorageCompletionCallback.h"
#include "mozIStorageService.h"
#include "mozIStorageStatementCallback.h"
#include "mozilla/storage.h"
#include "mozilla/Attributes.h"
#include "nsXULAppAPI.h"
#include "nsIIdleService.h"
#include "nsIPrincipal.h"
#include "nsIURIMutator.h"
#include "nsContentUtils.h"
#include "nsPIDOMWindow.h"
#include "mozilla/dom/Document.h"
#include "mozilla/net/NeckoMessageUtils.h"
#include "mozilla/Preferences.h"
#include "mozilla/StaticPrefs_permissions.h"
#include "nsReadLine.h"
#include "mozilla/Telemetry.h"
#include "nsIConsoleService.h"
#include "nsINavHistoryService.h"
#include "nsToolkitCompsCID.h"
#include "nsIObserverService.h"
#include "nsPrintfCString.h"
#include "mozilla/AbstractThread.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/ClearOnShutdown.h"
#include "nsEffectiveTLDService.h"

static mozilla::StaticRefPtr<nsPermissionManager> gPermissionManager;

// Initially, |false|. Set to |true| once shutdown has started, to avoid
// reopening the database.
static bool gIsShuttingDown = false;

using namespace mozilla;
using namespace mozilla::dom;

static bool IsChildProcess() { return XRE_IsContentProcess(); }

static void LogToConsole(const nsAString& aMsg) {
  nsCOMPtr<nsIConsoleService> console(
      do_GetService("@mozilla.org/consoleservice;1"));
  if (!console) {
    NS_WARNING("Failed to log message to console.");
    return;
  }

  nsAutoString msg(aMsg);
  console->LogStringMessage(msg.get());
}

#define ENSURE_NOT_CHILD_PROCESS_(onError)                 \
  PR_BEGIN_MACRO                                           \
  if (IsChildProcess()) {                                  \
    NS_ERROR("Cannot perform action in content process!"); \
    onError                                                \
  }                                                        \
  PR_END_MACRO

#define ENSURE_NOT_CHILD_PROCESS \
  ENSURE_NOT_CHILD_PROCESS_({ return NS_ERROR_NOT_AVAILABLE; })

#define ENSURE_NOT_CHILD_PROCESS_NORET ENSURE_NOT_CHILD_PROCESS_(;)

#define EXPIRY_NOW PR_Now() / 1000

////////////////////////////////////////////////////////////////////////////////

namespace {

// These permissions are special permissions which must be transmitted to the
// content process before documents with their principals have loaded within
// that process.
//
// Permissions which are in this list are considered to have a "" permission
// key, even if their principal would not normally have that key.
static const nsLiteralCString kPreloadPermissions[] = {
    // This permission is preloaded to support properly blocking service worker
    // interception when a user has disabled storage for a specific site.  Once
    // service worker interception moves to the parent process this should be
    // removed.  See bug 1428130.
    NS_LITERAL_CSTRING("cookie"),

    USER_INTERACTION_PERM};

// Certain permissions should never be persisted to disk under GeckoView; it's
// the responsibility of the app to manage storing these beyond the scope of
// a single session.
#ifdef ANDROID
static const nsLiteralCString kGeckoViewRestrictedPermissions[] = {
    NS_LITERAL_CSTRING("MediaManagerVideo"),
    NS_LITERAL_CSTRING("geolocation"),
    NS_LITERAL_CSTRING("desktop-notification"),
    NS_LITERAL_CSTRING("persistent-storage"),
    NS_LITERAL_CSTRING("trackingprotection"),
    NS_LITERAL_CSTRING("trackingprotection-pb")};
#endif

// NOTE: nullptr can be passed as aType - if it is this function will return
// "false" unconditionally.
bool IsPreloadPermission(const nsACString& aType) {
  if (!aType.IsEmpty()) {
    for (const auto& perm : kPreloadPermissions) {
      if (perm.Equals(aType)) {
        return true;
      }
    }
  }

  return false;
}

// Array of permission types which should not be isolated by origin attributes,
// for user context and private browsing.
// Keep this array in sync with 'STRIPPED_PERMS' in
// 'test_permmanager_oa_strip.js'
// Currently only preloaded permissions are supported.
// This is because perms are sent to the content process in bulk by perm key.
// Non-preloaded, but OA stripped permissions would not be accessible by sites
// in private browsing / non-default user context.
static constexpr std::array<nsLiteralCString, 1> kStripOAPermissions = {
    {NS_LITERAL_CSTRING("cookie")}};

bool IsOAForceStripPermission(const nsACString& aType) {
  if (aType.IsEmpty()) {
    return false;
  }
  for (const auto& perm : kStripOAPermissions) {
    if (perm.Equals(aType)) {
      return true;
    }
  }
  return false;
}

/**
 * Strip origin attributes depending on pref state
 * @param aForceStrip If true, strips user context and private browsing id,
 * ignoring stripping prefs.
 * @param aOriginAttributes object to strip.
 */
void MaybeStripOAs(bool aForceStrip, OriginAttributes& aOriginAttributes) {
  uint32_t flags = 0;

  if (aForceStrip || !StaticPrefs::permissions_isolateBy_privateBrowsing()) {
    flags |= OriginAttributes::STRIP_PRIVATE_BROWSING_ID;
  }

  if (aForceStrip || !StaticPrefs::permissions_isolateBy_userContext()) {
    flags |= OriginAttributes::STRIP_USER_CONTEXT_ID;
  }

  if (flags != 0) {
    aOriginAttributes.StripAttributes(flags);
  }
}

void OriginAppendOASuffix(OriginAttributes aOriginAttributes,
                          bool aForceStripOA, nsACString& aOrigin) {
  MaybeStripOAs(aForceStripOA, aOriginAttributes);

  nsAutoCString oaSuffix;
  aOriginAttributes.CreateSuffix(oaSuffix);
  aOrigin.Append(oaSuffix);
}

nsresult GetOriginFromPrincipal(nsIPrincipal* aPrincipal, bool aForceStripOA,
                                nsACString& aOrigin) {
  nsresult rv = aPrincipal->GetOriginNoSuffix(aOrigin);
  // The principal may belong to the about:blank content viewer, so this can be
  // expected to fail.
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsAutoCString suffix;
  rv = aPrincipal->GetOriginSuffix(suffix);
  NS_ENSURE_SUCCESS(rv, rv);

  mozilla::OriginAttributes attrs;
  if (!attrs.PopulateFromSuffix(suffix)) {
    return NS_ERROR_FAILURE;
  }

  OriginAppendOASuffix(attrs, aForceStripOA, aOrigin);

  return NS_OK;
}

nsresult GetOriginFromURIAndOA(nsIURI* aURI,
                               const OriginAttributes* aOriginAttributes,
                               bool aForceStripOA, nsACString& aOrigin) {
  nsAutoCString origin(aOrigin);
  nsresult rv = ContentPrincipal::GenerateOriginNoSuffixFromURI(aURI, origin);
  NS_ENSURE_SUCCESS(rv, rv);

  OriginAppendOASuffix(*aOriginAttributes, aForceStripOA, origin);

  aOrigin = origin;

  return NS_OK;
}

nsresult GetPrincipalFromOrigin(const nsACString& aOrigin, bool aForceStripOA,
                                nsIPrincipal** aPrincipal) {
  nsAutoCString originNoSuffix;
  mozilla::OriginAttributes attrs;
  if (!attrs.PopulateFromOrigin(aOrigin, originNoSuffix)) {
    return NS_ERROR_FAILURE;
  }

  MaybeStripOAs(aForceStripOA, attrs);

  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), originNoSuffix);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIPrincipal> principal =
      mozilla::BasePrincipal::CreateContentPrincipal(uri, attrs);
  principal.forget(aPrincipal);
  return NS_OK;
}

nsresult GetPrincipal(nsIURI* aURI, bool aIsInIsolatedMozBrowserElement,
                      nsIPrincipal** aPrincipal) {
  mozilla::OriginAttributes attrs(aIsInIsolatedMozBrowserElement);
  nsCOMPtr<nsIPrincipal> principal =
      mozilla::BasePrincipal::CreateContentPrincipal(aURI, attrs);
  NS_ENSURE_TRUE(principal, NS_ERROR_FAILURE);

  principal.forget(aPrincipal);
  return NS_OK;
}

nsresult GetPrincipal(nsIURI* aURI, nsIPrincipal** aPrincipal) {
  mozilla::OriginAttributes attrs;
  nsCOMPtr<nsIPrincipal> principal =
      mozilla::BasePrincipal::CreateContentPrincipal(aURI, attrs);
  NS_ENSURE_TRUE(principal, NS_ERROR_FAILURE);

  principal.forget(aPrincipal);
  return NS_OK;
}

nsCString GetNextSubDomainForHost(const nsACString& aHost) {
  nsCString subDomain;
  nsresult rv =
      nsEffectiveTLDService::GetInstance()->GetNextSubDomain(aHost, subDomain);
  // We can fail if there is no more subdomain or if the host can't have a
  // subdomain.
  if (NS_FAILED(rv)) {
    return EmptyCString();
  }

  return subDomain;
}

// This function produces a nsIURI which is identical to the current
// nsIURI, except that it has one less subdomain segment. It returns
// `nullptr` if there are no more segments to remove.
already_AddRefed<nsIURI> GetNextSubDomainURI(nsIURI* aURI) {
  nsAutoCString host;
  nsresult rv = aURI->GetHost(host);
  if (NS_FAILED(rv)) {
    return nullptr;
  }

  nsCString domain = GetNextSubDomainForHost(host);
  if (domain.IsEmpty()) {
    return nullptr;
  }

  nsCOMPtr<nsIURI> uri;
  rv = NS_MutateURI(aURI).SetHost(domain).Finalize(uri);
  if (NS_FAILED(rv) || !uri) {
    return nullptr;
  }

  return uri.forget();
}

// This function produces a nsIPrincipal which is identical to the current
// nsIPrincipal, except that it has one less subdomain segment. It returns
// `nullptr` if there are no more segments to remove.
already_AddRefed<nsIPrincipal> GetNextSubDomainPrincipal(
    nsIPrincipal* aPrincipal) {
  nsCOMPtr<nsIURI> uri;
  nsresult rv = aPrincipal->GetURI(getter_AddRefs(uri));
  if (NS_FAILED(rv) || !uri) {
    return nullptr;
  }

  // Create a new principal which is identical to the current one, but with the
  // new host
  nsCOMPtr<nsIURI> newURI = GetNextSubDomainURI(uri);
  if (!newURI) {
    return nullptr;
  }

  // Copy the attributes over
  mozilla::OriginAttributes attrs = aPrincipal->OriginAttributesRef();

  if (!StaticPrefs::permissions_isolateBy_userContext()) {
    // Disable userContext for permissions.
    attrs.StripAttributes(mozilla::OriginAttributes::STRIP_USER_CONTEXT_ID);
  }

  nsCOMPtr<nsIPrincipal> principal =
      mozilla::BasePrincipal::CreateContentPrincipal(newURI, attrs);

  return principal.forget();
}

class MOZ_STACK_CLASS UpgradeHostToOriginHelper {
 public:
  virtual nsresult Insert(const nsACString& aOrigin, const nsCString& aType,
                          uint32_t aPermission, uint32_t aExpireType,
                          int64_t aExpireTime, int64_t aModificationTime) = 0;
};

class MOZ_STACK_CLASS UpgradeHostToOriginDBMigration final
    : public UpgradeHostToOriginHelper {
 public:
  UpgradeHostToOriginDBMigration(mozIStorageConnection* aDBConn, int64_t* aID)
      : mDBConn(aDBConn), mID(aID) {
    mDBConn->CreateStatement(
        NS_LITERAL_CSTRING("INSERT INTO moz_hosts_new "
                           "(id, origin, type, permission, expireType, "
                           "expireTime, modificationTime) "
                           "VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7)"),
        getter_AddRefs(mStmt));
  }

  nsresult Insert(const nsACString& aOrigin, const nsCString& aType,
                  uint32_t aPermission, uint32_t aExpireType,
                  int64_t aExpireTime, int64_t aModificationTime) final {
    nsresult rv = mStmt->BindInt64ByIndex(0, *mID);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = mStmt->BindUTF8StringByIndex(1, aOrigin);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = mStmt->BindUTF8StringByIndex(2, aType);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = mStmt->BindInt32ByIndex(3, aPermission);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = mStmt->BindInt32ByIndex(4, aExpireType);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = mStmt->BindInt64ByIndex(5, aExpireTime);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = mStmt->BindInt64ByIndex(6, aModificationTime);
    NS_ENSURE_SUCCESS(rv, rv);

    // Increment the working identifier, as we are about to use this one
    (*mID)++;

    rv = mStmt->Execute();
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
  }

 private:
  nsCOMPtr<mozIStorageStatement> mStmt;
  nsCOMPtr<mozIStorageConnection> mDBConn;
  int64_t* mID;
};

class MOZ_STACK_CLASS UpgradeHostToOriginHostfileImport final
    : public UpgradeHostToOriginHelper {
 public:
  UpgradeHostToOriginHostfileImport(
      nsPermissionManager* aPm, nsPermissionManager::DBOperationType aOperation,
      int64_t aID)
      : mPm(aPm), mOperation(aOperation), mID(aID) {}

  nsresult Insert(const nsACString& aOrigin, const nsCString& aType,
                  uint32_t aPermission, uint32_t aExpireType,
                  int64_t aExpireTime, int64_t aModificationTime) final {
    nsCOMPtr<nsIPrincipal> principal;
    nsresult rv = GetPrincipalFromOrigin(
        aOrigin, IsOAForceStripPermission(aType), getter_AddRefs(principal));
    NS_ENSURE_SUCCESS(rv, rv);

    return mPm->AddInternal(principal, aType, aPermission, mID, aExpireType,
                            aExpireTime, aModificationTime,
                            nsPermissionManager::eDontNotify, mOperation, false,
                            &aOrigin);
  }

 private:
  RefPtr<nsPermissionManager> mPm;
  nsPermissionManager::DBOperationType mOperation;
  int64_t mID;
};

class MOZ_STACK_CLASS UpgradeIPHostToOriginDB final
    : public UpgradeHostToOriginHelper {
 public:
  UpgradeIPHostToOriginDB(mozIStorageConnection* aDBConn, int64_t* aID)
      : mDBConn(aDBConn), mID(aID) {
    mDBConn->CreateStatement(
        NS_LITERAL_CSTRING("INSERT INTO moz_perms"
                           "(id, origin, type, permission, expireType, "
                           "expireTime, modificationTime) "
                           "VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7)"),
        getter_AddRefs(mStmt));

    mDBConn->CreateStatement(
        NS_LITERAL_CSTRING(
            "SELECT id FROM moz_perms WHERE origin = ?1 AND type = ?2"),
        getter_AddRefs(mLookupStmt));
  }

  nsresult Insert(const nsACString& aOrigin, const nsCString& aType,
                  uint32_t aPermission, uint32_t aExpireType,
                  int64_t aExpireTime, int64_t aModificationTime) final {
    // Every time the migration code wants to insert an origin into
    // the database we need to check to see if someone has already
    // created a permissions entry for that permission. If they have,
    // we don't want to insert a duplicate row.
    //
    // We can afford to do this lookup unconditionally and not perform
    // caching, as a origin type pair should only be attempted to be
    // inserted once.

    nsresult rv = mLookupStmt->Reset();
    NS_ENSURE_SUCCESS(rv, rv);

    rv = mLookupStmt->BindUTF8StringByIndex(0, aOrigin);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = mLookupStmt->BindUTF8StringByIndex(1, aType);
    NS_ENSURE_SUCCESS(rv, rv);

    // Check if we already have the row in the database, if we do, then
    // we don't want to be inserting it again.
    bool moreStmts = false;
    if (NS_FAILED(mLookupStmt->ExecuteStep(&moreStmts)) || moreStmts) {
      mLookupStmt->Reset();
      NS_WARNING(
          "A permissions entry was going to be re-migrated, "
          "but was already found in the permissions database.");
      return NS_OK;
    }

    // Actually insert the statement into the database.
    rv = mStmt->BindInt64ByIndex(0, *mID);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = mStmt->BindUTF8StringByIndex(1, aOrigin);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = mStmt->BindUTF8StringByIndex(2, aType);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = mStmt->BindInt32ByIndex(3, aPermission);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = mStmt->BindInt32ByIndex(4, aExpireType);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = mStmt->BindInt64ByIndex(5, aExpireTime);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = mStmt->BindInt64ByIndex(6, aModificationTime);
    NS_ENSURE_SUCCESS(rv, rv);

    // Increment the working identifier, as we are about to use this one
    (*mID)++;

    rv = mStmt->Execute();
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
  }

 private:
  nsCOMPtr<mozIStorageStatement> mStmt;
  nsCOMPtr<mozIStorageStatement> mLookupStmt;
  nsCOMPtr<mozIStorageConnection> mDBConn;
  int64_t* mID;
};

nsresult UpgradeHostToOriginAndInsert(
    const nsACString& aHost, const nsCString& aType, uint32_t aPermission,
    uint32_t aExpireType, int64_t aExpireTime, int64_t aModificationTime,
    bool aIsInIsolatedMozBrowserElement, UpgradeHostToOriginHelper* aHelper) {
  if (aHost.EqualsLiteral("<file>")) {
    // We no longer support the magic host <file>
    NS_WARNING(
        "The magic host <file> is no longer supported. "
        "It is being removed from the permissions database.");
    return NS_OK;
  }

  // First, we check to see if the host is a valid URI. If it is, it can be
  // imported directly
  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), aHost);
  if (NS_SUCCEEDED(rv)) {
    // It was previously possible to insert useless entries to your permissions
    // database for URIs which have a null principal. This acts as a cleanup,
    // getting rid of these useless database entries
    if (uri->SchemeIs("moz-nullprincipal")) {
      NS_WARNING("A moz-nullprincipal: permission is being discarded.");
      return NS_OK;
    }

    nsCOMPtr<nsIPrincipal> principal;
    rv = GetPrincipal(uri, aIsInIsolatedMozBrowserElement,
                      getter_AddRefs(principal));
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoCString origin;
    rv = GetOriginFromPrincipal(principal, IsOAForceStripPermission(aType),
                                origin);
    NS_ENSURE_SUCCESS(rv, rv);

    return aHelper->Insert(origin, aType, aPermission, aExpireType, aExpireTime,
                           aModificationTime);
    return NS_OK;
  }

  // The user may use this host at non-standard ports or protocols, we can use
  // their history to guess what ports and protocols we want to add permissions
  // for. We find every URI which they have visited with this host (or a
  // subdomain of this host), and try to add it as a principal.
  bool foundHistory = false;

  nsCOMPtr<nsINavHistoryService> histSrv =
      do_GetService(NS_NAVHISTORYSERVICE_CONTRACTID);

  if (histSrv) {
    nsCOMPtr<nsINavHistoryQuery> histQuery;
    rv = histSrv->GetNewQuery(getter_AddRefs(histQuery));
    NS_ENSURE_SUCCESS(rv, rv);

    // Get the eTLD+1 of the domain
    nsAutoCString eTLD1;
    rv = nsEffectiveTLDService::GetInstance()->GetBaseDomainFromHost(aHost, 0,
                                                                     eTLD1);

    if (NS_FAILED(rv)) {
      // If the lookup on the tldService for the base domain for the host
      // failed, that means that we just want to directly use the host as the
      // host name for the lookup.
      eTLD1 = aHost;
    }

    // We want to only find history items for this particular eTLD+1, and
    // subdomains
    rv = histQuery->SetDomain(eTLD1);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = histQuery->SetDomainIsHost(false);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsINavHistoryQueryOptions> histQueryOpts;
    rv = histSrv->GetNewQueryOptions(getter_AddRefs(histQueryOpts));
    NS_ENSURE_SUCCESS(rv, rv);

    // We want to get the URIs for every item in the user's history with the
    // given host
    rv =
        histQueryOpts->SetResultType(nsINavHistoryQueryOptions::RESULTS_AS_URI);
    NS_ENSURE_SUCCESS(rv, rv);

    // We only search history, because searching both bookmarks and history
    // is not supported, and history tends to be more comprehensive.
    rv = histQueryOpts->SetQueryType(
        nsINavHistoryQueryOptions::QUERY_TYPE_HISTORY);
    NS_ENSURE_SUCCESS(rv, rv);

    // We include hidden URIs (such as those visited via iFrames) as they may
    // have permissions too
    rv = histQueryOpts->SetIncludeHidden(true);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsINavHistoryResult> histResult;
    rv = histSrv->ExecuteQuery(histQuery, histQueryOpts,
                               getter_AddRefs(histResult));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsINavHistoryContainerResultNode> histResultContainer;
    rv = histResult->GetRoot(getter_AddRefs(histResultContainer));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = histResultContainer->SetContainerOpen(true);
    NS_ENSURE_SUCCESS(rv, rv);

    uint32_t childCount = 0;
    rv = histResultContainer->GetChildCount(&childCount);
    NS_ENSURE_SUCCESS(rv, rv);

    nsTHashtable<nsCStringHashKey> insertedOrigins;
    for (uint32_t i = 0; i < childCount; i++) {
      nsCOMPtr<nsINavHistoryResultNode> child;
      histResultContainer->GetChild(i, getter_AddRefs(child));
      if (NS_WARN_IF(NS_FAILED(rv))) continue;

      uint32_t type;
      rv = child->GetType(&type);
      if (NS_WARN_IF(NS_FAILED(rv)) ||
          type != nsINavHistoryResultNode::RESULT_TYPE_URI) {
        NS_WARNING(
            "Unexpected non-RESULT_TYPE_URI node in "
            "UpgradeHostToOriginAndInsert()");
        continue;
      }

      nsAutoCString uriSpec;
      rv = child->GetUri(uriSpec);
      if (NS_WARN_IF(NS_FAILED(rv))) continue;

      nsCOMPtr<nsIURI> uri;
      rv = NS_NewURI(getter_AddRefs(uri), uriSpec);
      if (NS_WARN_IF(NS_FAILED(rv))) continue;

      // Use the provided host - this URI may be for a subdomain, rather than
      // the host we care about.
      rv = NS_MutateURI(uri).SetHost(aHost).Finalize(uri);
      if (NS_WARN_IF(NS_FAILED(rv))) continue;

      // We now have a URI which we can make a nsIPrincipal out of
      nsCOMPtr<nsIPrincipal> principal;
      rv = GetPrincipal(uri, aIsInIsolatedMozBrowserElement,
                        getter_AddRefs(principal));
      if (NS_WARN_IF(NS_FAILED(rv))) continue;

      nsAutoCString origin;
      rv = GetOriginFromPrincipal(principal, IsOAForceStripPermission(aType),
                                  origin);
      if (NS_WARN_IF(NS_FAILED(rv))) continue;

      // Ensure that we don't insert the same origin repeatedly
      if (insertedOrigins.Contains(origin)) {
        continue;
      }

      foundHistory = true;
      rv = aHelper->Insert(origin, aType, aPermission, aExpireType, aExpireTime,
                           aModificationTime);
      NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "Insert failed");
      insertedOrigins.PutEntry(origin);
    }

    rv = histResultContainer->SetContainerOpen(false);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // If we didn't find any origins for this host in the poermissions database,
  // we can insert the default http:// and https:// permissions into the
  // database. This has a relatively high likelihood of applying the permission
  // to the correct origin.
  if (!foundHistory) {
    nsAutoCString hostSegment;
    nsCOMPtr<nsIPrincipal> principal;
    nsAutoCString origin;

    // If this is an ipv6 URI, we need to surround it in '[', ']' before trying
    // to parse it as a URI.
    if (aHost.FindChar(':') != -1) {
      hostSegment.AssignLiteral("[");
      hostSegment.Append(aHost);
      hostSegment.AppendLiteral("]");
    } else {
      hostSegment.Assign(aHost);
    }

    // http:// URI default
    rv = NS_NewURI(getter_AddRefs(uri),
                   NS_LITERAL_CSTRING("http://") + hostSegment);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = GetPrincipal(uri, aIsInIsolatedMozBrowserElement,
                      getter_AddRefs(principal));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = GetOriginFromPrincipal(principal, IsOAForceStripPermission(aType),
                                origin);
    NS_ENSURE_SUCCESS(rv, rv);

    aHelper->Insert(origin, aType, aPermission, aExpireType, aExpireTime,
                    aModificationTime);

    // https:// URI default
    rv = NS_NewURI(getter_AddRefs(uri),
                   NS_LITERAL_CSTRING("https://") + hostSegment);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = GetPrincipal(uri, aIsInIsolatedMozBrowserElement,
                      getter_AddRefs(principal));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = GetOriginFromPrincipal(principal, IsOAForceStripPermission(aType),
                                origin);
    NS_ENSURE_SUCCESS(rv, rv);

    aHelper->Insert(origin, aType, aPermission, aExpireType, aExpireTime,
                    aModificationTime);
  }

  return NS_OK;
}

static bool IsExpandedPrincipal(nsIPrincipal* aPrincipal) {
  nsCOMPtr<nsIExpandedPrincipal> ep = do_QueryInterface(aPrincipal);
  return !!ep;
}

// We only want to persist permissions which don't have session or policy
// expiration.
static bool IsPersistentExpire(uint32_t aExpire, const nsACString& aType) {
  bool res = (aExpire != nsIPermissionManager::EXPIRE_SESSION &&
              aExpire != nsIPermissionManager::EXPIRE_POLICY);
#ifdef ANDROID
  for (const auto& perm : kGeckoViewRestrictedPermissions) {
    res = res && !perm.Equals(aType);
  }
#endif
  return res;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////

nsPermissionManager::PermissionKey*
nsPermissionManager::PermissionKey::CreateFromPrincipal(
    nsIPrincipal* aPrincipal, bool aForceStripOA, nsresult& aResult) {
  nsAutoCString origin;
  aResult = GetOriginFromPrincipal(aPrincipal, aForceStripOA, origin);
  if (NS_WARN_IF(NS_FAILED(aResult))) {
    return nullptr;
  }

  return new PermissionKey(origin);
}

nsPermissionManager::PermissionKey*
nsPermissionManager::PermissionKey::CreateFromURIAndOriginAttributes(
    nsIURI* aURI, const OriginAttributes* aOriginAttributes, bool aForceStripOA,
    nsresult& aResult) {
  nsAutoCString origin;
  aResult =
      GetOriginFromURIAndOA(aURI, aOriginAttributes, aForceStripOA, origin);
  if (NS_WARN_IF(NS_FAILED(aResult))) {
    return nullptr;
  }

  return new PermissionKey(origin);
}

nsPermissionManager::PermissionKey*
nsPermissionManager::PermissionKey::CreateFromURI(nsIURI* aURI,
                                                  nsresult& aResult) {
  nsAutoCString origin;
  aResult = ContentPrincipal::GenerateOriginNoSuffixFromURI(aURI, origin);
  if (NS_WARN_IF(NS_FAILED(aResult))) {
    return nullptr;
  }

  return new PermissionKey(origin);
}

/**
 * Simple callback used by |AsyncClose| to trigger a treatment once
 * the database is closed.
 *
 * Note: Beware that, if you hold onto a |CloseDatabaseListener| from a
 * |nsPermissionManager|, this will create a cycle.
 *
 * Note: Once the callback has been called this DeleteFromMozHostListener cannot
 * be reused.
 */
class CloseDatabaseListener final : public mozIStorageCompletionCallback {
  ~CloseDatabaseListener() {}

 public:
  NS_DECL_ISUPPORTS
  NS_DECL_MOZISTORAGECOMPLETIONCALLBACK
  /**
   * @param aManager The owning manager.
   * @param aRebuildOnSuccess If |true|, reinitialize the database once
   * it has been closed. Otherwise, do nothing such.
   */
  CloseDatabaseListener(nsPermissionManager* aManager, bool aRebuildOnSuccess);

 protected:
  RefPtr<nsPermissionManager> mManager;
  bool mRebuildOnSuccess;
};

NS_IMPL_ISUPPORTS(CloseDatabaseListener, mozIStorageCompletionCallback)

CloseDatabaseListener::CloseDatabaseListener(nsPermissionManager* aManager,
                                             bool aRebuildOnSuccess)
    : mManager(aManager), mRebuildOnSuccess(aRebuildOnSuccess) {}

NS_IMETHODIMP
CloseDatabaseListener::Complete(nsresult, nsISupports*) {
  // Help breaking cycles
  RefPtr<nsPermissionManager> manager = std::move(mManager);
  if (mRebuildOnSuccess && !gIsShuttingDown) {
    return manager->InitDB(true);
  }
  return NS_OK;
}

/**
 * Simple callback used by |RemoveAllInternal| to trigger closing
 * the database and reinitializing it.
 *
 * Note: Beware that, if you hold onto a |DeleteFromMozHostListener| from a
 * |nsPermissionManager|, this will create a cycle.
 *
 * Note: Once the callback has been called this DeleteFromMozHostListener cannot
 * be reused.
 */
class DeleteFromMozHostListener final : public mozIStorageStatementCallback {
  ~DeleteFromMozHostListener() {}

 public:
  NS_DECL_ISUPPORTS
  NS_DECL_MOZISTORAGESTATEMENTCALLBACK

  /**
   * @param aManager The owning manager.
   */
  explicit DeleteFromMozHostListener(nsPermissionManager* aManager);

 protected:
  RefPtr<nsPermissionManager> mManager;
};

NS_IMPL_ISUPPORTS(DeleteFromMozHostListener, mozIStorageStatementCallback)

DeleteFromMozHostListener::DeleteFromMozHostListener(
    nsPermissionManager* aManager)
    : mManager(aManager) {}

NS_IMETHODIMP DeleteFromMozHostListener::HandleResult(mozIStorageResultSet*) {
  MOZ_CRASH("Should not get any results");
}

NS_IMETHODIMP DeleteFromMozHostListener::HandleError(mozIStorageError*) {
  // Errors are handled in |HandleCompletion|
  return NS_OK;
}

NS_IMETHODIMP DeleteFromMozHostListener::HandleCompletion(uint16_t aReason) {
  // Help breaking cycles
  RefPtr<nsPermissionManager> manager = std::move(mManager);

  if (aReason == REASON_ERROR) {
    manager->CloseDB(true);
  }

  return NS_OK;
}

/* static */
void nsPermissionManager::Startup() {
  nsCOMPtr<nsIPermissionManager> permManager =
      do_GetService("@mozilla.org/permissionmanager;1");
}

////////////////////////////////////////////////////////////////////////////////
// nsPermissionManager Implementation

#define PERMISSIONS_FILE_NAME "permissions.sqlite"
#define HOSTS_SCHEMA_VERSION 11

// Default permissions are read from a URL - this is the preference we read
// to find that URL. If not set, don't use any default permissions.
static const char kDefaultsUrlPrefName[] = "permissions.manager.defaultsUrl";

static const char kPermissionChangeNotification[] = PERM_CHANGE_NOTIFICATION;

NS_IMPL_ISUPPORTS(nsPermissionManager, nsIPermissionManager, nsIObserver,
                  nsISupportsWeakReference)

nsPermissionManager::nsPermissionManager()
    : mMemoryOnlyDB(false), mLargestID(0) {}

nsPermissionManager::~nsPermissionManager() {
  // NOTE: Make sure to reject each of the promises in mPermissionKeyPromiseMap
  // before destroying.
  for (auto iter = mPermissionKeyPromiseMap.Iter(); !iter.Done(); iter.Next()) {
    if (iter.Data()) {
      iter.Data()->Reject(NS_ERROR_FAILURE, __func__);
    }
  }
  mPermissionKeyPromiseMap.Clear();

  RemoveAllFromMemory();
  if (gPermissionManager) {
    MOZ_ASSERT(gPermissionManager == this);
    gPermissionManager = nullptr;
  }
}

// static
already_AddRefed<nsIPermissionManager>
nsPermissionManager::GetXPCOMSingleton() {
  if (gPermissionManager) {
    return do_AddRef(gPermissionManager);
  }

  if (gIsShuttingDown) {
    return nullptr;
  }

  // Create a new singleton nsPermissionManager.
  // We AddRef only once since XPCOM has rules about the ordering of module
  // teardowns - by the time our module destructor is called, it's too late to
  // Release our members, since GC cycles have already been completed and
  // would result in serious leaks.
  // See bug 209571.
  auto permManager = MakeRefPtr<nsPermissionManager>();
  if (NS_SUCCEEDED(permManager->Init())) {
    // Note: This is cleared in the nsPermissionManager destructor.
    gPermissionManager = permManager.get();
    ClearOnShutdown(&gPermissionManager);
    return permManager.forget();
  }

  return nullptr;
}

// static
nsPermissionManager* nsPermissionManager::GetInstance() {
  if (!gPermissionManager) {
    // Hand off the creation of the permission manager to GetXPCOMSingleton.
    nsCOMPtr<nsIPermissionManager> permManager = GetXPCOMSingleton();
  }

  return gPermissionManager;
}

nsresult nsPermissionManager::Init() {
  // If the 'permissions.memory_only' pref is set to true, then don't write any
  // permission settings to disk, but keep them in a memory-only database.
  mMemoryOnlyDB =
      mozilla::Preferences::GetBool("permissions.memory_only", false);

  nsresult rv;
  nsCOMPtr<nsIPrefService> prefService =
      do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = prefService->GetBranch("permissions.default.",
                              getter_AddRefs(mDefaultPrefBranch));
  NS_ENSURE_SUCCESS(rv, rv);

  if (IsChildProcess()) {
    // Stop here; we don't need the DB in the child process. Instead we will be
    // sent permissions as we need them by our parent process.
    return NS_OK;
  }

  nsCOMPtr<nsIObserverService> observerService =
      mozilla::services::GetObserverService();
  if (observerService) {
    observerService->AddObserver(this, "profile-before-change", true);
    observerService->AddObserver(this, "profile-do-change", true);
    observerService->AddObserver(this, "testonly-reload-permissions-from-disk",
                                 true);
  }

  // ignore failure here, since it's non-fatal (we can run fine without
  // persistent storage - e.g. if there's no profile).
  // XXX should we tell the user about this?
  InitDB(false);

  return NS_OK;
}

nsresult nsPermissionManager::OpenDatabase(nsIFile* aPermissionsFile) {
  nsresult rv;
  nsCOMPtr<mozIStorageService> storage =
      do_GetService(MOZ_STORAGE_SERVICE_CONTRACTID);
  if (!storage) {
    return NS_ERROR_UNEXPECTED;
  }
  // cache a connection to the hosts database
  if (mMemoryOnlyDB) {
    rv = storage->OpenSpecialDatabase("memory", getter_AddRefs(mDBConn));
  } else {
    rv = storage->OpenDatabase(aPermissionsFile, getter_AddRefs(mDBConn));
  }
  return rv;
}

nsresult nsPermissionManager::InitDB(bool aRemoveFile) {
  nsCOMPtr<nsIFile> permissionsFile;
  nsresult rv = NS_GetSpecialDirectory(NS_APP_PERMISSION_PARENT_DIR,
                                       getter_AddRefs(permissionsFile));
  if (NS_FAILED(rv)) {
    rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
                                getter_AddRefs(permissionsFile));
    if (NS_FAILED(rv)) {
      return NS_ERROR_UNEXPECTED;
    }
  }

  rv = permissionsFile->AppendNative(NS_LITERAL_CSTRING(PERMISSIONS_FILE_NAME));
  NS_ENSURE_SUCCESS(rv, rv);

  if (aRemoveFile) {
    bool exists = false;
    rv = permissionsFile->Exists(&exists);
    NS_ENSURE_SUCCESS(rv, rv);
    if (exists) {
      rv = permissionsFile->Remove(false);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  rv = OpenDatabase(permissionsFile);
  if (rv == NS_ERROR_FILE_CORRUPTED) {
    LogToConsole(
        NS_LITERAL_STRING("permissions.sqlite is corrupted! Try again!"));

    // Add telemetry probe
    mozilla::Telemetry::Accumulate(
        mozilla::Telemetry::PERMISSIONS_SQL_CORRUPTED, 1);

    // delete corrupted permissions.sqlite and try again
    rv = permissionsFile->Remove(false);
    NS_ENSURE_SUCCESS(rv, rv);
    LogToConsole(
        NS_LITERAL_STRING("Corrupted permissions.sqlite has been removed."));

    rv = OpenDatabase(permissionsFile);
    NS_ENSURE_SUCCESS(rv, rv);
    LogToConsole(
        NS_LITERAL_STRING("OpenDatabase to permissions.sqlite is successful!"));
  } else if (NS_FAILED(rv)) {
    return rv;
  }

  bool ready;
  mDBConn->GetConnectionReady(&ready);
  if (!ready) {
    LogToConsole(NS_LITERAL_STRING(
        "Fail to get connection to permissions.sqlite! Try again!"));

    // delete and try again
    rv = permissionsFile->Remove(false);
    NS_ENSURE_SUCCESS(rv, rv);
    LogToConsole(
        NS_LITERAL_STRING("Defective permissions.sqlite has been removed."));

    // Add telemetry probe
    mozilla::Telemetry::Accumulate(
        mozilla::Telemetry::DEFECTIVE_PERMISSIONS_SQL_REMOVED, 1);

    rv = OpenDatabase(permissionsFile);
    NS_ENSURE_SUCCESS(rv, rv);
    LogToConsole(
        NS_LITERAL_STRING("OpenDatabase to permissions.sqlite is successful!"));

    mDBConn->GetConnectionReady(&ready);
    if (!ready) return NS_ERROR_UNEXPECTED;
  }

  bool tableExists = false;
  mDBConn->TableExists(NS_LITERAL_CSTRING("moz_perms"), &tableExists);
  if (!tableExists) {
    mDBConn->TableExists(NS_LITERAL_CSTRING("moz_hosts"), &tableExists);
  }
  if (!tableExists) {
    rv = CreateTable();
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    // table already exists; check the schema version before reading
    int32_t dbSchemaVersion;
    rv = mDBConn->GetSchemaVersion(&dbSchemaVersion);
    NS_ENSURE_SUCCESS(rv, rv);

    switch (dbSchemaVersion) {
        // upgrading.
        // every time you increment the database schema, you need to implement
        // the upgrading code from the previous version to the new one.
        // fall through to current version

      case 1: {
        // previous non-expiry version of database.  Upgrade it by adding the
        // expiration columns
        rv = mDBConn->ExecuteSimpleSQL(
            NS_LITERAL_CSTRING("ALTER TABLE moz_hosts ADD expireType INTEGER"));
        NS_ENSURE_SUCCESS(rv, rv);

        rv = mDBConn->ExecuteSimpleSQL(
            NS_LITERAL_CSTRING("ALTER TABLE moz_hosts ADD expireTime INTEGER"));
        NS_ENSURE_SUCCESS(rv, rv);
      }

        // fall through to the next upgrade
        [[fallthrough]];

      // TODO: we want to make default version as version 2 in order to fix bug
      // 784875.
      case 0:
      case 2: {
        // Add appId/isInBrowserElement fields.
        rv = mDBConn->ExecuteSimpleSQL(
            NS_LITERAL_CSTRING("ALTER TABLE moz_hosts ADD appId INTEGER"));
        NS_ENSURE_SUCCESS(rv, rv);

        rv = mDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
            "ALTER TABLE moz_hosts ADD isInBrowserElement INTEGER"));
        NS_ENSURE_SUCCESS(rv, rv);

        rv = mDBConn->SetSchemaVersion(3);
        NS_ENSURE_SUCCESS(rv, rv);
      }

        // fall through to the next upgrade
        [[fallthrough]];

      // Version 3->4 is the creation of the modificationTime field.
      case 3: {
        rv = mDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
            "ALTER TABLE moz_hosts ADD modificationTime INTEGER"));
        NS_ENSURE_SUCCESS(rv, rv);

        // We leave the modificationTime at zero for all existing records; using
        // now() would mean, eg, that doing "remove all from the last hour"
        // within the first hour after migration would remove all permissions.

        rv = mDBConn->SetSchemaVersion(4);
        NS_ENSURE_SUCCESS(rv, rv);
      }

        // fall through to the next upgrade
        [[fallthrough]];

      // In version 5, host appId, and isInBrowserElement were merged into a
      // single origin entry
      //
      // In version 6, the tables were renamed for backwards compatability
      // reasons with version 4 and earlier.
      //
      // In version 7, a bug in the migration used for version 4->5 was
      // discovered which could have triggered data-loss. Because of that, all
      // users with a version 4, 5, or 6 database will be re-migrated from the
      // backup database. (bug 1186034). This migration bug is not present after
      // bug 1185340, and the re-migration ensures that all users have the fix.
      case 5:
        // This branch could also be reached via dbSchemaVersion == 3, in which
        // case we want to fall through to the dbSchemaVersion == 4 case. The
        // easiest way to do that is to perform this extra check here to make
        // sure that we didn't get here via a fallthrough from v3
        if (dbSchemaVersion == 5) {
          // In version 5, the backup database is named moz_hosts_v4. We perform
          // the version 5->6 migration to get the tables to have consistent
          // naming conventions.

          // Version 5->6 is the renaming of moz_hosts to moz_perms, and
          // moz_hosts_v4 to moz_hosts (bug 1185343)
          //
          // In version 5, we performed the modifications to the permissions
          // database in place, this meant that if you upgraded to a version
          // which used V5, and then downgraded to a version which used v4 or
          // earlier, the fallback path would drop the table, and your
          // permissions data would be lost. This migration undoes that mistake,
          // by restoring the old moz_hosts table (if it was present), and
          // instead using the new table moz_perms for the new permissions
          // schema.
          //
          // NOTE: If you downgrade, store new permissions, and then upgrade
          // again, these new permissions won't be migrated or reflected in the
          // updated database. This migration only occurs once, as if moz_perms
          // exists, it will skip creating it. In addition, permissions added
          // after the migration will not be visible in previous versions of
          // firefox.

          bool permsTableExists = false;
          mDBConn->TableExists(NS_LITERAL_CSTRING("moz_perms"),
                               &permsTableExists);
          if (!permsTableExists) {
            // Move the upgraded database to moz_perms
            rv = mDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
                "ALTER TABLE moz_hosts RENAME TO moz_perms"));
            NS_ENSURE_SUCCESS(rv, rv);
          } else {
            NS_WARNING(
                "moz_hosts was not renamed to moz_perms, "
                "as a moz_perms table already exists");

            // In the situation where a moz_perms table already exists, but the
            // schema is lower than 6, a migration has already previously
            // occured to V6, but a downgrade has caused the moz_hosts table to
            // be dropped. This should only occur in the case of a downgrade to
            // a V5 database, which was only present in a few day's nightlies.
            // As that version was likely used only on a temporary basis, we
            // assume that the database from the previous V6 has the permissions
            // which the user actually wants to use. We have to get rid of
            // moz_hosts such that moz_hosts_v4 can be moved into its place if
            // it exists.
            rv = mDBConn->ExecuteSimpleSQL(
                NS_LITERAL_CSTRING("DROP TABLE moz_hosts"));
            NS_ENSURE_SUCCESS(rv, rv);
          }

#ifdef DEBUG
          // The moz_hosts table shouldn't exist anymore
          bool hostsTableExists = false;
          mDBConn->TableExists(NS_LITERAL_CSTRING("moz_hosts"),
                               &hostsTableExists);
          MOZ_ASSERT(!hostsTableExists);
#endif

          // Rename moz_hosts_v4 back to it's original location, if it exists
          bool v4TableExists = false;
          mDBConn->TableExists(NS_LITERAL_CSTRING("moz_hosts_v4"),
                               &v4TableExists);
          if (v4TableExists) {
            rv = mDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
                "ALTER TABLE moz_hosts_v4 RENAME TO moz_hosts"));
            NS_ENSURE_SUCCESS(rv, rv);
          }

          rv = mDBConn->SetSchemaVersion(6);
          NS_ENSURE_SUCCESS(rv, rv);
        }

        // fall through to the next upgrade
        [[fallthrough]];

      // At this point, the version 5 table has been migrated to a version 6
      // table We are guaranteed to have at least one of moz_hosts and
      // moz_perms. If we have moz_hosts, we will migrate moz_hosts into
      // moz_perms (even if we already have a moz_perms, as we need a
      // re-migration due to bug 1186034).
      //
      // After this migration, we are guaranteed to have both a moz_hosts (for
      // backwards compatability), and a moz_perms table. The moz_hosts table
      // will have a v4 schema, and the moz_perms table will have a v6 schema.
      case 4:
      case 6: {
        bool hostsTableExists = false;
        mDBConn->TableExists(NS_LITERAL_CSTRING("moz_hosts"),
                             &hostsTableExists);
        if (hostsTableExists) {
          // Both versions 4 and 6 have a version 4 formatted hosts table named
          // moz_hosts. We can migrate this table to our version 7 table
          // moz_perms. If moz_perms is present, then we can use it as a basis
          // for comparison.

          rv = mDBConn->BeginTransaction();
          NS_ENSURE_SUCCESS(rv, rv);

          bool tableExists = false;
          mDBConn->TableExists(NS_LITERAL_CSTRING("moz_hosts_new"),
                               &tableExists);
          if (tableExists) {
            NS_WARNING(
                "The temporary database moz_hosts_new already exists, dropping "
                "it.");
            rv = mDBConn->ExecuteSimpleSQL(
                NS_LITERAL_CSTRING("DROP TABLE moz_hosts_new"));
            NS_ENSURE_SUCCESS(rv, rv);
          }
          rv = mDBConn->ExecuteSimpleSQL(
              NS_LITERAL_CSTRING("CREATE TABLE moz_hosts_new ("
                                 " id INTEGER PRIMARY KEY"
                                 ",origin TEXT"
                                 ",type TEXT"
                                 ",permission INTEGER"
                                 ",expireType INTEGER"
                                 ",expireTime INTEGER"
                                 ",modificationTime INTEGER"
                                 ")"));
          NS_ENSURE_SUCCESS(rv, rv);

          nsCOMPtr<mozIStorageStatement> stmt;
          rv = mDBConn->CreateStatement(
              NS_LITERAL_CSTRING(
                  "SELECT host, type, permission, expireType, expireTime, "
                  "modificationTime, isInBrowserElement FROM moz_hosts"),
              getter_AddRefs(stmt));
          NS_ENSURE_SUCCESS(rv, rv);

          int64_t id = 0;
          nsAutoCString host, type;
          uint32_t permission;
          uint32_t expireType;
          int64_t expireTime;
          int64_t modificationTime;
          bool isInBrowserElement;
          bool hasResult;

          while (NS_SUCCEEDED(stmt->ExecuteStep(&hasResult)) && hasResult) {
            // Read in the old row
            rv = stmt->GetUTF8String(0, host);
            if (NS_WARN_IF(NS_FAILED(rv))) {
              continue;
            }
            rv = stmt->GetUTF8String(1, type);
            if (NS_WARN_IF(NS_FAILED(rv))) {
              continue;
            }
            permission = stmt->AsInt32(2);
            expireType = stmt->AsInt32(3);
            expireTime = stmt->AsInt64(4);
            modificationTime = stmt->AsInt64(5);
            isInBrowserElement = static_cast<bool>(stmt->AsInt32(6));

            // Perform the meat of the migration by deferring to the
            // UpgradeHostToOriginAndInsert function.
            UpgradeHostToOriginDBMigration upHelper(mDBConn, &id);
            rv = UpgradeHostToOriginAndInsert(
                host, type, permission, expireType, expireTime,
                modificationTime, isInBrowserElement, &upHelper);
            if (NS_FAILED(rv)) {
              NS_WARNING(
                  "Unexpected failure when upgrading migrating permission "
                  "from host to origin");
            }
          }

          // We don't drop the moz_hosts table such that it is available for
          // backwards-compatability and for future migrations in case of
          // migration errors in the current code.
          // Create a marker empty table which will indicate that the moz_hosts
          // table is intended to act as a backup. If this table is not present,
          // then the moz_hosts table was created as a random empty table.
          rv = mDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
              "CREATE TABLE moz_hosts_is_backup (dummy INTEGER PRIMARY KEY)"));
          NS_ENSURE_SUCCESS(rv, rv);

          bool permsTableExists = false;
          mDBConn->TableExists(NS_LITERAL_CSTRING("moz_perms"),
                               &permsTableExists);
          if (permsTableExists) {
            // The user already had a moz_perms table, and we are performing a
            // re-migration. We count the rows in the old table for telemetry,
            // and then back up their old database as moz_perms_v6

            nsCOMPtr<mozIStorageStatement> countStmt;
            rv = mDBConn->CreateStatement(
                NS_LITERAL_CSTRING("SELECT COUNT(*) FROM moz_perms"),
                getter_AddRefs(countStmt));
            bool hasResult = false;
            if (NS_FAILED(rv) ||
                NS_FAILED(countStmt->ExecuteStep(&hasResult)) || !hasResult) {
              NS_WARNING("Could not count the rows in moz_perms");
            }

            // Back up the old moz_perms database as moz_perms_v6 before we
            // move the new table into its position
            rv = mDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
                "ALTER TABLE moz_perms RENAME TO moz_perms_v6"));
            NS_ENSURE_SUCCESS(rv, rv);
          }

          rv = mDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
              "ALTER TABLE moz_hosts_new RENAME TO moz_perms"));
          NS_ENSURE_SUCCESS(rv, rv);

          rv = mDBConn->CommitTransaction();
          NS_ENSURE_SUCCESS(rv, rv);
        } else {
          // We don't have a moz_hosts table, so we create one for downgrading
          // purposes. This table is empty.
          rv = mDBConn->ExecuteSimpleSQL(
              NS_LITERAL_CSTRING("CREATE TABLE moz_hosts ("
                                 " id INTEGER PRIMARY KEY"
                                 ",host TEXT"
                                 ",type TEXT"
                                 ",permission INTEGER"
                                 ",expireType INTEGER"
                                 ",expireTime INTEGER"
                                 ",modificationTime INTEGER"
                                 ",appId INTEGER"
                                 ",isInBrowserElement INTEGER"
                                 ")"));
          NS_ENSURE_SUCCESS(rv, rv);

          // We are guaranteed to have a moz_perms table at this point.
        }

#ifdef DEBUG
        {
          // At this point, both the moz_hosts and moz_perms tables should exist
          bool hostsTableExists = false;
          bool permsTableExists = false;
          mDBConn->TableExists(NS_LITERAL_CSTRING("moz_hosts"),
                               &hostsTableExists);
          mDBConn->TableExists(NS_LITERAL_CSTRING("moz_perms"),
                               &permsTableExists);
          MOZ_ASSERT(hostsTableExists && permsTableExists);
        }
#endif

        rv = mDBConn->SetSchemaVersion(7);
        NS_ENSURE_SUCCESS(rv, rv);
      }

        // fall through to the next upgrade
        [[fallthrough]];

      // The version 7-8 migration is the re-migration of localhost and
      // ip-address entries due to errors in the previous version 7 migration
      // which caused localhost and ip-address entries to be incorrectly
      // discarded. The version 7 migration logic has been corrected, and thus
      // this logic only needs to execute if the user is currently on version 7.
      case 7: {
        // This migration will be relatively expensive as we need to perform
        // database lookups for each origin which we want to insert.
        // Fortunately, it shouldn't be too expensive as we only want to insert
        // a small number of entries created for localhost or IP addresses.

        // We only want to perform the re-migration if moz_hosts is a backup
        bool hostsIsBackupExists = false;
        mDBConn->TableExists(NS_LITERAL_CSTRING("moz_hosts_is_backup"),
                             &hostsIsBackupExists);

        // Only perform this migration if the original schema version was 7, and
        // the moz_hosts table is a backup.
        if (dbSchemaVersion == 7 && hostsIsBackupExists) {
          nsCOMPtr<mozIStorageStatement> stmt;
          rv = mDBConn->CreateStatement(
              NS_LITERAL_CSTRING(
                  "SELECT host, type, permission, expireType, expireTime, "
                  "modificationTime, isInBrowserElement FROM moz_hosts"),
              getter_AddRefs(stmt));
          NS_ENSURE_SUCCESS(rv, rv);

          nsCOMPtr<mozIStorageStatement> idStmt;
          rv = mDBConn->CreateStatement(
              NS_LITERAL_CSTRING("SELECT MAX(id) FROM moz_hosts"),
              getter_AddRefs(idStmt));
          int64_t id = 0;
          bool hasResult = false;
          if (NS_SUCCEEDED(rv) &&
              NS_SUCCEEDED(idStmt->ExecuteStep(&hasResult)) && hasResult) {
            id = idStmt->AsInt32(0) + 1;
          }

          nsAutoCString host, type;
          uint32_t permission;
          uint32_t expireType;
          int64_t expireTime;
          int64_t modificationTime;
          bool isInBrowserElement;

          while (NS_SUCCEEDED(stmt->ExecuteStep(&hasResult)) && hasResult) {
            // Read in the old row
            rv = stmt->GetUTF8String(0, host);
            if (NS_WARN_IF(NS_FAILED(rv))) {
              continue;
            }

            nsAutoCString eTLD1;
            rv = nsEffectiveTLDService::GetInstance()->GetBaseDomainFromHost(
                host, 0, eTLD1);
            if (NS_SUCCEEDED(rv)) {
              // We only care about entries which the tldService can't handle
              continue;
            }

            rv = stmt->GetUTF8String(1, type);
            if (NS_WARN_IF(NS_FAILED(rv))) {
              continue;
            }
            permission = stmt->AsInt32(2);
            expireType = stmt->AsInt32(3);
            expireTime = stmt->AsInt64(4);
            modificationTime = stmt->AsInt64(5);
            isInBrowserElement = static_cast<bool>(stmt->AsInt32(6));

            // Perform the meat of the migration by deferring to the
            // UpgradeHostToOriginAndInsert function.
            UpgradeIPHostToOriginDB upHelper(mDBConn, &id);
            rv = UpgradeHostToOriginAndInsert(
                host, type, permission, expireType, expireTime,
                modificationTime, isInBrowserElement, &upHelper);
            if (NS_FAILED(rv)) {
              NS_WARNING(
                  "Unexpected failure when upgrading migrating permission "
                  "from host to origin");
            }
          }
        }

        // Even if we didn't perform the migration, we want to bump the schema
        // version to 8.
        rv = mDBConn->SetSchemaVersion(8);
        NS_ENSURE_SUCCESS(rv, rv);
      }

        // fall through to the next upgrade
        [[fallthrough]];

      // The version 8-9 migration removes the unnecessary backup moz-hosts
      // database contents. as the data no longer needs to be migrated
      case 8: {
        // We only want to clear out the old table if it is a backup. If it
        // isn't a backup, we don't need to touch it.
        bool hostsIsBackupExists = false;
        mDBConn->TableExists(NS_LITERAL_CSTRING("moz_hosts_is_backup"),
                             &hostsIsBackupExists);
        if (hostsIsBackupExists) {
          // Delete everything from the backup, we want to keep around the table
          // so that you can still downgrade and not break things, but we don't
          // need to keep the rows around.
          rv = mDBConn->ExecuteSimpleSQL(
              NS_LITERAL_CSTRING("DELETE FROM moz_hosts"));
          NS_ENSURE_SUCCESS(rv, rv);

          // The table is no longer a backup, so get rid of it.
          rv = mDBConn->ExecuteSimpleSQL(
              NS_LITERAL_CSTRING("DROP TABLE moz_hosts_is_backup"));
          NS_ENSURE_SUCCESS(rv, rv);
        }

        rv = mDBConn->SetSchemaVersion(9);
        NS_ENSURE_SUCCESS(rv, rv);
      }

        // fall through to the next upgrade
        [[fallthrough]];

      case 9: {
        rv = mDBConn->SetSchemaVersion(10);
        NS_ENSURE_SUCCESS(rv, rv);
      }

        // fall through to the next upgrade
        [[fallthrough]];

      case 10: {
        // Filter out the rows with storage access API permissions with a
        // granted origin, and remove the granted origin part from the
        // permission type.
        rv = mDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
            "UPDATE moz_perms "
            "SET type=SUBSTR(type, 0, INSTR(SUBSTR(type, INSTR(type, '^') + "
            "1), '^') + INSTR(type, '^')) "
            "WHERE INSTR(SUBSTR(type, INSTR(type, '^') + 1), '^') AND "
            "SUBSTR(type, 0, 18) == \"storageAccessAPI^\";"));
        NS_ENSURE_SUCCESS(rv, rv);

        rv = mDBConn->SetSchemaVersion(HOSTS_SCHEMA_VERSION);
        NS_ENSURE_SUCCESS(rv, rv);
      }

        // fall through to the next upgrade
        [[fallthrough]];

      // current version.
      case HOSTS_SCHEMA_VERSION:
        break;

      // downgrading.
      // if columns have been added to the table, we can still use the ones we
      // understand safely. if columns have been deleted or altered, just
      // blow away the table and start from scratch! if you change the way
      // a column is interpreted, make sure you also change its name so this
      // check will catch it.
      default: {
        // check if all the expected columns exist
        nsCOMPtr<mozIStorageStatement> stmt;
        rv = mDBConn->CreateStatement(
            NS_LITERAL_CSTRING(
                "SELECT origin, type, permission, expireType, expireTime, "
                "modificationTime FROM moz_perms"),
            getter_AddRefs(stmt));
        if (NS_SUCCEEDED(rv)) break;

        // our columns aren't there - drop the table!
        rv = mDBConn->ExecuteSimpleSQL(
            NS_LITERAL_CSTRING("DROP TABLE moz_perms"));
        NS_ENSURE_SUCCESS(rv, rv);

        rv = CreateTable();
        NS_ENSURE_SUCCESS(rv, rv);
      } break;
    }
  }

  // cache frequently used statements (for insertion, deletion, and updating)
  rv = mDBConn->CreateAsyncStatement(
      NS_LITERAL_CSTRING("INSERT INTO moz_perms "
                         "(id, origin, type, permission, expireType, "
                         "expireTime, modificationTime) "
                         "VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7)"),
      getter_AddRefs(mStmtInsert));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mDBConn->CreateAsyncStatement(NS_LITERAL_CSTRING("DELETE FROM moz_perms "
                                                        "WHERE id = ?1"),
                                     getter_AddRefs(mStmtDelete));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mDBConn->CreateAsyncStatement(
      NS_LITERAL_CSTRING("UPDATE moz_perms "
                         "SET permission = ?2, expireType= ?3, expireTime = "
                         "?4, modificationTime = ?5 WHERE id = ?1"),
      getter_AddRefs(mStmtUpdate));
  NS_ENSURE_SUCCESS(rv, rv);

  // Always import default permissions.
  ImportDefaults();
  // check whether to import or just read in the db
  if (tableExists) {
    rv = Read();
    NS_ENSURE_SUCCESS(rv, rv);

    AddIdleDailyMaintenanceJob();
  }

  return NS_OK;
}

void nsPermissionManager::AddIdleDailyMaintenanceJob() {
  nsCOMPtr<nsIObserverService> observerService =
      mozilla::services::GetObserverService();
  NS_ENSURE_TRUE_VOID(observerService);

  nsresult rv =
      observerService->AddObserver(this, OBSERVER_TOPIC_IDLE_DAILY, false);
  NS_ENSURE_SUCCESS_VOID(rv);
}

void nsPermissionManager::RemoveIdleDailyMaintenanceJob() {
  nsCOMPtr<nsIObserverService> observerService =
      mozilla::services::GetObserverService();
  NS_ENSURE_TRUE_VOID(observerService);

  nsresult rv =
      observerService->RemoveObserver(this, OBSERVER_TOPIC_IDLE_DAILY);
  NS_ENSURE_SUCCESS_VOID(rv);
}

void nsPermissionManager::PerformIdleDailyMaintenance() {
  if (!mDBConn) {
    return;
  }

  nsCOMPtr<mozIStorageAsyncStatement> stmtDeleteExpired;
  nsresult rv = mDBConn->CreateAsyncStatement(
      NS_LITERAL_CSTRING("DELETE FROM moz_perms WHERE expireType = "
                         "?1 AND expireTime <= ?2"),
      getter_AddRefs(stmtDeleteExpired));
  NS_ENSURE_SUCCESS_VOID(rv);

  rv =
      stmtDeleteExpired->BindInt32ByIndex(0, nsIPermissionManager::EXPIRE_TIME);
  NS_ENSURE_SUCCESS_VOID(rv);

  rv = stmtDeleteExpired->BindInt64ByIndex(1, EXPIRY_NOW);
  NS_ENSURE_SUCCESS_VOID(rv);

  nsCOMPtr<mozIStoragePendingStatement> pending;
  rv = stmtDeleteExpired->ExecuteAsync(nullptr, getter_AddRefs(pending));
  NS_ENSURE_SUCCESS_VOID(rv);
}

// sets the schema version and creates the moz_perms table.
nsresult nsPermissionManager::CreateTable() {
  // set the schema version, before creating the table
  nsresult rv = mDBConn->SetSchemaVersion(HOSTS_SCHEMA_VERSION);
  if (NS_FAILED(rv)) return rv;

  // create the table
  // SQL also lives in automation.py.in. If you change this SQL change that
  // one too
  rv =
      mDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING("CREATE TABLE moz_perms ("
                                                   " id INTEGER PRIMARY KEY"
                                                   ",origin TEXT"
                                                   ",type TEXT"
                                                   ",permission INTEGER"
                                                   ",expireType INTEGER"
                                                   ",expireTime INTEGER"
                                                   ",modificationTime INTEGER"
                                                   ")"));
  if (NS_FAILED(rv)) return rv;

  // We also create a legacy V4 table, for backwards compatability,
  // and to ensure that downgrades don't trigger a schema version change.
  return mDBConn->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("CREATE TABLE moz_hosts ("
                         " id INTEGER PRIMARY KEY"
                         ",host TEXT"
                         ",type TEXT"
                         ",permission INTEGER"
                         ",expireType INTEGER"
                         ",expireTime INTEGER"
                         ",modificationTime INTEGER"
                         ",isInBrowserElement INTEGER"
                         ")"));
}

NS_IMETHODIMP
nsPermissionManager::AddFromPrincipal(nsIPrincipal* aPrincipal,
                                      const nsACString& aType,
                                      uint32_t aPermission,
                                      uint32_t aExpireType,
                                      int64_t aExpireTime) {
  ENSURE_NOT_CHILD_PROCESS;
  NS_ENSURE_ARG_POINTER(aPrincipal);
  NS_ENSURE_TRUE(aExpireType == nsIPermissionManager::EXPIRE_NEVER ||
                     aExpireType == nsIPermissionManager::EXPIRE_TIME ||
                     aExpireType == nsIPermissionManager::EXPIRE_SESSION ||
                     aExpireType == nsIPermissionManager::EXPIRE_POLICY,
                 NS_ERROR_INVALID_ARG);

  // Skip addition if the permission is already expired. Note that
  // EXPIRE_SESSION only honors expireTime if it is nonzero.
  if ((aExpireType == nsIPermissionManager::EXPIRE_TIME ||
       (aExpireType == nsIPermissionManager::EXPIRE_SESSION &&
        aExpireTime != 0)) &&
      aExpireTime <= EXPIRY_NOW) {
    return NS_OK;
  }

  // We don't add the system principal because it actually has no URI and we
  // always allow action for them.
  if (aPrincipal->IsSystemPrincipal()) {
    return NS_OK;
  }

  // Null principals can't meaningfully have persisted permissions attached to
  // them, so we don't allow adding permissions for them.
  if (aPrincipal->GetIsNullPrincipal()) {
    return NS_OK;
  }

  // Permissions may not be added to expanded principals.
  if (IsExpandedPrincipal(aPrincipal)) {
    return NS_ERROR_INVALID_ARG;
  }

  // A modificationTime of zero will cause AddInternal to use now().
  int64_t modificationTime = 0;

  return AddInternal(aPrincipal, aType, aPermission, 0, aExpireType,
                     aExpireTime, modificationTime, eNotify, eWriteToDB);
}

nsresult nsPermissionManager::AddInternal(
    nsIPrincipal* aPrincipal, const nsACString& aType, uint32_t aPermission,
    int64_t aID, uint32_t aExpireType, int64_t aExpireTime,
    int64_t aModificationTime, NotifyOperationType aNotifyOperation,
    DBOperationType aDBOperation, const bool aIgnoreSessionPermissions,
    const nsACString* aOriginString) {
  nsresult rv = NS_OK;
  nsAutoCString origin;
  // Only attempt to compute the origin string when it is going to be needed
  // later on in the function.
  if (!IsChildProcess() ||
      (aDBOperation == eWriteToDB && IsPersistentExpire(aExpireType, aType))) {
    if (aOriginString) {
      // Use the origin string provided by the caller.
      origin = *aOriginString;
    } else {
      // Compute it from the principal provided.
      rv = GetOriginFromPrincipal(aPrincipal, IsOAForceStripPermission(aType),
                                  origin);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  // For private browsing only store permissions for the session
  if (aExpireType != EXPIRE_SESSION) {
    uint32_t privateBrowsingId =
        nsScriptSecurityManager::DEFAULT_PRIVATE_BROWSING_ID;
    nsresult rv = aPrincipal->GetPrivateBrowsingId(&privateBrowsingId);
    if (NS_SUCCEEDED(rv) &&
        privateBrowsingId !=
            nsScriptSecurityManager::DEFAULT_PRIVATE_BROWSING_ID) {
      aExpireType = EXPIRE_SESSION;
    }
  }

  if (!IsChildProcess()) {
    IPC::Permission permission(origin, aType, aPermission, aExpireType,
                               aExpireTime);

    nsAutoCString permissionKey;
    GetKeyForPermission(aPrincipal, aType, permissionKey);

    nsTArray<ContentParent*> cplist;
    ContentParent::GetAll(cplist);
    for (uint32_t i = 0; i < cplist.Length(); ++i) {
      ContentParent* cp = cplist[i];
      if (cp->NeedsPermissionsUpdate(permissionKey))
        Unused << cp->SendAddPermission(permission);
    }
  }

  MOZ_ASSERT(PermissionAvailable(aPrincipal, aType));

  // look up the type index
  int32_t typeIndex = GetTypeIndex(aType, true);
  NS_ENSURE_TRUE(typeIndex != -1, NS_ERROR_OUT_OF_MEMORY);

  // When an entry already exists, PutEntry will return that, instead
  // of adding a new one
  RefPtr<PermissionKey> key = PermissionKey::CreateFromPrincipal(
      aPrincipal, IsOAForceStripPermission(aType), rv);
  if (!key) {
    MOZ_ASSERT(NS_FAILED(rv));
    return rv;
  }

  PermissionHashKey* entry = mPermissionTable.PutEntry(key);
  if (!entry) return NS_ERROR_FAILURE;
  if (!entry->GetKey()) {
    mPermissionTable.RemoveEntry(entry);
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // figure out the transaction type, and get any existing permission value
  OperationType op;
  int32_t index = entry->GetPermissionIndex(typeIndex);
  if (index == -1) {
    if (aPermission == nsIPermissionManager::UNKNOWN_ACTION)
      op = eOperationNone;
    else
      op = eOperationAdding;

  } else {
    PermissionEntry oldPermissionEntry = entry->GetPermissions()[index];

    // remove the permission if the permission is UNKNOWN, update the
    // permission if its value or expire type have changed OR if the time has
    // changed and the expire type is time, otherwise, don't modify.  There's
    // no need to modify a permission that doesn't expire with time when the
    // only thing changed is the expire time.
    if (aPermission == oldPermissionEntry.mPermission &&
        aExpireType == oldPermissionEntry.mExpireType &&
        (aExpireType == nsIPermissionManager::EXPIRE_NEVER ||
         aExpireTime == oldPermissionEntry.mExpireTime))
      op = eOperationNone;
    else if (oldPermissionEntry.mID == cIDPermissionIsDefault)
      // The existing permission is one added as a default and the new
      // permission doesn't exactly match so we are replacing the default.  This
      // is true even if the new permission is UNKNOWN_ACTION (which means a
      // "logical remove" of the default)
      op = eOperationReplacingDefault;
    else if (aID == cIDPermissionIsDefault)
      // We are adding a default permission but a "real" permission already
      // exists.  This almost-certainly means we just did a removeAllSince and
      // are re-importing defaults - so we can ignore this.
      op = eOperationNone;
    else if (aPermission == nsIPermissionManager::UNKNOWN_ACTION)
      op = eOperationRemoving;
    else
      op = eOperationChanging;
  }

  // child processes should *always* be passed a modificationTime of zero.
  MOZ_ASSERT(!IsChildProcess() || aModificationTime == 0);

  // do the work for adding, deleting, or changing a permission:
  // update the in-memory list, write to the db, and notify consumers.
  int64_t id;
  if (aModificationTime == 0) {
    aModificationTime = EXPIRY_NOW;
  }

  switch (op) {
    case eOperationNone: {
      // nothing to do
      return NS_OK;
    }

    case eOperationAdding: {
      if (aDBOperation == eWriteToDB) {
        // we'll be writing to the database - generate a known unique id
        id = ++mLargestID;
      } else {
        // we're reading from the database - use the id already assigned
        id = aID;
      }

      entry->GetPermissions().AppendElement(
          PermissionEntry(id, typeIndex, aPermission, aExpireType, aExpireTime,
                          aModificationTime));

      if (aDBOperation == eWriteToDB &&
          IsPersistentExpire(aExpireType, aType)) {
        UpdateDB(op, mStmtInsert, id, origin, aType, aPermission, aExpireType,
                 aExpireTime, aModificationTime);
      }

      if (aNotifyOperation == eNotify) {
        NotifyObserversWithPermission(aPrincipal, mTypeArray[typeIndex],
                                      aPermission, aExpireType, aExpireTime,
                                      aModificationTime, u"added");
      }

      break;
    }

    case eOperationRemoving: {
      PermissionEntry oldPermissionEntry = entry->GetPermissions()[index];
      id = oldPermissionEntry.mID;

      // If the type we want to remove is EXPIRE_POLICY, we need to reject
      // attempts to change the permission.
      if (entry->GetPermissions()[index].mExpireType == EXPIRE_POLICY) {
        NS_WARNING("Attempting to remove EXPIRE_POLICY permission");
        break;
      }

      entry->GetPermissions().RemoveElementAt(index);

      if (aDBOperation == eWriteToDB)
        // We care only about the id here so we pass dummy values for all other
        // parameters.
        UpdateDB(op, mStmtDelete, id, EmptyCString(), EmptyCString(), 0,
                 nsIPermissionManager::EXPIRE_NEVER, 0, 0);

      if (aNotifyOperation == eNotify) {
        NotifyObserversWithPermission(
            aPrincipal, mTypeArray[typeIndex], oldPermissionEntry.mPermission,
            oldPermissionEntry.mExpireType, oldPermissionEntry.mExpireTime,
            oldPermissionEntry.mModificationTime, u"deleted");
      }

      // If there are no more permissions stored for that entry, clear it.
      if (entry->GetPermissions().IsEmpty()) {
        mPermissionTable.RemoveEntry(entry);
      }

      break;
    }

    case eOperationChanging: {
      id = entry->GetPermissions()[index].mID;

      // If the existing type is EXPIRE_POLICY, we need to reject attempts to
      // change the permission.
      if (entry->GetPermissions()[index].mExpireType == EXPIRE_POLICY) {
        NS_WARNING("Attempting to modify EXPIRE_POLICY permission");
        break;
      }

      // If the new expireType is EXPIRE_SESSION, then we have to keep a
      // copy of the previous permission/expireType values. This cached value
      // will be used when restoring the permissions of an app.
      if (entry->GetPermissions()[index].mExpireType !=
              nsIPermissionManager::EXPIRE_SESSION &&
          aExpireType == nsIPermissionManager::EXPIRE_SESSION) {
        entry->GetPermissions()[index].mNonSessionPermission =
            entry->GetPermissions()[index].mPermission;
        entry->GetPermissions()[index].mNonSessionExpireType =
            entry->GetPermissions()[index].mExpireType;
        entry->GetPermissions()[index].mNonSessionExpireTime =
            entry->GetPermissions()[index].mExpireTime;
      } else if (aExpireType != nsIPermissionManager::EXPIRE_SESSION) {
        entry->GetPermissions()[index].mNonSessionPermission = aPermission;
        entry->GetPermissions()[index].mNonSessionExpireType = aExpireType;
        entry->GetPermissions()[index].mNonSessionExpireTime = aExpireTime;
      }

      entry->GetPermissions()[index].mPermission = aPermission;
      entry->GetPermissions()[index].mExpireType = aExpireType;
      entry->GetPermissions()[index].mExpireTime = aExpireTime;
      entry->GetPermissions()[index].mModificationTime = aModificationTime;

      if (aDBOperation == eWriteToDB &&
          IsPersistentExpire(aExpireType, aType)) {
        // We care only about the id, the permission and
        // expireType/expireTime/modificationTime here. We pass dummy values for
        // all other parameters.
        UpdateDB(op, mStmtUpdate, id, EmptyCString(), EmptyCString(),
                 aPermission, aExpireType, aExpireTime, aModificationTime);
      }

      if (aNotifyOperation == eNotify) {
        NotifyObserversWithPermission(aPrincipal, mTypeArray[typeIndex],
                                      aPermission, aExpireType, aExpireTime,
                                      aModificationTime, u"changed");
      }

      break;
    }
    case eOperationReplacingDefault: {
      // this is handling the case when we have an existing permission
      // entry that was created as a "default" (and thus isn't in the DB) with
      // an explicit permission (that may include UNKNOWN_ACTION.)
      // Note we will *not* get here if we are replacing an already replaced
      // default value - that is handled as eOperationChanging.

      // So this is a hybrid of eOperationAdding (as we are writing a new entry
      // to the DB) and eOperationChanging (as we are replacing the in-memory
      // repr and sending a "changed" notification).

      // We want a new ID even if not writing to the DB, so the modified entry
      // in memory doesn't have the magic cIDPermissionIsDefault value.
      id = ++mLargestID;

      // The default permission being replaced can't have session expiry or
      // policy expiry.
      NS_ENSURE_TRUE(entry->GetPermissions()[index].mExpireType !=
                         nsIPermissionManager::EXPIRE_SESSION,
                     NS_ERROR_UNEXPECTED);
      NS_ENSURE_TRUE(entry->GetPermissions()[index].mExpireType !=
                         nsIPermissionManager::EXPIRE_POLICY,
                     NS_ERROR_UNEXPECTED);
      // We don't support the new entry having any expiry - supporting that
      // would make things far more complex and none of the permissions we set
      // as a default support that.
      NS_ENSURE_TRUE(aExpireType == EXPIRE_NEVER, NS_ERROR_UNEXPECTED);

      // update the existing entry in memory.
      entry->GetPermissions()[index].mID = id;
      entry->GetPermissions()[index].mPermission = aPermission;
      entry->GetPermissions()[index].mExpireType = aExpireType;
      entry->GetPermissions()[index].mExpireTime = aExpireTime;
      entry->GetPermissions()[index].mModificationTime = aModificationTime;

      // If requested, create the entry in the DB.
      if (aDBOperation == eWriteToDB &&
          IsPersistentExpire(aExpireType, aType)) {
        UpdateDB(eOperationAdding, mStmtInsert, id, origin, aType, aPermission,
                 aExpireType, aExpireTime, aModificationTime);
      }

      if (aNotifyOperation == eNotify) {
        NotifyObserversWithPermission(aPrincipal, mTypeArray[typeIndex],
                                      aPermission, aExpireType, aExpireTime,
                                      aModificationTime, u"changed");
      }

    } break;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsPermissionManager::RemoveFromPrincipal(nsIPrincipal* aPrincipal,
                                         const nsACString& aType) {
  ENSURE_NOT_CHILD_PROCESS;
  NS_ENSURE_ARG_POINTER(aPrincipal);

  // System principals are never added to the database, no need to remove them.
  if (aPrincipal->IsSystemPrincipal()) {
    return NS_OK;
  }

  // Permissions may not be added to expanded principals.
  if (IsExpandedPrincipal(aPrincipal)) {
    return NS_ERROR_INVALID_ARG;
  }

  // AddInternal() handles removal, just let it do the work
  return AddInternal(aPrincipal, aType, nsIPermissionManager::UNKNOWN_ACTION, 0,
                     nsIPermissionManager::EXPIRE_NEVER, 0, 0, eNotify,
                     eWriteToDB);
}

NS_IMETHODIMP
nsPermissionManager::RemovePermission(nsIPermission* aPerm) {
  if (!aPerm) {
    return NS_OK;
  }
  nsCOMPtr<nsIPrincipal> principal;
  nsresult rv = aPerm->GetPrincipal(getter_AddRefs(principal));
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString type;
  rv = aPerm->GetType(type);
  NS_ENSURE_SUCCESS(rv, rv);

  // Permissions are uniquely identified by their principal and type.
  // We remove the permission using these two pieces of data.
  return RemoveFromPrincipal(principal, type);
}

NS_IMETHODIMP
nsPermissionManager::RemoveAll() {
  ENSURE_NOT_CHILD_PROCESS;
  return RemoveAllInternal(true);
}

NS_IMETHODIMP
nsPermissionManager::RemoveAllSince(int64_t aSince) {
  ENSURE_NOT_CHILD_PROCESS;
  return RemoveAllModifiedSince(aSince);
}

template <class T>
nsresult nsPermissionManager::RemovePermissionEntries(T aCondition) {
  Vector<Tuple<nsCOMPtr<nsIPrincipal>, nsCString, nsCString>, 10> array;
  for (auto iter = mPermissionTable.Iter(); !iter.Done(); iter.Next()) {
    PermissionHashKey* entry = iter.Get();
    for (const auto& permEntry : entry->GetPermissions()) {
      if (!aCondition(permEntry)) {
        continue;
      }

      nsCOMPtr<nsIPrincipal> principal;
      nsresult rv = GetPrincipalFromOrigin(
          entry->GetKey()->mOrigin,
          IsOAForceStripPermission(mTypeArray[permEntry.mType]),
          getter_AddRefs(principal));
      if (NS_FAILED(rv)) {
        continue;
      }

      if (!array.emplaceBack(principal, mTypeArray[permEntry.mType],
                             entry->GetKey()->mOrigin)) {
        continue;
      }
    }
  }

  for (auto& i : array) {
    // AddInternal handles removal, so let it do the work...
    AddInternal(Get<0>(i), Get<1>(i), nsIPermissionManager::UNKNOWN_ACTION, 0,
                nsIPermissionManager::EXPIRE_NEVER, 0, 0,
                nsPermissionManager::eNotify, nsPermissionManager::eWriteToDB,
                false, &Get<2>(i));
  }
  // now re-import any defaults as they may now be required if we just deleted
  // an override.
  ImportDefaults();
  return NS_OK;
}

NS_IMETHODIMP
nsPermissionManager::RemoveByType(const nsACString& aType) {
  ENSURE_NOT_CHILD_PROCESS;

  int32_t typeIndex = GetTypeIndex(aType, false);
  // If type == -1, the type isn't known,
  // so just return NS_OK
  if (typeIndex == -1) {
    return NS_OK;
  }

  return RemovePermissionEntries(
      [typeIndex](const PermissionEntry& aPermEntry) {
        return static_cast<uint32_t>(typeIndex) == aPermEntry.mType;
      });
}

NS_IMETHODIMP
nsPermissionManager::RemoveByTypeSince(const nsACString& aType,
                                       int64_t aModificationTime) {
  ENSURE_NOT_CHILD_PROCESS;

  int32_t typeIndex = GetTypeIndex(aType, false);
  // If type == -1, the type isn't known,
  // so just return NS_OK
  if (typeIndex == -1) {
    return NS_OK;
  }

  return RemovePermissionEntries(
      [typeIndex, aModificationTime](const PermissionEntry& aPermEntry) {
        return uint32_t(typeIndex) == aPermEntry.mType &&
               aModificationTime <= aPermEntry.mModificationTime;
      });
}

void nsPermissionManager::CloseDB(bool aRebuildOnSuccess) {
  // Null the statements, this will finalize them.
  mStmtInsert = nullptr;
  mStmtDelete = nullptr;
  mStmtUpdate = nullptr;
  if (mDBConn) {
    mozIStorageCompletionCallback* cb =
        new CloseDatabaseListener(this, aRebuildOnSuccess);
    mozilla::DebugOnly<nsresult> rv = mDBConn->AsyncClose(cb);
    MOZ_ASSERT(NS_SUCCEEDED(rv));
    mDBConn = nullptr;  // Avoid race conditions
  }
}

nsresult nsPermissionManager::RemoveAllFromIPC() {
  MOZ_ASSERT(IsChildProcess());

  // Remove from memory and notify immediately. Since the in-memory
  // database is authoritative, we do not need confirmation from the
  // on-disk database to notify observers.
  RemoveAllFromMemory();

  return NS_OK;
}

nsresult nsPermissionManager::RemoveAllInternal(bool aNotifyObservers) {
  ENSURE_NOT_CHILD_PROCESS;

  // Let's broadcast the removeAll() to any content process.
  nsTArray<ContentParent*> parents;
  ContentParent::GetAll(parents);
  for (ContentParent* parent : parents) {
    Unused << parent->SendRemoveAllPermissions();
  }

  // Remove from memory and notify immediately. Since the in-memory
  // database is authoritative, we do not need confirmation from the
  // on-disk database to notify observers.
  RemoveAllFromMemory();

  // Re-import the defaults
  ImportDefaults();

  if (aNotifyObservers) {
    NotifyObservers(nullptr, u"cleared");
  }

  // clear the db
  if (mDBConn) {
    nsCOMPtr<mozIStorageAsyncStatement> removeStmt;
    nsresult rv = mDBConn->CreateAsyncStatement(
        NS_LITERAL_CSTRING("DELETE FROM moz_perms"),
        getter_AddRefs(removeStmt));
    MOZ_ASSERT(NS_SUCCEEDED(rv));
    if (!removeStmt) {
      return NS_ERROR_UNEXPECTED;
    }
    nsCOMPtr<mozIStoragePendingStatement> pending;
    mozIStorageStatementCallback* cb = new DeleteFromMozHostListener(this);
    rv = removeStmt->ExecuteAsync(cb, getter_AddRefs(pending));
    MOZ_ASSERT(NS_SUCCEEDED(rv));

    return rv;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsPermissionManager::TestExactPermissionFromPrincipal(nsIPrincipal* aPrincipal,
                                                      const nsACString& aType,
                                                      uint32_t* aPermission) {
  return CommonTestPermission(aPrincipal, -1, aType, aPermission,
                              nsIPermissionManager::UNKNOWN_ACTION, false, true,
                              true);
}

NS_IMETHODIMP
nsPermissionManager::TestExactPermanentPermission(nsIPrincipal* aPrincipal,
                                                  const nsACString& aType,
                                                  uint32_t* aPermission) {
  return CommonTestPermission(aPrincipal, -1, aType, aPermission,
                              nsIPermissionManager::UNKNOWN_ACTION, false, true,
                              false);
}

nsresult nsPermissionManager::LegacyTestPermissionFromURI(
    nsIURI* aURI, const mozilla::OriginAttributes* aOriginAttributes,
    const nsACString& aType, uint32_t* aPermission) {
  return CommonTestPermission(aURI, aOriginAttributes, -1, aType, aPermission,
                              nsIPermissionManager::UNKNOWN_ACTION, false,
                              false, true);
}

NS_IMETHODIMP
nsPermissionManager::TestPermissionFromPrincipal(nsIPrincipal* aPrincipal,
                                                 const nsACString& aType,
                                                 uint32_t* aPermission) {
  return CommonTestPermission(aPrincipal, -1, aType, aPermission,
                              nsIPermissionManager::UNKNOWN_ACTION, false,
                              false, true);
}

NS_IMETHODIMP
nsPermissionManager::GetPermissionObject(nsIPrincipal* aPrincipal,
                                         const nsACString& aType,
                                         bool aExactHostMatch,
                                         nsIPermission** aResult) {
  NS_ENSURE_ARG_POINTER(aPrincipal);

  *aResult = nullptr;

  if (aPrincipal->IsSystemPrincipal()) {
    return NS_OK;
  }

  // Querying the permission object of an nsEP is non-sensical.
  if (IsExpandedPrincipal(aPrincipal)) {
    return NS_ERROR_INVALID_ARG;
  }

  MOZ_ASSERT(PermissionAvailable(aPrincipal, aType));

  int32_t typeIndex = GetTypeIndex(aType, false);
  // If type == -1, the type isn't known,
  // so just return NS_OK
  if (typeIndex == -1) return NS_OK;

  PermissionHashKey* entry =
      GetPermissionHashKey(aPrincipal, typeIndex, aExactHostMatch);
  if (!entry) {
    return NS_OK;
  }

  // We don't call GetPermission(typeIndex) because that returns a fake
  // UNKNOWN_ACTION entry if there is no match.
  int32_t idx = entry->GetPermissionIndex(typeIndex);
  if (-1 == idx) {
    return NS_OK;
  }

  nsCOMPtr<nsIPrincipal> principal;
  nsresult rv = GetPrincipalFromOrigin(entry->GetKey()->mOrigin,
                                       IsOAForceStripPermission(aType),
                                       getter_AddRefs(principal));
  NS_ENSURE_SUCCESS(rv, rv);

  PermissionEntry& perm = entry->GetPermissions()[idx];
  nsCOMPtr<nsIPermission> r = nsPermission::Create(
      principal, mTypeArray[perm.mType], perm.mPermission, perm.mExpireType,
      perm.mExpireTime, perm.mModificationTime);
  if (NS_WARN_IF(!r)) {
    return NS_ERROR_FAILURE;
  }
  r.forget(aResult);
  return NS_OK;
}

nsresult nsPermissionManager::CommonTestPermissionInternal(
    nsIPrincipal* aPrincipal, nsIURI* aURI,
    const OriginAttributes* aOriginAttributes, int32_t aTypeIndex,
    const nsACString& aType, uint32_t* aPermission, bool aExactHostMatch,
    bool aIncludingSession) {
  MOZ_ASSERT(aPrincipal || aURI);
  NS_ENSURE_ARG_POINTER(aPrincipal || aURI);
  MOZ_ASSERT_IF(aPrincipal, !aURI && !aOriginAttributes);
  MOZ_ASSERT_IF(aURI || aOriginAttributes, !aPrincipal);

#ifdef DEBUG
  {
    nsCOMPtr<nsIPrincipal> prin = aPrincipal;
    if (!prin) {
      if (aURI) {
        prin = mozilla::BasePrincipal::CreateContentPrincipal(
            aURI, OriginAttributes());
      }
    }
    MOZ_ASSERT(prin);
    MOZ_ASSERT(PermissionAvailable(prin, aType));
  }
#endif

  PermissionHashKey* entry =
      aPrincipal ? GetPermissionHashKey(aPrincipal, aTypeIndex, aExactHostMatch)
                 : GetPermissionHashKey(aURI, aOriginAttributes, aTypeIndex,
                                        aExactHostMatch);
  if (!entry || (!aIncludingSession &&
                 entry->GetPermission(aTypeIndex).mNonSessionExpireType ==
                     nsIPermissionManager::EXPIRE_SESSION)) {
    return NS_OK;
  }

  *aPermission = aIncludingSession
                     ? entry->GetPermission(aTypeIndex).mPermission
                     : entry->GetPermission(aTypeIndex).mNonSessionPermission;

  return NS_OK;
}

// Returns PermissionHashKey for a given { host, isInBrowserElement }
// tuple. This is not simply using PermissionKey because we will walk-up domains
// in case of |host| contains sub-domains. Returns null if nothing found. Also
// accepts host on the format "<foo>". This will perform an exact match lookup
// as the string doesn't contain any dots.
nsPermissionManager::PermissionHashKey*
nsPermissionManager::GetPermissionHashKey(nsIPrincipal* aPrincipal,
                                          uint32_t aType,
                                          bool aExactHostMatch) {
  MOZ_ASSERT(PermissionAvailable(aPrincipal, mTypeArray[aType]));

  nsresult rv;
  RefPtr<PermissionKey> key = PermissionKey::CreateFromPrincipal(
      aPrincipal, IsOAForceStripPermission(mTypeArray[aType]), rv);
  if (!key) {
    return nullptr;
  }

  PermissionHashKey* entry = mPermissionTable.GetEntry(key);

  if (entry) {
    PermissionEntry permEntry = entry->GetPermission(aType);

    // if the entry is expired, remove and keep looking for others.
    // Note that EXPIRE_SESSION only honors expireTime if it is nonzero.
    if ((permEntry.mExpireType == nsIPermissionManager::EXPIRE_TIME ||
         (permEntry.mExpireType == nsIPermissionManager::EXPIRE_SESSION &&
          permEntry.mExpireTime != 0)) &&
        permEntry.mExpireTime <= EXPIRY_NOW) {
      entry = nullptr;
      RemoveFromPrincipal(aPrincipal, mTypeArray[aType]);
    } else if (permEntry.mPermission == nsIPermissionManager::UNKNOWN_ACTION) {
      entry = nullptr;
    }
  }

  if (entry) {
    return entry;
  }

  // If aExactHostMatch wasn't true, we can check if the base domain has a
  // permission entry.
  if (!aExactHostMatch) {
    nsCOMPtr<nsIPrincipal> principal = GetNextSubDomainPrincipal(aPrincipal);
    if (principal) {
      return GetPermissionHashKey(principal, aType, aExactHostMatch);
    }
  }

  // No entry, really...
  return nullptr;
}

// Returns PermissionHashKey for a given { host, isInBrowserElement }
// tuple. This is not simply using PermissionKey because we will walk-up domains
// in case of |host| contains sub-domains. Returns null if nothing found. Also
// accepts host on the format "<foo>". This will perform an exact match lookup
// as the string doesn't contain any dots.
nsPermissionManager::PermissionHashKey*
nsPermissionManager::GetPermissionHashKey(
    nsIURI* aURI, const OriginAttributes* aOriginAttributes, uint32_t aType,
    bool aExactHostMatch) {
  MOZ_ASSERT(aURI);

#ifdef DEBUG
  {
    nsCOMPtr<nsIPrincipal> principal;
    nsresult rv = NS_OK;
    if (aURI) {
      rv = GetPrincipal(aURI, getter_AddRefs(principal));
    }
    MOZ_ASSERT_IF(NS_SUCCEEDED(rv),
                  PermissionAvailable(principal, mTypeArray[aType]));
  }
#endif

  nsresult rv;
  RefPtr<PermissionKey> key;

  if (aOriginAttributes) {
    key = PermissionKey::CreateFromURIAndOriginAttributes(
        aURI, aOriginAttributes, IsOAForceStripPermission(mTypeArray[aType]),
        rv);
  } else {
    key = PermissionKey::CreateFromURI(aURI, rv);
  }

  if (!key) {
    return nullptr;
  }

  PermissionHashKey* entry = mPermissionTable.GetEntry(key);

  if (entry) {
    PermissionEntry permEntry = entry->GetPermission(aType);

    // if the entry is expired, remove and keep looking for others.
    // Note that EXPIRE_SESSION only honors expireTime if it is nonzero.
    if ((permEntry.mExpireType == nsIPermissionManager::EXPIRE_TIME ||
         (permEntry.mExpireType == nsIPermissionManager::EXPIRE_SESSION &&
          permEntry.mExpireTime != 0)) &&
        permEntry.mExpireTime <= EXPIRY_NOW) {
      entry = nullptr;
      // If we need to remove a permission we mint a principal.  This is a bit
      // inefficient, but hopefully this code path isn't super common.
      nsCOMPtr<nsIPrincipal> principal;
      if (aURI) {
        nsresult rv = GetPrincipal(aURI, getter_AddRefs(principal));
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return nullptr;
        }
      }
      RemoveFromPrincipal(principal, mTypeArray[aType]);
    } else if (permEntry.mPermission == nsIPermissionManager::UNKNOWN_ACTION) {
      entry = nullptr;
    }
  }

  if (entry) {
    return entry;
  }

  // If aExactHostMatch wasn't true, we can check if the base domain has a
  // permission entry.
  if (!aExactHostMatch) {
    nsCOMPtr<nsIURI> uri;
    if (aURI) {
      uri = GetNextSubDomainURI(aURI);
    }
    if (uri) {
      return GetPermissionHashKey(uri, aOriginAttributes, aType,
                                  aExactHostMatch);
    }
  }

  // No entry, really...
  return nullptr;
}

NS_IMETHODIMP nsPermissionManager::GetAll(
    nsTArray<RefPtr<nsIPermission>>& aResult) {
  return GetAllWithTypePrefix(NS_LITERAL_CSTRING(""), aResult);
}

NS_IMETHODIMP nsPermissionManager::GetAllWithTypePrefix(
    const nsACString& aPrefix, nsTArray<RefPtr<nsIPermission>>& aResult) {
  aResult.Clear();
  if (XRE_IsContentProcess()) {
    NS_WARNING(
        "nsPermissionManager's getAllWithTypePrefix is not available in the "
        "content process, as not all permissions may be available.");
    return NS_ERROR_NOT_AVAILABLE;
  }

  for (auto iter = mPermissionTable.Iter(); !iter.Done(); iter.Next()) {
    PermissionHashKey* entry = iter.Get();
    for (const auto& permEntry : entry->GetPermissions()) {
      // Given how "default" permissions work and the possibility of them being
      // overridden with UNKNOWN_ACTION, we might see this value here - but we
      // do *not* want to return them via the enumerator.
      if (permEntry.mPermission == nsIPermissionManager::UNKNOWN_ACTION) {
        continue;
      }

      if (!aPrefix.IsEmpty() &&
          !StringBeginsWith(mTypeArray[permEntry.mType], aPrefix)) {
        continue;
      }

      nsCOMPtr<nsIPrincipal> principal;
      nsresult rv = GetPrincipalFromOrigin(
          entry->GetKey()->mOrigin,
          IsOAForceStripPermission(mTypeArray[permEntry.mType]),
          getter_AddRefs(principal));
      if (NS_FAILED(rv)) {
        continue;
      }

      RefPtr<nsIPermission> permission = nsPermission::Create(
          principal, mTypeArray[permEntry.mType], permEntry.mPermission,
          permEntry.mExpireType, permEntry.mExpireTime,
          permEntry.mModificationTime);
      if (NS_WARN_IF(!permission)) {
        continue;
      }
      aResult.AppendElement(std::move(permission));
    }
  }

  return NS_OK;
}

nsresult nsPermissionManager::GetStripPermsForPrincipal(
    nsIPrincipal* aPrincipal, nsTArray<PermissionEntry>& aResult) {
  aResult.Clear();
  aResult.SetCapacity(kStripOAPermissions.size());

  // No special strip permissions
  if (kStripOAPermissions.empty()) {
    return NS_OK;
  }

  nsresult rv;
  // Create a key for the principal, but strip any origin attributes
  RefPtr<PermissionKey> key =
      PermissionKey::CreateFromPrincipal(aPrincipal, true, rv);
  if (!key) {
    MOZ_ASSERT(NS_FAILED(rv));
    return rv;
  }

  PermissionHashKey* hashKey = mPermissionTable.GetEntry(key);
  if (!hashKey) {
    return NS_OK;
  }

  for (const auto& permType : kStripOAPermissions) {
    int32_t index = GetTypeIndex(permType, false);
    if (index == -1) {
      continue;
    }
    PermissionEntry perm = hashKey->GetPermission(index);
    if (perm.mPermission == nsIPermissionManager::UNKNOWN_ACTION) {
      continue;
    }
    aResult.AppendElement(perm);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsPermissionManager::GetAllForPrincipal(
    nsIPrincipal* aPrincipal, nsTArray<RefPtr<nsIPermission>>& aResult) {
  aResult.Clear();

  MOZ_ASSERT(PermissionAvailable(aPrincipal, EmptyCString()));

  nsresult rv;
  RefPtr<PermissionKey> key =
      PermissionKey::CreateFromPrincipal(aPrincipal, false, rv);
  if (!key) {
    MOZ_ASSERT(NS_FAILED(rv));
    return rv;
  }
  PermissionHashKey* entry = mPermissionTable.GetEntry(key);

  nsTArray<PermissionEntry> strippedPerms;
  rv = GetStripPermsForPrincipal(aPrincipal, strippedPerms);
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (entry) {
    for (const auto& permEntry : entry->GetPermissions()) {
      // Only return custom permissions
      if (permEntry.mPermission == nsIPermissionManager::UNKNOWN_ACTION) {
        continue;
      }

      // Stripped principal permissions overwrite regular ones
      // For each permission check if there is a stripped permission we should
      // use instead
      PermissionEntry perm = permEntry;
      nsTArray<PermissionEntry>::index_type index = 0;
      for (const auto& strippedPerm : strippedPerms) {
        if (strippedPerm.mType == permEntry.mType) {
          perm = strippedPerm;
          strippedPerms.RemoveElementAt(index);
          break;
        }
        index++;
      }

      RefPtr<nsIPermission> permission = nsPermission::Create(
          aPrincipal, mTypeArray[perm.mType], perm.mPermission,
          perm.mExpireType, perm.mExpireTime, perm.mModificationTime);
      if (NS_WARN_IF(!permission)) {
        continue;
      }
      aResult.AppendElement(permission);
    }
  }

  for (const auto& perm : strippedPerms) {
    RefPtr<nsIPermission> permission = nsPermission::Create(
        aPrincipal, mTypeArray[perm.mType], perm.mPermission, perm.mExpireType,
        perm.mExpireTime, perm.mModificationTime);
    if (NS_WARN_IF(!permission)) {
      continue;
    }
    aResult.AppendElement(permission);
  }

  return NS_OK;
}

NS_IMETHODIMP nsPermissionManager::Observe(nsISupports* aSubject,
                                           const char* aTopic,
                                           const char16_t* someData) {
  ENSURE_NOT_CHILD_PROCESS;

  if (!nsCRT::strcmp(aTopic, "profile-before-change")) {
    // The profile is about to change,
    // or is going away because the application is shutting down.
    RemoveIdleDailyMaintenanceJob();
    gIsShuttingDown = true;
    RemoveAllFromMemory();
    CloseDB(false);
  } else if (!nsCRT::strcmp(aTopic, "profile-do-change")) {
    // the profile has already changed; init the db from the new location
    InitDB(false);
  } else if (!nsCRT::strcmp(aTopic, "testonly-reload-permissions-from-disk")) {
    // Testing mechanism to reload all permissions from disk. Because the
    // permission manager automatically initializes itself at startup, tests
    // that directly manipulate the permissions database need some way to reload
    // the database for their changes to have any effect. This mechanism was
    // introduced when moving the permissions manager from on-demand startup to
    // always being initialized. This is not guarded by a pref because it's not
    // dangerous to reload permissions from disk, just bad for performance.
    RemoveAllFromMemory();
    CloseDB(false);
    InitDB(false);
  } else if (!nsCRT::strcmp(aTopic, OBSERVER_TOPIC_IDLE_DAILY)) {
    PerformIdleDailyMaintenance();
  }

  return NS_OK;
}

nsresult nsPermissionManager::RemoveAllModifiedSince(
    int64_t aModificationTime) {
  ENSURE_NOT_CHILD_PROCESS;

  return RemovePermissionEntries(
      [aModificationTime](const PermissionEntry& aPermEntry) {
        return aModificationTime <= aPermEntry.mModificationTime;
      });
}

NS_IMETHODIMP
nsPermissionManager::RemovePermissionsWithAttributes(
    const nsAString& aPattern) {
  ENSURE_NOT_CHILD_PROCESS;
  mozilla::OriginAttributesPattern pattern;
  if (!pattern.Init(aPattern)) {
    return NS_ERROR_INVALID_ARG;
  }

  return RemovePermissionsWithAttributes(pattern);
}

nsresult nsPermissionManager::RemovePermissionsWithAttributes(
    mozilla::OriginAttributesPattern& aPattern) {
  Vector<Tuple<nsCOMPtr<nsIPrincipal>, nsCString, nsCString>, 10> permissions;
  for (auto iter = mPermissionTable.Iter(); !iter.Done(); iter.Next()) {
    PermissionHashKey* entry = iter.Get();

    nsCOMPtr<nsIPrincipal> principal;
    nsresult rv = GetPrincipalFromOrigin(entry->GetKey()->mOrigin, false,
                                         getter_AddRefs(principal));
    if (NS_FAILED(rv)) {
      continue;
    }

    if (!aPattern.Matches(principal->OriginAttributesRef())) {
      continue;
    }

    for (const auto& permEntry : entry->GetPermissions()) {
      if (!permissions.emplaceBack(principal, mTypeArray[permEntry.mType],
                                   entry->GetKey()->mOrigin)) {
        continue;
      }
    }
  }

  for (auto& i : permissions) {
    AddInternal(Get<0>(i), Get<1>(i), nsIPermissionManager::UNKNOWN_ACTION, 0,
                nsIPermissionManager::EXPIRE_NEVER, 0, 0,
                nsPermissionManager::eNotify, nsPermissionManager::eWriteToDB,
                false, &Get<2>(i));
  }

  return NS_OK;
}

//*****************************************************************************
//*** nsPermissionManager private methods
//*****************************************************************************

nsresult nsPermissionManager::RemoveAllFromMemory() {
  mLargestID = 0;
  mTypeArray.clear();
  mPermissionTable.Clear();

  return NS_OK;
}

// wrapper function for mangling (host,type,perm,expireType,expireTime)
// set into an nsIPermission.
void nsPermissionManager::NotifyObserversWithPermission(
    nsIPrincipal* aPrincipal, const nsACString& aType, uint32_t aPermission,
    uint32_t aExpireType, int64_t aExpireTime, int64_t aModificationTime,
    const char16_t* aData) {
  nsCOMPtr<nsIPermission> permission =
      nsPermission::Create(aPrincipal, aType, aPermission, aExpireType,
                           aExpireTime, aModificationTime);
  if (permission) NotifyObservers(permission, aData);
}

// notify observers that the permission list changed. there are four possible
// values for aData:
// "deleted" means a permission was deleted. aPermission is the deleted
// permission. "added"   means a permission was added. aPermission is the added
// permission. "changed" means a permission was altered. aPermission is the new
// permission. "cleared" means the entire permission list was cleared.
// aPermission is null.
void nsPermissionManager::NotifyObservers(nsIPermission* aPermission,
                                          const char16_t* aData) {
  nsCOMPtr<nsIObserverService> observerService =
      mozilla::services::GetObserverService();
  if (observerService)
    observerService->NotifyObservers(aPermission, kPermissionChangeNotification,
                                     aData);
}

nsresult nsPermissionManager::Read() {
  ENSURE_NOT_CHILD_PROCESS;

  nsresult rv;

  nsCOMPtr<mozIStorageStatement> stmt;
  rv = mDBConn->CreateStatement(
      NS_LITERAL_CSTRING(
          "SELECT id, origin, type, permission, expireType, "
          "expireTime, modificationTime "
          "FROM moz_perms WHERE expireType != ?1 OR expireTime > ?2"),
      getter_AddRefs(stmt));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = stmt->BindInt32ByIndex(0, nsIPermissionManager::EXPIRE_TIME);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = stmt->BindInt64ByIndex(1, EXPIRY_NOW);
  NS_ENSURE_SUCCESS(rv, rv);

  int64_t id;
  nsAutoCString origin, type;
  uint32_t permission;
  uint32_t expireType;
  int64_t expireTime;
  int64_t modificationTime;
  bool hasResult;
  bool readError = false;

  while (NS_SUCCEEDED(stmt->ExecuteStep(&hasResult)) && hasResult) {
    // explicitly set our entry id counter for use in AddInternal(),
    // and keep track of the largest id so we know where to pick up.
    id = stmt->AsInt64(0);
    if (id > mLargestID) mLargestID = id;

    rv = stmt->GetUTF8String(1, origin);
    if (NS_FAILED(rv)) {
      readError = true;
      continue;
    }

    rv = stmt->GetUTF8String(2, type);
    if (NS_FAILED(rv)) {
      readError = true;
      continue;
    }

    permission = stmt->AsInt32(3);
    expireType = stmt->AsInt32(4);

    // convert into int64_t values (milliseconds)
    expireTime = stmt->AsInt64(5);
    modificationTime = stmt->AsInt64(6);

    nsCOMPtr<nsIPrincipal> principal;
    nsresult rv = GetPrincipalFromOrigin(origin, IsOAForceStripPermission(type),
                                         getter_AddRefs(principal));
    if (NS_FAILED(rv)) {
      readError = true;
      continue;
    }

    rv = AddInternal(principal, type, permission, id, expireType, expireTime,
                     modificationTime, eDontNotify, eNoDBOperation, false,
                     &origin);
    if (NS_FAILED(rv)) {
      readError = true;
      continue;
    }
  }

  if (readError) {
    NS_ERROR("Error occured while reading the permissions database!");
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

static const char kMatchTypeHost[] = "host";
static const char kMatchTypeOrigin[] = "origin";

// ImportDefaults will read a URL with default permissions and add them to the
// in-memory copy of permissions.  The database is *not* written to.
nsresult nsPermissionManager::ImportDefaults() {
  nsAutoCString defaultsURL;
  mozilla::Preferences::GetCString(kDefaultsUrlPrefName, defaultsURL);
  if (defaultsURL.IsEmpty()) {  // == Don't use built-in permissions.
    return NS_OK;
  }

  nsCOMPtr<nsIURI> defaultsURI;
  nsresult rv = NS_NewURI(getter_AddRefs(defaultsURI), defaultsURL);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIChannel> channel;
  rv = NS_NewChannel(getter_AddRefs(channel), defaultsURI,
                     nsContentUtils::GetSystemPrincipal(),
                     nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_DATA_IS_NULL,
                     nsIContentPolicy::TYPE_OTHER);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIInputStream> inputStream;
  rv = channel->Open(getter_AddRefs(inputStream));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = _DoImport(inputStream, nullptr);
  inputStream->Close();
  return rv;
}

// _DoImport reads the specified stream and adds the parsed elements.  If
// |conn| is passed, the imported data will be written to the database, but if
// |conn| is null the data will be added only to the in-memory copy of the
// database.
nsresult nsPermissionManager::_DoImport(nsIInputStream* inputStream,
                                        mozIStorageConnection* conn) {
  ENSURE_NOT_CHILD_PROCESS;

  nsresult rv;
  // start a transaction on the storage db, to optimize insertions.
  // transaction will automically commit on completion
  // (note the transaction is a no-op if a null connection is passed)
  mozStorageTransaction transaction(conn, true);

  // The DB operation - we only try and write if a connection was passed.
  DBOperationType operation = conn ? eWriteToDB : eNoDBOperation;
  // and if no DB connection was passed we assume this is a "default"
  // permission, so use the special ID which indicates this.
  int64_t id = conn ? 0 : cIDPermissionIsDefault;

  /* format is:
   * matchtype \t type \t permission \t host
   * Only "host" is supported for matchtype
   * type is a string that identifies the type of permission (e.g. "cookie")
   * permission is an integer between 1 and 15
   */

  // Ideally we'd do this with nsILineInputString, but this is called with an
  // nsIInputStream that comes from a resource:// URI, which doesn't support
  // that interface.  So NS_ReadLine to the rescue...
  nsLineBuffer<char> lineBuffer;
  nsCString line;
  bool isMore = true;
  do {
    rv = NS_ReadLine(inputStream, &lineBuffer, line, &isMore);
    NS_ENSURE_SUCCESS(rv, rv);

    if (line.IsEmpty() || line.First() == '#') {
      continue;
    }

    nsTArray<nsCString> lineArray;

    // Split the line at tabs
    ParseString(line, '\t', lineArray);

    if (lineArray[0].EqualsLiteral(kMatchTypeHost) && lineArray.Length() == 4) {
      nsresult error = NS_OK;
      uint32_t permission = lineArray[2].ToInteger(&error);
      if (NS_FAILED(error)) continue;

      // the import file format doesn't handle modification times, so we use
      // 0, which AddInternal will convert to now()
      int64_t modificationTime = 0;

      UpgradeHostToOriginHostfileImport upHelper(this, operation, id);
      error =
          UpgradeHostToOriginAndInsert(lineArray[3], lineArray[1], permission,
                                       nsIPermissionManager::EXPIRE_NEVER, 0,
                                       modificationTime, false, &upHelper);
      if (NS_FAILED(error)) {
        NS_WARNING("There was a problem importing a host permission");
      }
    } else if (lineArray[0].EqualsLiteral(kMatchTypeOrigin) &&
               lineArray.Length() == 4) {
      nsresult error = NS_OK;
      uint32_t permission = lineArray[2].ToInteger(&error);
      if (NS_FAILED(error)) continue;

      nsCOMPtr<nsIPrincipal> principal;
      error = GetPrincipalFromOrigin(lineArray[3],
                                     IsOAForceStripPermission(lineArray[1]),
                                     getter_AddRefs(principal));
      if (NS_FAILED(error)) {
        NS_WARNING("Couldn't import an origin permission - malformed origin");
        continue;
      }

      // the import file format doesn't handle modification times, so we use
      // 0, which AddInternal will convert to now()
      int64_t modificationTime = 0;

      error = AddInternal(principal, lineArray[1], permission, id,
                          nsIPermissionManager::EXPIRE_NEVER, 0,
                          modificationTime, eDontNotify, operation);
      if (NS_FAILED(error)) {
        NS_WARNING("There was a problem importing an origin permission");
      }
    }

  } while (isMore);

  return NS_OK;
}

void nsPermissionManager::UpdateDB(
    OperationType aOp, mozIStorageAsyncStatement* aStmt, int64_t aID,
    const nsACString& aOrigin, const nsACString& aType, uint32_t aPermission,
    uint32_t aExpireType, int64_t aExpireTime, int64_t aModificationTime) {
  ENSURE_NOT_CHILD_PROCESS_NORET;

  nsresult rv;

  // no statement is ok - just means we don't have a profile
  if (!aStmt) return;

  switch (aOp) {
    case eOperationAdding: {
      rv = aStmt->BindInt64ByIndex(0, aID);
      if (NS_FAILED(rv)) break;

      rv = aStmt->BindUTF8StringByIndex(1, aOrigin);
      if (NS_FAILED(rv)) break;

      rv = aStmt->BindUTF8StringByIndex(2, aType);
      if (NS_FAILED(rv)) break;

      rv = aStmt->BindInt32ByIndex(3, aPermission);
      if (NS_FAILED(rv)) break;

      rv = aStmt->BindInt32ByIndex(4, aExpireType);
      if (NS_FAILED(rv)) break;

      rv = aStmt->BindInt64ByIndex(5, aExpireTime);
      if (NS_FAILED(rv)) break;

      rv = aStmt->BindInt64ByIndex(6, aModificationTime);
      break;
    }

    case eOperationRemoving: {
      rv = aStmt->BindInt64ByIndex(0, aID);
      break;
    }

    case eOperationChanging: {
      rv = aStmt->BindInt64ByIndex(0, aID);
      if (NS_FAILED(rv)) break;

      rv = aStmt->BindInt32ByIndex(1, aPermission);
      if (NS_FAILED(rv)) break;

      rv = aStmt->BindInt32ByIndex(2, aExpireType);
      if (NS_FAILED(rv)) break;

      rv = aStmt->BindInt64ByIndex(3, aExpireTime);
      if (NS_FAILED(rv)) break;

      rv = aStmt->BindInt64ByIndex(4, aModificationTime);
      break;
    }

    default: {
      MOZ_ASSERT_UNREACHABLE("need a valid operation in UpdateDB()!");
      rv = NS_ERROR_UNEXPECTED;
      break;
    }
  }

  if (NS_FAILED(rv)) {
    NS_WARNING("db change failed!");
    return;
  }

  nsCOMPtr<mozIStoragePendingStatement> pending;
  rv = aStmt->ExecuteAsync(nullptr, getter_AddRefs(pending));
  MOZ_ASSERT(NS_SUCCEEDED(rv));
}

bool nsPermissionManager::GetPermissionsFromOriginOrKey(
    const nsACString& aOrigin, const nsACString& aKey,
    nsTArray<IPC::Permission>& aPerms) {
  aPerms.Clear();
  if (NS_WARN_IF(XRE_IsContentProcess())) {
    return false;
  }

  for (auto iter = mPermissionTable.Iter(); !iter.Done(); iter.Next()) {
    PermissionHashKey* entry = iter.Get();

    nsAutoCString permissionKey;
    if (aOrigin.IsEmpty()) {
      // We can't check for individual OA strip perms here.
      // Don't force strip origin attributes.
      GetKeyForOrigin(entry->GetKey()->mOrigin, false, permissionKey);

      // If the keys don't match, and we aren't getting the default "" key, then
      // we can exit early. We have to keep looking if we're getting the default
      // key, as we may see a preload permission which should be transmitted.
      if (aKey != permissionKey && !aKey.IsEmpty()) {
        continue;
      }
    } else if (aOrigin != entry->GetKey()->mOrigin) {
      // If the origins don't match, then we can exit early. We have to keep
      // looking if we're getting the default origin, as we may see a preload
      // permission which should be transmitted.
      continue;
    }

    for (const auto& permEntry : entry->GetPermissions()) {
      // Given how "default" permissions work and the possibility of them
      // being overridden with UNKNOWN_ACTION, we might see this value here -
      // but we do not want to send it to the content process.
      if (permEntry.mPermission == nsIPermissionManager::UNKNOWN_ACTION) {
        continue;
      }

      bool isPreload = IsPreloadPermission(mTypeArray[permEntry.mType]);
      bool shouldAppend;
      if (aOrigin.IsEmpty()) {
        shouldAppend = (isPreload && aKey.IsEmpty()) ||
                       (!isPreload && aKey == permissionKey);
      } else {
        shouldAppend = (!isPreload && aOrigin == entry->GetKey()->mOrigin);
      }
      if (shouldAppend) {
        aPerms.AppendElement(
            IPC::Permission(entry->GetKey()->mOrigin,
                            mTypeArray[permEntry.mType], permEntry.mPermission,
                            permEntry.mExpireType, permEntry.mExpireTime));
      }
    }
  }

  return true;
}

void nsPermissionManager::SetPermissionsWithKey(
    const nsACString& aPermissionKey, nsTArray<IPC::Permission>& aPerms) {
  if (NS_WARN_IF(XRE_IsParentProcess())) {
    return;
  }

  RefPtr<GenericNonExclusivePromise::Private> promise;
  bool foundKey =
      mPermissionKeyPromiseMap.Get(aPermissionKey, getter_AddRefs(promise));
  if (promise) {
    MOZ_ASSERT(foundKey);
    // NOTE: This will resolve asynchronously, so we can mark it as resolved
    // now, and be confident that we will have filled in the database before any
    // callbacks run.
    promise->Resolve(true, __func__);
  } else if (foundKey) {
    // NOTE: We shouldn't be sent two InitializePermissionsWithKey for the same
    // key, but it's possible.
    return;
  }
  mPermissionKeyPromiseMap.Put(aPermissionKey,
                               RefPtr<GenericNonExclusivePromise::Private>{});

  // Add the permissions locally to our process
  for (IPC::Permission& perm : aPerms) {
    nsCOMPtr<nsIPrincipal> principal;
    nsresult rv =
        GetPrincipalFromOrigin(perm.origin, IsOAForceStripPermission(perm.type),
                               getter_AddRefs(principal));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      continue;
    }

#ifdef DEBUG
    nsAutoCString permissionKey;
    GetKeyForPermission(principal, perm.type, permissionKey);
    MOZ_ASSERT(permissionKey == aPermissionKey,
               "The permission keys which were sent over should match!");
#endif

    // The child process doesn't care about modification times - it neither
    // reads nor writes, nor removes them based on the date - so 0 (which
    // will end up as now()) is fine.
    uint64_t modificationTime = 0;
    AddInternal(principal, perm.type, perm.capability, 0, perm.expireType,
                perm.expireTime, modificationTime, eNotify, eNoDBOperation,
                true /* ignoreSessionPermissions */);
  }
}

/* static */
void nsPermissionManager::GetKeyForOrigin(const nsACString& aOrigin,
                                          bool aForceStripOA,
                                          nsACString& aKey) {
  aKey.Truncate();

  // We only key origins for http, https, and ftp URIs. All origins begin with
  // the URL which they apply to, which means that they should begin with their
  // scheme in the case where they are one of these interesting URIs. We don't
  // want to actually parse the URL here however, because this can be called on
  // hot paths.
  if (!StringBeginsWith(aOrigin, NS_LITERAL_CSTRING("http:")) &&
      !StringBeginsWith(aOrigin, NS_LITERAL_CSTRING("https:")) &&
      !StringBeginsWith(aOrigin, NS_LITERAL_CSTRING("ftp:"))) {
    return;
  }

  // We need to look at the originAttributes if they are present, to make sure
  // to remove any which we don't want. We put the rest of the origin, not
  // including the attributes, into the key.
  OriginAttributes attrs;
  if (!attrs.PopulateFromOrigin(aOrigin, aKey)) {
    aKey.Truncate();
    return;
  }

  MaybeStripOAs(aForceStripOA, attrs);

#ifdef DEBUG
  // Parse the origin string into a principal, and extract some useful
  // information from it for assertions.
  nsCOMPtr<nsIPrincipal> dbgPrincipal;
  MOZ_ALWAYS_SUCCEEDS(GetPrincipalFromOrigin(aOrigin, aForceStripOA,
                                             getter_AddRefs(dbgPrincipal)));
  MOZ_ASSERT(dbgPrincipal->SchemeIs("http") ||
             dbgPrincipal->SchemeIs("https") || dbgPrincipal->SchemeIs("ftp"));
  MOZ_ASSERT(dbgPrincipal->OriginAttributesRef() == attrs);
#endif

  // Append the stripped suffix to the output origin key.
  nsAutoCString suffix;
  attrs.CreateSuffix(suffix);
  aKey.Append(suffix);
}

/* static */
void nsPermissionManager::GetKeyForPrincipal(nsIPrincipal* aPrincipal,
                                             bool aForceStripOA,
                                             nsACString& aKey) {
  nsAutoCString origin;
  nsresult rv = aPrincipal->GetOrigin(origin);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aKey.Truncate();
    return;
  }
  GetKeyForOrigin(origin, aForceStripOA, aKey);
}

/* static */
void nsPermissionManager::GetKeyForPermission(nsIPrincipal* aPrincipal,
                                              const nsACString& aType,
                                              nsACString& aKey) {
  // Preload permissions have the "" key.
  if (IsPreloadPermission(aType)) {
    aKey.Truncate();
    return;
  }

  GetKeyForPrincipal(aPrincipal, IsOAForceStripPermission(aType), aKey);
}

/* static */
nsTArray<std::pair<nsCString, nsCString>>
nsPermissionManager::GetAllKeysForPrincipal(nsIPrincipal* aPrincipal) {
  MOZ_ASSERT(aPrincipal);

  nsTArray<std::pair<nsCString, nsCString>> pairs;
  nsCOMPtr<nsIPrincipal> prin = aPrincipal;
  while (prin) {
    // Add the pair to the list
    std::pair<nsCString, nsCString>* pair =
        pairs.AppendElement(std::make_pair(EmptyCString(), EmptyCString()));
    // We can't check for individual OA strip perms here.
    // Don't force strip origin attributes.
    GetKeyForPrincipal(prin, false, pair->first);

    Unused << GetOriginFromPrincipal(prin, false, pair->second);

    // Get the next subdomain principal and loop back around.
    prin = GetNextSubDomainPrincipal(prin);
  }

  MOZ_ASSERT(pairs.Length() >= 1,
             "Every principal should have at least one pair item.");
  return pairs;
}

NS_IMETHODIMP
nsPermissionManager::BroadcastPermissionsForPrincipalToAllContentProcesses(
    nsIPrincipal* aPrincipal) {
  nsTArray<ContentParent*> cps;
  ContentParent::GetAll(cps);
  for (ContentParent* cp : cps) {
    nsresult rv = cp->TransmitPermissionsForPrincipal(aPrincipal);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

bool nsPermissionManager::PermissionAvailable(nsIPrincipal* aPrincipal,
                                              const nsACString& aType) {
  if (XRE_IsContentProcess()) {
    nsAutoCString permissionKey;
    // NOTE: GetKeyForPermission accepts a null aType.
    GetKeyForPermission(aPrincipal, aType, permissionKey);

    // If we have a pending promise for the permission key in question, we don't
    // have the permission available, so report a warning and return false.
    RefPtr<GenericNonExclusivePromise::Private> promise;
    if (!mPermissionKeyPromiseMap.Get(permissionKey, getter_AddRefs(promise)) ||
        promise) {
      // Emit a useful diagnostic warning with the permissionKey for the process
      // which hasn't received permissions yet.
      NS_WARNING(nsPrintfCString("This content process hasn't received the "
                                 "permissions for %s yet",
                                 permissionKey.get())
                     .get());
      return false;
    }
  }
  return true;
}

void nsPermissionManager::WhenPermissionsAvailable(nsIPrincipal* aPrincipal,
                                                   nsIRunnable* aRunnable) {
  MOZ_ASSERT(aRunnable);

  if (!XRE_IsContentProcess()) {
    aRunnable->Run();
    return;
  }

  nsTArray<RefPtr<GenericNonExclusivePromise>> promises;
  for (auto& pair : GetAllKeysForPrincipal(aPrincipal)) {
    RefPtr<GenericNonExclusivePromise::Private> promise;
    if (!mPermissionKeyPromiseMap.Get(pair.first, getter_AddRefs(promise))) {
      // In this case we have found a permission which isn't available in the
      // content process and hasn't been requested yet. We need to create a new
      // promise, and send the request to the parent (if we have not already
      // done so).
      promise = new GenericNonExclusivePromise::Private(__func__);
      mPermissionKeyPromiseMap.Put(pair.first, RefPtr{promise});
    }

    if (promise) {
      promises.AppendElement(std::move(promise));
    }
  }

  // If all of our permissions are available, immediately run the runnable. This
  // avoids any extra overhead during fetch interception which is performance
  // sensitive.
  if (promises.IsEmpty()) {
    aRunnable->Run();
    return;
  }

  auto* thread = SystemGroup::AbstractMainThreadFor(TaskCategory::Other);

  RefPtr<nsIRunnable> runnable = aRunnable;
  GenericNonExclusivePromise::All(thread, promises)
      ->Then(
          thread, __func__, [runnable]() { runnable->Run(); },
          []() {
            NS_WARNING(
                "nsPermissionManager permission promise rejected. We're "
                "probably shutting down.");
          });
}
