/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DOMStorageObserver.h"

#include "DOMStorageDBThread.h"
#include "DOMStorageCache.h"

#include "nsIObserverService.h"
#include "nsIURI.h"
#include "nsIURL.h"
#include "nsIScriptSecurityManager.h"
#include "nsIPermission.h"
#include "nsIIDNService.h"
#include "mozIApplicationClearPrivateDataParams.h"
#include "nsICookiePermission.h"

#include "nsPrintfCString.h"
#include "nsXULAppAPI.h"
#include "nsEscape.h"
#include "nsNetCID.h"
#include "mozilla/Services.h"
#include "nsServiceManagerUtils.h"

namespace mozilla {
namespace dom {

static const char kStartupTopic[] = "sessionstore-windows-restored";
static const uint32_t kStartupDelay = 0;

NS_IMPL_ISUPPORTS(DOMStorageObserver,
                  nsIObserver,
                  nsISupportsWeakReference)

DOMStorageObserver* DOMStorageObserver::sSelf = nullptr;

extern nsresult
CreateReversedDomain(const nsACString& aAsciiDomain, nsACString& aKey);

// static
nsresult
DOMStorageObserver::Init()
{
  if (sSelf) {
    return NS_OK;
  }

  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (!obs) {
    return NS_ERROR_UNEXPECTED;
  }

  sSelf = new DOMStorageObserver();
  NS_ADDREF(sSelf);

  // Chrome clear operations.
  obs->AddObserver(sSelf, kStartupTopic, true);
  obs->AddObserver(sSelf, "cookie-changed", true);
  obs->AddObserver(sSelf, "perm-changed", true);
  obs->AddObserver(sSelf, "browser:purge-domain-data", true);
  obs->AddObserver(sSelf, "last-pb-context-exited", true);
  obs->AddObserver(sSelf, "webapps-clear-data", true);

  // Shutdown
  obs->AddObserver(sSelf, "profile-after-change", true);
  obs->AddObserver(sSelf, "profile-before-change", true);
  obs->AddObserver(sSelf, "xpcom-shutdown", true);

  // Observe low device storage notifications.
  obs->AddObserver(sSelf, "disk-space-watcher", true);

#ifdef DOM_STORAGE_TESTS
  // Testing
  obs->AddObserver(sSelf, "domstorage-test-flush-force", true);
  if (XRE_IsParentProcess()) {
    // Only to forward to child process.
    obs->AddObserver(sSelf, "domstorage-test-flushed", true);
  }

  obs->AddObserver(sSelf, "domstorage-test-reload", true);
#endif

  return NS_OK;
}

// static
nsresult
DOMStorageObserver::Shutdown()
{
  if (!sSelf) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  NS_RELEASE(sSelf);
  return NS_OK;
}

void
DOMStorageObserver::AddSink(DOMStorageObserverSink* aObs)
{
  mSinks.AppendElement(aObs);
}

void
DOMStorageObserver::RemoveSink(DOMStorageObserverSink* aObs)
{
  mSinks.RemoveElement(aObs);
}

void
DOMStorageObserver::Notify(const char* aTopic, const nsACString& aData)
{
  for (uint32_t i = 0; i < mSinks.Length(); ++i) {
    DOMStorageObserverSink* sink = mSinks[i];
    sink->Observe(aTopic, aData);
  }
}

NS_IMETHODIMP
DOMStorageObserver::Observe(nsISupports* aSubject,
                            const char* aTopic,
                            const char16_t* aData)
{
  nsresult rv;

  // Start the thread that opens the database.
  if (!strcmp(aTopic, kStartupTopic)) {
    nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
    obs->RemoveObserver(this, kStartupTopic);

    mDBThreadStartDelayTimer = do_CreateInstance(NS_TIMER_CONTRACTID);
    if (!mDBThreadStartDelayTimer) {
      return NS_ERROR_UNEXPECTED;
    }

    mDBThreadStartDelayTimer->Init(this, nsITimer::TYPE_ONE_SHOT, kStartupDelay);

    return NS_OK;
  }

  // Timer callback used to start the database a short timer after startup
  if (!strcmp(aTopic, NS_TIMER_CALLBACK_TOPIC)) {
    nsCOMPtr<nsITimer> timer = do_QueryInterface(aSubject);
    if (!timer) {
      return NS_ERROR_UNEXPECTED;
    }

    if (timer == mDBThreadStartDelayTimer) {
      mDBThreadStartDelayTimer = nullptr;

      DOMStorageDBBridge* db = DOMStorageCache::StartDatabase();
      NS_ENSURE_TRUE(db, NS_ERROR_FAILURE);
    }

    return NS_OK;
  }

  // Clear everything, caches + database
  if (!strcmp(aTopic, "cookie-changed")) {
    if (!NS_LITERAL_STRING("cleared").Equals(aData)) {
      return NS_OK;
    }

    DOMStorageDBBridge* db = DOMStorageCache::StartDatabase();
    NS_ENSURE_TRUE(db, NS_ERROR_FAILURE);

    db->AsyncClearAll();

    Notify("cookie-cleared");

    return NS_OK;
  }

  // Clear from caches everything that has been stored
  // while in session-only mode
  if (!strcmp(aTopic, "perm-changed")) {
    // Check for cookie permission change
    nsCOMPtr<nsIPermission> perm(do_QueryInterface(aSubject));
    if (!perm) {
      return NS_OK;
    }

    nsAutoCString type;
    perm->GetType(type);
    if (type != NS_LITERAL_CSTRING("cookie")) {
      return NS_OK;
    }

    uint32_t cap = 0;
    perm->GetCapability(&cap);
    if (!(cap & nsICookiePermission::ACCESS_SESSION) ||
        !NS_LITERAL_STRING("deleted").Equals(nsDependentString(aData))) {
      return NS_OK;
    }

    nsCOMPtr<nsIPrincipal> principal;
    perm->GetPrincipal(getter_AddRefs(principal));
    if (!principal) {
      return NS_OK;
    }

    nsCOMPtr<nsIURI> origin;
    principal->GetURI(getter_AddRefs(origin));
    if (!origin) {
      return NS_OK;
    }

    nsAutoCString host;
    origin->GetHost(host);
    if (host.IsEmpty()) {
      return NS_OK;
    }

    nsAutoCString scope;
    rv = CreateReversedDomain(host, scope);
    NS_ENSURE_SUCCESS(rv, rv);

    Notify("session-only-cleared", scope);

    return NS_OK;
  }

  // Clear everything (including so and pb data) from caches and database
  // for the gived domain and subdomains.
  if (!strcmp(aTopic, "browser:purge-domain-data")) {
    // Convert the domain name to the ACE format
    nsAutoCString aceDomain;
    nsCOMPtr<nsIIDNService> converter = do_GetService(NS_IDNSERVICE_CONTRACTID);
    if (converter) {
      rv = converter->ConvertUTF8toACE(NS_ConvertUTF16toUTF8(aData), aceDomain);
      NS_ENSURE_SUCCESS(rv, rv);
    } else {
      // In case the IDN service is not available, this is the best we can come up with!
      NS_EscapeURL(NS_ConvertUTF16toUTF8(aData),
                   esc_OnlyNonASCII | esc_AlwaysCopy,
                   aceDomain);
    }

    nsAutoCString scopePrefix;
    rv = CreateReversedDomain(aceDomain, scopePrefix);
    NS_ENSURE_SUCCESS(rv, rv);

    DOMStorageDBBridge* db = DOMStorageCache::StartDatabase();
    NS_ENSURE_TRUE(db, NS_ERROR_FAILURE);

    db->AsyncClearMatchingScope(scopePrefix);

    Notify("domain-data-cleared", scopePrefix);

    return NS_OK;
  }

  // Clear all private-browsing caches
  if (!strcmp(aTopic, "last-pb-context-exited")) {
    Notify("private-browsing-data-cleared");

    return NS_OK;
  }

  // Clear data beloging to an app.
  if (!strcmp(aTopic, "webapps-clear-data")) {
    nsCOMPtr<mozIApplicationClearPrivateDataParams> params =
      do_QueryInterface(aSubject);
    if (!params) {
      NS_ERROR("'webapps-clear-data' notification's subject should be a mozIApplicationClearPrivateDataParams");
      return NS_ERROR_UNEXPECTED;
    }

    uint32_t appId;
    bool browserOnly;

    rv = params->GetAppId(&appId);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = params->GetBrowserOnly(&browserOnly);
    NS_ENSURE_SUCCESS(rv, rv);

    MOZ_ASSERT(appId != nsIScriptSecurityManager::UNKNOWN_APP_ID);

    DOMStorageDBBridge* db = DOMStorageCache::StartDatabase();
    NS_ENSURE_TRUE(db, NS_ERROR_FAILURE);

    nsAutoCString scope;
    scope.AppendInt(appId);
    scope.AppendLiteral(":t:");
    db->AsyncClearMatchingScope(scope);
    Notify("app-data-cleared", scope);

    if (!browserOnly) {
      scope.Truncate();
      scope.AppendInt(appId);
      scope.AppendLiteral(":f:");
      db->AsyncClearMatchingScope(scope);
      Notify("app-data-cleared", scope);
    }

    return NS_OK;
  }

  if (!strcmp(aTopic, "profile-after-change")) {
    Notify("profile-change");

    return NS_OK;
  }

  if (!strcmp(aTopic, "profile-before-change") ||
      !strcmp(aTopic, "xpcom-shutdown")) {
    rv = DOMStorageCache::StopDatabase();
    if (NS_FAILED(rv)) {
      NS_WARNING("Error while stopping DOMStorage DB background thread");
    }

    return NS_OK;
  }

  if (!strcmp(aTopic, "disk-space-watcher")) {
    if (NS_LITERAL_STRING("full").Equals(aData)) {
      Notify("low-disk-space");
    } else if (NS_LITERAL_STRING("free").Equals(aData)) {
      Notify("no-low-disk-space");
    }

    return NS_OK;
  }

#ifdef DOM_STORAGE_TESTS
  if (!strcmp(aTopic, "domstorage-test-flush-force")) {
    DOMStorageDBBridge* db = DOMStorageCache::GetDatabase();
    if (db) {
      db->AsyncFlush();
    }

    return NS_OK;
  }

  if (!strcmp(aTopic, "domstorage-test-flushed")) {
    // Only used to propagate to IPC children
    Notify("test-flushed");

    return NS_OK;
  }

  if (!strcmp(aTopic, "domstorage-test-reload")) {
    Notify("test-reload");

    return NS_OK;
  }
#endif

  NS_ERROR("Unexpected topic");
  return NS_ERROR_UNEXPECTED;
}

} // namespace dom
} // namespace mozilla
