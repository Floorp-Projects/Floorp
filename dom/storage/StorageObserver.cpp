/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "StorageObserver.h"

#include "LocalStorageCache.h"
#include "StorageDBThread.h"
#include "StorageUtils.h"

#include "mozilla/BasePrincipal.h"
#include "nsIObserverService.h"
#include "nsIURI.h"
#include "nsIURL.h"
#include "nsIScriptSecurityManager.h"
#include "nsIPermission.h"
#include "nsIIDNService.h"
#include "nsICookiePermission.h"

#include "nsPrintfCString.h"
#include "nsXULAppAPI.h"
#include "nsEscape.h"
#include "nsNetCID.h"
#include "mozilla/Preferences.h"
#include "mozilla/Services.h"
#include "nsServiceManagerUtils.h"

namespace mozilla {
namespace dom {

using namespace StorageUtils;

static const char kStartupTopic[] = "sessionstore-windows-restored";
static const uint32_t kStartupDelay = 0;

const char kTestingPref[] = "dom.storage.testing";

NS_IMPL_ISUPPORTS(StorageObserver,
                  nsIObserver,
                  nsISupportsWeakReference)

StorageObserver* StorageObserver::sSelf = nullptr;

// static
nsresult
StorageObserver::Init()
{
  if (sSelf) {
    return NS_OK;
  }

  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (!obs) {
    return NS_ERROR_UNEXPECTED;
  }

  sSelf = new StorageObserver();
  NS_ADDREF(sSelf);

  // Chrome clear operations.
  obs->AddObserver(sSelf, kStartupTopic, true);
  obs->AddObserver(sSelf, "cookie-changed", true);
  obs->AddObserver(sSelf, "perm-changed", true);
  obs->AddObserver(sSelf, "browser:purge-domain-data", true);
  obs->AddObserver(sSelf, "last-pb-context-exited", true);
  obs->AddObserver(sSelf, "clear-origin-attributes-data", true);
  obs->AddObserver(sSelf, "extension:purge-localStorage", true);

  // Shutdown
  obs->AddObserver(sSelf, "profile-after-change", true);
  if (XRE_IsParentProcess()) {
    obs->AddObserver(sSelf, "profile-before-change", true);
  }

  // Observe low device storage notifications.
  obs->AddObserver(sSelf, "disk-space-watcher", true);

  // Testing
#ifdef DOM_STORAGE_TESTS
  Preferences::RegisterCallbackAndCall(TestingPrefChanged, kTestingPref);
#endif

  return NS_OK;
}

// static
nsresult
StorageObserver::Shutdown()
{
  if (!sSelf) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  NS_RELEASE(sSelf);
  return NS_OK;
}

// static
void
StorageObserver::TestingPrefChanged(const char* aPrefName, void* aClosure)
{
  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (!obs) {
    return;
  }

  if (Preferences::GetBool(kTestingPref)) {
    obs->AddObserver(sSelf, "domstorage-test-flush-force", true);
    if (XRE_IsParentProcess()) {
      // Only to forward to child process.
      obs->AddObserver(sSelf, "domstorage-test-flushed", true);
    }
    obs->AddObserver(sSelf, "domstorage-test-reload", true);
  } else {
    obs->RemoveObserver(sSelf, "domstorage-test-flush-force");
    if (XRE_IsParentProcess()) {
      // Only to forward to child process.
      obs->RemoveObserver(sSelf, "domstorage-test-flushed");
    }
    obs->RemoveObserver(sSelf, "domstorage-test-reload");
  }
}

void
StorageObserver::AddSink(StorageObserverSink* aObs)
{
  mSinks.AppendElement(aObs);
}

void
StorageObserver::RemoveSink(StorageObserverSink* aObs)
{
  mSinks.RemoveElement(aObs);
}

void
StorageObserver::Notify(const char* aTopic,
                        const nsAString& aOriginAttributesPattern,
                        const nsACString& aOriginScope)
{
  for (uint32_t i = 0; i < mSinks.Length(); ++i) {
    StorageObserverSink* sink = mSinks[i];
    sink->Observe(aTopic, aOriginAttributesPattern, aOriginScope);
  }
}

void
StorageObserver::NoteBackgroundThread(nsIEventTarget* aBackgroundThread)
{
  mBackgroundThread = aBackgroundThread;
}

NS_IMETHODIMP
StorageObserver::Observe(nsISupports* aSubject,
                         const char* aTopic,
                         const char16_t* aData)
{
  nsresult rv;

  // Start the thread that opens the database.
  if (!strcmp(aTopic, kStartupTopic)) {
    MOZ_ASSERT(XRE_IsParentProcess());

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
    MOZ_ASSERT(XRE_IsParentProcess());

    nsCOMPtr<nsITimer> timer = do_QueryInterface(aSubject);
    if (!timer) {
      return NS_ERROR_UNEXPECTED;
    }

    if (timer == mDBThreadStartDelayTimer) {
      mDBThreadStartDelayTimer = nullptr;

      StorageDBChild* storageChild = StorageDBChild::GetOrCreate();
      if (NS_WARN_IF(!storageChild)) {
        return NS_ERROR_FAILURE;
      }

      storageChild->SendStartup();
    }

    return NS_OK;
  }

  // Clear everything, caches + database
  if (!strcmp(aTopic, "cookie-changed")) {
    if (!NS_LITERAL_STRING("cleared").Equals(aData)) {
      return NS_OK;
    }

    StorageDBChild* storageChild = StorageDBChild::GetOrCreate();
    if (NS_WARN_IF(!storageChild)) {
      return NS_ERROR_FAILURE;
    }

    storageChild->AsyncClearAll();

    if (XRE_IsParentProcess()) {
      storageChild->SendClearAll();
    }

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

    nsAutoCString originSuffix;
    BasePrincipal::Cast(principal)->OriginAttributesRef().CreateSuffix(originSuffix);

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

    nsAutoCString originScope;
    rv = CreateReversedDomain(host, originScope);
    NS_ENSURE_SUCCESS(rv, rv);

    Notify("session-only-cleared", NS_ConvertUTF8toUTF16(originSuffix),
           originScope);

    return NS_OK;
  }

  if (!strcmp(aTopic, "extension:purge-localStorage")) {
    StorageDBChild* storageChild = StorageDBChild::GetOrCreate();
    if (NS_WARN_IF(!storageChild)) {
      return NS_ERROR_FAILURE;
    }

    storageChild->AsyncClearAll();

    if (XRE_IsParentProcess()) {
      storageChild->SendClearAll();
    }

    Notify("extension:purge-localStorage-caches");

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
      // In case the IDN service is not available, this is the best we can come
      // up with!
      rv = NS_EscapeURL(NS_ConvertUTF16toUTF8(aData),
                        esc_OnlyNonASCII | esc_AlwaysCopy,
                        aceDomain,
                        fallible);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    nsAutoCString originScope;
    rv = CreateReversedDomain(aceDomain, originScope);
    NS_ENSURE_SUCCESS(rv, rv);

    if (XRE_IsParentProcess()) {
      StorageDBChild* storageChild = StorageDBChild::GetOrCreate();
      if (NS_WARN_IF(!storageChild)) {
        return NS_ERROR_FAILURE;
      }

      storageChild->SendClearMatchingOrigin(originScope);
    }

    Notify("domain-data-cleared", EmptyString(), originScope);

    return NS_OK;
  }

  // Clear all private-browsing caches
  if (!strcmp(aTopic, "last-pb-context-exited")) {
    Notify("private-browsing-data-cleared");

    return NS_OK;
  }

  // Clear data of the origins whose prefixes will match the suffix.
  if (!strcmp(aTopic, "clear-origin-attributes-data")) {
    MOZ_ASSERT(XRE_IsParentProcess());

    OriginAttributesPattern pattern;
    if (!pattern.Init(nsDependentString(aData))) {
      NS_ERROR("Cannot parse origin attributes pattern");
      return NS_ERROR_FAILURE;
    }

    StorageDBChild* storageChild = StorageDBChild::GetOrCreate();
    if (NS_WARN_IF(!storageChild)) {
      return NS_ERROR_FAILURE;
    }

    storageChild->SendClearMatchingOriginAttributes(pattern);

    Notify("origin-attr-pattern-cleared", nsDependentString(aData));

    return NS_OK;
  }

  if (!strcmp(aTopic, "profile-after-change")) {
    Notify("profile-change");

    return NS_OK;
  }

  if (!strcmp(aTopic, "profile-before-change")) {
    MOZ_ASSERT(XRE_IsParentProcess());

    if (mBackgroundThread) {
      bool done = false;

      RefPtr<StorageDBThread::ShutdownRunnable> shutdownRunnable =
        new StorageDBThread::ShutdownRunnable(done);
      MOZ_ALWAYS_SUCCEEDS(
        mBackgroundThread->Dispatch(shutdownRunnable, NS_DISPATCH_NORMAL));

      MOZ_ALWAYS_TRUE(SpinEventLoopUntil([&]() { return done; }));

      mBackgroundThread = nullptr;
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
    StorageDBChild* storageChild = StorageDBChild::GetOrCreate();
    if (NS_WARN_IF(!storageChild)) {
      return NS_ERROR_FAILURE;
    }

    storageChild->SendAsyncFlush();

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
