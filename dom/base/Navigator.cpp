/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Needs to be first.
#include "base/basictypes.h"

#include "Navigator.h"
#include "nsIXULAppInfo.h"
#include "nsPluginArray.h"
#include "nsMimeTypeArray.h"
#include "mozilla/AntiTrackingCommon.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/dom/BodyExtractor.h"
#include "mozilla/dom/FetchBinding.h"
#include "mozilla/dom/File.h"
#include "nsGeolocation.h"
#include "nsIClassOfService.h"
#include "nsIHttpProtocolHandler.h"
#include "nsIContentPolicy.h"
#include "nsIContentSecurityPolicy.h"
#include "nsContentPolicyUtils.h"
#include "nsISupportsPriority.h"
#include "nsICachingChannel.h"
#include "nsIWebProtocolHandlerRegistrar.h"
#include "nsICookiePermission.h"
#include "nsIScriptSecurityManager.h"
#include "nsCharSeparatedTokenizer.h"
#include "nsContentUtils.h"
#include "nsUnicharUtils.h"
#include "mozilla/Preferences.h"
#include "mozilla/StaticPrefs.h"
#include "mozilla/Telemetry.h"
#include "BatteryManager.h"
#include "mozilla/dom/CredentialsContainer.h"
#include "mozilla/dom/Clipboard.h"
#include "mozilla/dom/FeaturePolicyUtils.h"
#include "mozilla/dom/GamepadServiceTest.h"
#include "mozilla/dom/MediaCapabilities.h"
#include "mozilla/dom/WakeLock.h"
#include "mozilla/dom/power/PowerManagerService.h"
#include "mozilla/dom/MIDIAccessManager.h"
#include "mozilla/dom/MIDIOptionsBinding.h"
#include "mozilla/dom/Permissions.h"
#include "mozilla/dom/Presentation.h"
#include "mozilla/dom/ServiceWorkerContainer.h"
#include "mozilla/dom/StorageManager.h"
#include "mozilla/dom/TCPSocket.h"
#include "mozilla/dom/URLSearchParams.h"
#include "mozilla/dom/VRDisplay.h"
#include "mozilla/dom/VRDisplayEvent.h"
#include "mozilla/dom/VRServiceTest.h"
#include "mozilla/dom/workerinternals/RuntimeService.h"
#include "mozilla/Hal.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/StaticPtr.h"
#include "Connection.h"
#include "mozilla/dom/Event.h"  // for Event
#include "nsGlobalWindow.h"
#include "nsIPermissionManager.h"
#include "nsMimeTypes.h"
#include "nsNetUtil.h"
#include "nsRFPService.h"
#include "nsStringStream.h"
#include "nsComponentManagerUtils.h"
#include "nsICookieService.h"
#include "nsIStringStream.h"
#include "nsIHttpChannel.h"
#include "nsIHttpChannelInternal.h"
#include "nsStreamUtils.h"
#include "WidgetUtils.h"
#include "nsIPresentationService.h"
#include "nsIScriptError.h"
#include "ReferrerInfo.h"

#include "nsIExternalProtocolHandler.h"
#include "BrowserChild.h"
#include "URIUtils.h"

#include "mozilla/dom/MediaDevices.h"
#include "MediaManager.h"

#include "nsIDOMGlobalPropertyInitializer.h"
#include "nsJSUtils.h"

#include "mozilla/dom/NavigatorBinding.h"
#include "mozilla/dom/Promise.h"

#include "nsIUploadChannel2.h"
#include "mozilla/dom/FormData.h"
#include "nsIDocShell.h"

#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/dom/WorkerRunnable.h"

#if defined(XP_LINUX)
#  include "mozilla/Hal.h"
#endif

#include "mozilla/EMEUtils.h"
#include "mozilla/DetailedPromise.h"
#include "mozilla/Unused.h"

namespace mozilla {
namespace dom {

static bool sVibratorEnabled = false;
static uint32_t sMaxVibrateMS = 0;
static uint32_t sMaxVibrateListLen = 0;
static const nsLiteralCString kVibrationPermissionType =
    NS_LITERAL_CSTRING("vibration");

/* static */
void Navigator::Init() {
  Preferences::AddBoolVarCache(&sVibratorEnabled, "dom.vibrator.enabled", true);
  Preferences::AddUintVarCache(&sMaxVibrateMS, "dom.vibrator.max_vibrate_ms",
                               10000);
  Preferences::AddUintVarCache(&sMaxVibrateListLen,
                               "dom.vibrator.max_vibrate_list_len", 128);
}

Navigator::Navigator(nsPIDOMWindowInner* aWindow) : mWindow(aWindow) {}

Navigator::~Navigator() { Invalidate(); }

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(Navigator)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(Navigator)
NS_IMPL_CYCLE_COLLECTING_RELEASE(Navigator)

NS_IMPL_CYCLE_COLLECTION_CLASS(Navigator)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(Navigator)
  tmp->Invalidate();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mWindow)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(Navigator)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mMimeTypes)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPlugins)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPermissions)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mGeolocation)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mBatteryManager)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mBatteryPromise)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mConnection)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mStorageManager)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mCredentials)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mMediaDevices)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mServiceWorkerContainer)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mMediaCapabilities)

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mWindow)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mMediaKeySystemAccessManager)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPresentation)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mGamepadServiceTest)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mVRGetDisplaysPromises)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mVRServiceTest)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_WRAPPERCACHE(Navigator)

void Navigator::Invalidate() {
  // Don't clear mWindow here so we know we've got a non-null mWindow
  // until we're unlinked.

  mMimeTypes = nullptr;

  if (mPlugins) {
    mPlugins->Invalidate();
    mPlugins = nullptr;
  }

  mPermissions = nullptr;

  mStorageManager = nullptr;

  // If there is a page transition, make sure delete the geolocation object.
  if (mGeolocation) {
    mGeolocation->Shutdown();
    mGeolocation = nullptr;
  }

  if (mBatteryManager) {
    mBatteryManager->Shutdown();
    mBatteryManager = nullptr;
  }

  mBatteryPromise = nullptr;

  if (mConnection) {
    mConnection->Shutdown();
    mConnection = nullptr;
  }

  mMediaDevices = nullptr;

  if (mPresentation) {
    mPresentation = nullptr;
  }

  mServiceWorkerContainer = nullptr;

  if (mMediaKeySystemAccessManager) {
    mMediaKeySystemAccessManager->Shutdown();
    mMediaKeySystemAccessManager = nullptr;
  }

  if (mGamepadServiceTest) {
    mGamepadServiceTest->Shutdown();
    mGamepadServiceTest = nullptr;
  }

  mVRGetDisplaysPromises.Clear();

  if (mVRServiceTest) {
    mVRServiceTest->Shutdown();
    mVRServiceTest = nullptr;
  }

  mMediaCapabilities = nullptr;
}

void Navigator::GetUserAgent(nsAString& aUserAgent, CallerType aCallerType,
                             ErrorResult& aRv) const {
  nsCOMPtr<nsPIDOMWindowInner> window;

  if (mWindow) {
    window = mWindow;
    nsIDocShell* docshell = window->GetDocShell();
    nsString customUserAgent;
    if (docshell) {
      docshell->GetCustomUserAgent(customUserAgent);

      if (!customUserAgent.IsEmpty()) {
        aUserAgent = customUserAgent;
        return;
      }
    }
  }

  nsCOMPtr<Document> doc = mWindow->GetExtantDoc();

  nsresult rv = GetUserAgent(window, doc ? doc->NodePrincipal() : nullptr,
                             aCallerType == CallerType::System, aUserAgent);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(rv);
  }
}

void Navigator::GetAppCodeName(nsAString& aAppCodeName, ErrorResult& aRv) {
  nsresult rv;

  nsCOMPtr<nsIHttpProtocolHandler> service(
      do_GetService(NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "http", &rv));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(rv);
    return;
  }

  nsAutoCString appName;
  rv = service->GetAppName(appName);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(rv);
    return;
  }

  CopyASCIItoUTF16(appName, aAppCodeName);
}

void Navigator::GetAppVersion(nsAString& aAppVersion, CallerType aCallerType,
                              ErrorResult& aRv) const {
  nsCOMPtr<Document> doc = mWindow->GetExtantDoc();

  nsresult rv = GetAppVersion(
      aAppVersion, doc ? doc->NodePrincipal() : nullptr,
      /* aUsePrefOverriddenValue = */ aCallerType != CallerType::System);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(rv);
  }
}

void Navigator::GetAppName(nsAString& aAppName, CallerType aCallerType) const {
  nsCOMPtr<Document> doc = mWindow->GetExtantDoc();

  AppName(aAppName, doc ? doc->NodePrincipal() : nullptr,
          /* aUsePrefOverriddenValue = */ aCallerType != CallerType::System);
}

/**
 * Returns the value of Accept-Languages (HTTP header) as a nsTArray of
 * languages. The value is set in the preference by the user ("Content
 * Languages").
 *
 * "en", "en-US" and "i-cherokee" and "" are valid languages tokens.
 *
 * An empty array will be returned if there is no valid languages.
 */
/* static */
void Navigator::GetAcceptLanguages(nsTArray<nsString>& aLanguages) {
  MOZ_ASSERT(NS_IsMainThread());

  aLanguages.Clear();

  // E.g. "de-de, en-us,en".
  nsAutoString acceptLang;
  Preferences::GetLocalizedString("intl.accept_languages", acceptLang);

  // Split values on commas.
  nsCharSeparatedTokenizer langTokenizer(acceptLang, ',');
  while (langTokenizer.hasMoreTokens()) {
    nsDependentSubstring lang = langTokenizer.nextToken();

    // Replace "_" with "-" to avoid POSIX/Windows "en_US" notation.
    // NOTE: we should probably rely on the pref being set correctly.
    if (lang.Length() > 2 && lang[2] == char16_t('_')) {
      lang.Replace(2, 1, char16_t('-'));
    }

    // Use uppercase for country part, e.g. "en-US", not "en-us", see BCP47
    // only uppercase 2-letter country codes, not "zh-Hant", "de-DE-x-goethe".
    // NOTE: we should probably rely on the pref being set correctly.
    if (lang.Length() > 2) {
      nsCharSeparatedTokenizer localeTokenizer(lang, '-');
      int32_t pos = 0;
      bool first = true;
      while (localeTokenizer.hasMoreTokens()) {
        const nsAString& code = localeTokenizer.nextToken();

        if (code.Length() == 2 && !first) {
          nsAutoString upper(code);
          ToUpperCase(upper);
          lang.Replace(pos, code.Length(), upper);
        }

        pos += code.Length() + 1;  // 1 is the separator
        first = false;
      }
    }

    aLanguages.AppendElement(lang);
  }
}

/**
 * Do not use UI language (chosen app locale) here but the first value set in
 * the Accept Languages header, see ::GetAcceptLanguages().
 *
 * See RFC 2616, Section 15.1.4 "Privacy Issues Connected to Accept Headers" for
 * the reasons why.
 */
void Navigator::GetLanguage(nsAString& aLanguage) {
  nsTArray<nsString> languages;
  GetLanguages(languages);
  if (languages.Length() >= 1) {
    aLanguage.Assign(languages[0]);
  } else {
    aLanguage.Truncate();
  }
}

void Navigator::GetLanguages(nsTArray<nsString>& aLanguages) {
  GetAcceptLanguages(aLanguages);

  // The returned value is cached by the binding code. The window listen to the
  // accept languages change and will clear the cache when needed. It has to
  // take care of dispatching the DOM event already and the invalidation and the
  // event has to be timed correctly.
}

void Navigator::GetPlatform(nsAString& aPlatform, CallerType aCallerType,
                            ErrorResult& aRv) const {
  nsCOMPtr<Document> doc = mWindow->GetExtantDoc();

  nsresult rv = GetPlatform(
      aPlatform, doc ? doc->NodePrincipal() : nullptr,
      /* aUsePrefOverriddenValue = */ aCallerType != CallerType::System);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(rv);
  }
}

void Navigator::GetOscpu(nsAString& aOSCPU, CallerType aCallerType,
                         ErrorResult& aRv) const {
  if (aCallerType != CallerType::System) {
    // If fingerprinting resistance is on, we will spoof this value. See
    // nsRFPService.h for details about spoofed values.
    if (nsContentUtils::ShouldResistFingerprinting(GetDocShell())) {
      aOSCPU.AssignLiteral(SPOOFED_OSCPU);
      return;
    }

    nsAutoString override;
    nsresult rv = Preferences::GetString("general.oscpu.override", override);
    if (NS_SUCCEEDED(rv)) {
      aOSCPU = override;
      return;
    }
  }

  nsresult rv;
  nsCOMPtr<nsIHttpProtocolHandler> service(
      do_GetService(NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "http", &rv));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(rv);
    return;
  }

  nsAutoCString oscpu;
  rv = service->GetOscpu(oscpu);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(rv);
    return;
  }

  CopyASCIItoUTF16(oscpu, aOSCPU);
}

void Navigator::GetVendor(nsAString& aVendor) { aVendor.Truncate(); }

void Navigator::GetVendorSub(nsAString& aVendorSub) { aVendorSub.Truncate(); }

void Navigator::GetProduct(nsAString& aProduct) {
  aProduct.AssignLiteral("Gecko");
}

void Navigator::GetProductSub(nsAString& aProductSub) {
  // Legacy build date hardcoded for backward compatibility (bug 776376)
  aProductSub.AssignLiteral(LEGACY_UA_GECKO_TRAIL);
}

nsMimeTypeArray* Navigator::GetMimeTypes(ErrorResult& aRv) {
  if (!mMimeTypes) {
    if (!mWindow) {
      aRv.Throw(NS_ERROR_UNEXPECTED);
      return nullptr;
    }
    mMimeTypes = new nsMimeTypeArray(mWindow);
  }

  return mMimeTypes;
}

nsPluginArray* Navigator::GetPlugins(ErrorResult& aRv) {
  if (!mPlugins) {
    if (!mWindow) {
      aRv.Throw(NS_ERROR_UNEXPECTED);
      return nullptr;
    }
    mPlugins = new nsPluginArray(mWindow);
    mPlugins->Init();
  }

  return mPlugins;
}

Permissions* Navigator::GetPermissions(ErrorResult& aRv) {
  if (!mWindow) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  if (!mPermissions) {
    mPermissions = new Permissions(mWindow);
  }

  return mPermissions;
}

StorageManager* Navigator::Storage() {
  MOZ_ASSERT(mWindow);

  if (!mStorageManager) {
    mStorageManager = new StorageManager(mWindow->AsGlobal());
  }

  return mStorageManager;
}

bool Navigator::CookieEnabled() {
  bool cookieEnabled = (StaticPrefs::network_cookie_cookieBehavior() !=
                        nsICookieService::BEHAVIOR_REJECT);

  // Check whether an exception overrides the global cookie behavior
  // Note that the code for getting the URI here matches that in
  // nsHTMLDocument::SetCookie.
  if (!mWindow || !mWindow->GetDocShell()) {
    return cookieEnabled;
  }

  nsCOMPtr<Document> doc = mWindow->GetExtantDoc();
  if (!doc) {
    return cookieEnabled;
  }

  nsCOMPtr<nsIURI> codebaseURI;
  doc->NodePrincipal()->GetURI(getter_AddRefs(codebaseURI));

  if (!codebaseURI) {
    // Not a codebase, so technically can't set cookies, but let's
    // just return the default value.
    return cookieEnabled;
  }

  uint32_t rejectedReason = 0;
  bool granted = AntiTrackingCommon::IsFirstPartyStorageAccessGrantedFor(
      mWindow, codebaseURI, &rejectedReason);

  AntiTrackingCommon::NotifyBlockingDecision(
      mWindow,
      granted ? AntiTrackingCommon::BlockingDecision::eAllow
              : AntiTrackingCommon::BlockingDecision::eBlock,
      rejectedReason);
  return granted;
}

bool Navigator::OnLine() { return !NS_IsOffline(); }

void Navigator::GetBuildID(nsAString& aBuildID, CallerType aCallerType,
                           ErrorResult& aRv) const {
  if (aCallerType != CallerType::System) {
    // If fingerprinting resistance is on, we will spoof this value. See
    // nsRFPService.h for details about spoofed values.
    if (nsContentUtils::ShouldResistFingerprinting(GetDocShell())) {
      aBuildID.AssignLiteral(LEGACY_BUILD_ID);
      return;
    }

    nsAutoString override;
    nsresult rv = Preferences::GetString("general.buildID.override", override);
    if (NS_SUCCEEDED(rv)) {
      aBuildID = override;
      return;
    }

    nsAutoCString host;
    bool isHTTPS = false;
    if (mWindow) {
      nsCOMPtr<Document> doc = mWindow->GetDoc();
      if (doc) {
        nsIURI* uri = doc->GetDocumentURI();
        if (uri) {
          MOZ_ALWAYS_SUCCEEDS(uri->SchemeIs("https", &isHTTPS));
          if (isHTTPS) {
            MOZ_ALWAYS_SUCCEEDS(uri->GetHost(host));
          }
        }
      }
    }

    // Spoof the buildID on pages not loaded from "https://*.mozilla.org".
    if (!isHTTPS || !StringEndsWith(host, NS_LITERAL_CSTRING(".mozilla.org"))) {
      aBuildID.AssignLiteral(LEGACY_BUILD_ID);
      return;
    }
  }

  nsCOMPtr<nsIXULAppInfo> appInfo =
      do_GetService("@mozilla.org/xre/app-info;1");
  if (!appInfo) {
    aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
    return;
  }

  nsAutoCString buildID;
  nsresult rv = appInfo->GetAppBuildID(buildID);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(rv);
    return;
  }

  aBuildID.Truncate();
  AppendASCIItoUTF16(buildID, aBuildID);
}

void Navigator::GetDoNotTrack(nsAString& aResult) {
  bool doNotTrack = StaticPrefs::privacy_donottrackheader_enabled();
  if (!doNotTrack) {
    nsCOMPtr<nsILoadContext> loadContext = do_GetInterface(mWindow);
    doNotTrack = loadContext && loadContext->UseTrackingProtection();
  }

  if (doNotTrack) {
    aResult.AssignLiteral("1");
  } else {
    aResult.AssignLiteral("unspecified");
  }
}

uint64_t Navigator::HardwareConcurrency() {
  workerinternals::RuntimeService* rts =
      workerinternals::RuntimeService::GetOrCreateService();
  if (!rts) {
    return 1;
  }

  return rts->ClampedHardwareConcurrency();
}

void Navigator::RefreshMIMEArray() {
  if (mMimeTypes) {
    mMimeTypes->Refresh();
  }
}

namespace {

class VibrateWindowListener : public nsIDOMEventListener {
 public:
  VibrateWindowListener(nsPIDOMWindowInner* aWindow, Document* aDocument) {
    mWindow = do_GetWeakReference(aWindow);
    mDocument = do_GetWeakReference(aDocument);

    NS_NAMED_LITERAL_STRING(visibilitychange, "visibilitychange");
    aDocument->AddSystemEventListener(visibilitychange, this, /* listener */
                                      true,                   /* use capture */
                                      false /* wants untrusted */);
  }

  void RemoveListener();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMEVENTLISTENER

 private:
  virtual ~VibrateWindowListener() {}

  nsWeakPtr mWindow;
  nsWeakPtr mDocument;
};

NS_IMPL_ISUPPORTS(VibrateWindowListener, nsIDOMEventListener)

StaticRefPtr<VibrateWindowListener> gVibrateWindowListener;

static bool MayVibrate(Document* doc) {
  // Hidden documents cannot start or stop a vibration.
  return (doc && !doc->Hidden());
}

NS_IMETHODIMP
VibrateWindowListener::HandleEvent(Event* aEvent) {
  nsCOMPtr<Document> doc = do_QueryInterface(aEvent->GetTarget());

  if (!MayVibrate(doc)) {
    // It's important that we call CancelVibrate(), not Vibrate() with an
    // empty list, because Vibrate() will fail if we're no longer focused, but
    // CancelVibrate() will succeed, so long as nobody else has started a new
    // vibration pattern.
    nsCOMPtr<nsPIDOMWindowInner> window = do_QueryReferent(mWindow);
    hal::CancelVibrate(window);
    RemoveListener();
    gVibrateWindowListener = nullptr;
    // Careful: The line above might have deleted |this|!
  }

  return NS_OK;
}

void VibrateWindowListener::RemoveListener() {
  nsCOMPtr<EventTarget> target = do_QueryReferent(mDocument);
  if (!target) {
    return;
  }
  NS_NAMED_LITERAL_STRING(visibilitychange, "visibilitychange");
  target->RemoveSystemEventListener(visibilitychange, this,
                                    true /* use capture */);
}

}  // namespace

void Navigator::SetVibrationPermission(bool aPermitted, bool aPersistent) {
  MOZ_ASSERT(NS_IsMainThread());

  nsTArray<uint32_t> pattern;
  pattern.SwapElements(mRequestedVibrationPattern);

  if (!mWindow) {
    return;
  }

  nsCOMPtr<Document> doc = mWindow->GetExtantDoc();

  if (!MayVibrate(doc)) {
    return;
  }

  if (aPermitted) {
    // Add a listener to cancel the vibration if the document becomes hidden,
    // and remove the old visibility listener, if there was one.
    if (!gVibrateWindowListener) {
      // If gVibrateWindowListener is null, this is the first time we've
      // vibrated, and we need to register a listener to clear
      // gVibrateWindowListener on shutdown.
      ClearOnShutdown(&gVibrateWindowListener);
    } else {
      gVibrateWindowListener->RemoveListener();
    }
    gVibrateWindowListener = new VibrateWindowListener(mWindow, doc);
    hal::Vibrate(pattern, mWindow);
  }

  if (aPersistent) {
    nsCOMPtr<nsIPermissionManager> permMgr = services::GetPermissionManager();
    if (!permMgr) {
      return;
    }
    permMgr->AddFromPrincipal(doc->NodePrincipal(), kVibrationPermissionType,
                              aPermitted ? nsIPermissionManager::ALLOW_ACTION
                                         : nsIPermissionManager::DENY_ACTION,
                              nsIPermissionManager::EXPIRE_SESSION, 0);
  }
}

bool Navigator::Vibrate(uint32_t aDuration) {
  AutoTArray<uint32_t, 1> pattern;
  pattern.AppendElement(aDuration);
  return Vibrate(pattern);
}

bool Navigator::Vibrate(const nsTArray<uint32_t>& aPattern) {
  MOZ_ASSERT(NS_IsMainThread());

  if (!mWindow) {
    return false;
  }

  nsCOMPtr<Document> doc = mWindow->GetExtantDoc();

  if (!MayVibrate(doc)) {
    return false;
  }

  nsTArray<uint32_t> pattern(aPattern);

  if (pattern.Length() > sMaxVibrateListLen) {
    pattern.SetLength(sMaxVibrateListLen);
  }

  for (size_t i = 0; i < pattern.Length(); ++i) {
    pattern[i] = std::min(sMaxVibrateMS, pattern[i]);
  }

  // The spec says we check sVibratorEnabled after we've done the sanity
  // checking on the pattern.
  if (!sVibratorEnabled) {
    return true;
  }

  mRequestedVibrationPattern.SwapElements(pattern);
  nsCOMPtr<nsIPermissionManager> permMgr = services::GetPermissionManager();
  if (!permMgr) {
    return false;
  }

  uint32_t permission = nsIPermissionManager::UNKNOWN_ACTION;

  permMgr->TestPermissionFromPrincipal(doc->NodePrincipal(),
                                       kVibrationPermissionType, &permission);

  if (permission == nsIPermissionManager::ALLOW_ACTION ||
      mRequestedVibrationPattern.IsEmpty() ||
      (mRequestedVibrationPattern.Length() == 1 &&
       mRequestedVibrationPattern[0] == 0)) {
    // Always allow cancelling vibration and respect session permissions.
    SetVibrationPermission(true /* permitted */, false /* persistent */);
    return true;
  }

  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  if (!obs || permission == nsIPermissionManager::DENY_ACTION) {
    // Abort without observer service or on denied session permission.
    SetVibrationPermission(false /* permitted */, false /* persistent */);
    return true;
  }

  // Request user permission.
  obs->NotifyObservers(ToSupports(this), "Vibration:Request", nullptr);

  return true;
}

//*****************************************************************************
//  Pointer Events interface
//*****************************************************************************

uint32_t Navigator::MaxTouchPoints(CallerType aCallerType) {
  // The maxTouchPoints is going to reveal the detail of users' hardware. So,
  // we will spoof it into 0 if fingerprinting resistance is on.
  if (aCallerType != CallerType::System &&
      nsContentUtils::ShouldResistFingerprinting(GetDocShell())) {
    return 0;
  }

  nsCOMPtr<nsIWidget> widget =
      widget::WidgetUtils::DOMWindowToWidget(mWindow->GetOuterWindow());

  NS_ENSURE_TRUE(widget, 0);
  return widget->GetMaxTouchPoints();
}

//*****************************************************************************
//    Navigator::nsIDOMClientInformation
//*****************************************************************************

void Navigator::RegisterContentHandler(const nsAString& aMIMEType,
                                       const nsAString& aURI,
                                       const nsAString& aTitle,
                                       ErrorResult& aRv) {}

// This list should be kept up-to-date with the spec:
// https://html.spec.whatwg.org/multipage/system-state.html#custom-handlers
static const char* const kSafeSchemes[] = {
    "bitcoin", "geo",  "im",   "irc",         "ircs", "magnet", "mailto",
    "mms",     "news", "nntp", "openpgp4fpr", "sip",  "sms",    "smsto",
    "ssh",     "tel",  "urn",  "webcal",      "wtai", "xmpp"};

void Navigator::CheckProtocolHandlerAllowed(const nsAString& aScheme,
                                            nsIURI* aHandlerURI,
                                            nsIURI* aDocumentURI,
                                            ErrorResult& aRv) {
  auto raisePermissionDeniedHandler = [&] {
    nsAutoCString spec;
    aHandlerURI->GetSpec(spec);
    nsPrintfCString message("Permission denied to add %s as a protocol handler",
                            spec.get());
    aRv.ThrowDOMException(NS_ERROR_DOM_SECURITY_ERR, message);
  };

  auto raisePermissionDeniedScheme = [&] {
    nsPrintfCString message(
        "Permission denied to add a protocol handler for %s",
        NS_ConvertUTF16toUTF8(aScheme).get());
    aRv.ThrowDOMException(NS_ERROR_DOM_SECURITY_ERR, message);
  };

  if (!aDocumentURI || !aHandlerURI) {
    aRv.Throw(NS_ERROR_DOM_SYNTAX_ERR);
    return;
  }

  nsCString spec;
  aHandlerURI->GetSpec(spec);
  // If the uri doesn't contain '%s', it won't be a good handler - the %s
  // gets replaced with the handled URI.
  if (!FindInReadable(NS_LITERAL_CSTRING("%s"), spec)) {
    aRv.ThrowDOMException(NS_ERROR_DOM_SYNTAX_ERR);
    return;
  }

  // For security reasons we reject non-http(s) urls (see bug 354316),
  nsAutoCString docScheme;
  nsAutoCString handlerScheme;
  aDocumentURI->GetScheme(docScheme);
  aHandlerURI->GetScheme(handlerScheme);
  if ((!docScheme.EqualsLiteral("https") && !docScheme.EqualsLiteral("http")) ||
      (!handlerScheme.EqualsLiteral("https") &&
       !handlerScheme.EqualsLiteral("http"))) {
    raisePermissionDeniedHandler();
    return;
  }

  // Should be same-origin:
  nsAutoCString handlerHost;
  aHandlerURI->GetHostPort(handlerHost);
  nsAutoCString documentHost;
  aDocumentURI->GetHostPort(documentHost);
  if (!handlerHost.Equals(documentHost) || !handlerScheme.Equals(docScheme)) {
    raisePermissionDeniedHandler();
    return;
  }

  // Having checked the handler URI, check the scheme:
  nsAutoCString scheme;
  ToLowerCase(NS_ConvertUTF16toUTF8(aScheme), scheme);
  if (StringBeginsWith(scheme, NS_LITERAL_CSTRING("web+"))) {
    // Check for non-ascii
    nsReadingIterator<char> iter;
    nsReadingIterator<char> iterEnd;
    auto remainingScheme = Substring(scheme, 4 /* web+ */);
    remainingScheme.BeginReading(iter);
    remainingScheme.EndReading(iterEnd);
    // Scheme suffix must be non-empty
    if (iter == iterEnd) {
      raisePermissionDeniedScheme();
      return;
    }
    for (; iter != iterEnd; iter++) {
      if (*iter < 'a' || *iter > 'z') {
        raisePermissionDeniedScheme();
        return;
      }
    }
  } else {
    bool matches = false;
    for (const char* safeScheme : kSafeSchemes) {
      if (scheme.Equals(safeScheme)) {
        matches = true;
        break;
      }
    }
    if (!matches) {
      raisePermissionDeniedScheme();
      return;
    }
  }

  nsCOMPtr<nsIProtocolHandler> handler;
  nsCOMPtr<nsIIOService> io = services::GetIOService();
  if (NS_FAILED(
          io->GetProtocolHandler(scheme.get(), getter_AddRefs(handler)))) {
    raisePermissionDeniedScheme();
    return;
  }

  // Check to make sure this isn't already handled internally (we don't
  // want to let them take over, say "chrome"). In theory, the checks above
  // should have already taken care of this.
  nsCOMPtr<nsIExternalProtocolHandler> externalHandler =
      do_QueryInterface(handler);
  MOZ_RELEASE_ASSERT(
      externalHandler,
      "We should never allow overriding a builtin protocol handler");

  // check if we have prefs set saying not to add this.
  bool defaultExternal =
      Preferences::GetBool("network.protocol-handler.external-default");
  nsPrintfCString specificPref("network.protocol-handler.external.%s",
                               scheme.get());
  if (!Preferences::GetBool(specificPref.get(), defaultExternal)) {
    raisePermissionDeniedScheme();
    return;
  }
}

void Navigator::RegisterProtocolHandler(const nsAString& aScheme,
                                        const nsAString& aURI,
                                        const nsAString& aTitle,
                                        ErrorResult& aRv) {
  if (!mWindow || !mWindow->GetOuterWindow() || !mWindow->GetDocShell() ||
      !mWindow->GetDoc()) {
    return;
  }
  nsCOMPtr<nsILoadContext> loadContext = do_GetInterface(mWindow);
  if (loadContext->UsePrivateBrowsing()) {
    // If we're a private window, don't alert the user or webpage. We log to the
    // console so that web developers have some way to tell what's going wrong.
    nsContentUtils::ReportToConsole(
        nsIScriptError::warningFlag, NS_LITERAL_CSTRING("DOM"),
        mWindow->GetDoc(), nsContentUtils::eDOM_PROPERTIES,
        "RegisterProtocolHandlerPrivateBrowsingWarning");
    return;
  }

  nsCOMPtr<Document> doc = mWindow->GetDoc();
  if (!mWindow->IsSecureContext()) {
    doc->WarnOnceAbout(Document::eRegisterProtocolHandlerInsecure);
  }

  // Determine if doc is allowed to assign this handler
  nsIURI* docURI = doc->GetDocumentURIObject();
  nsCOMPtr<nsIURI> handlerURI;
  NS_NewURI(getter_AddRefs(handlerURI), NS_ConvertUTF16toUTF8(aURI),
            doc->GetDocumentCharacterSet(), docURI);
  CheckProtocolHandlerAllowed(aScheme, handlerURI, docURI, aRv);
  if (aRv.Failed()) {
    return;
  }

  if (XRE_IsContentProcess()) {
    nsAutoString scheme(aScheme);
    nsAutoString title(aTitle);
    RefPtr<BrowserChild> browserChild = BrowserChild::GetFrom(mWindow);
    browserChild->SendRegisterProtocolHandler(scheme, handlerURI, title,
                                              docURI);
    return;
  }

  nsCOMPtr<nsIWebProtocolHandlerRegistrar> registrar =
      do_GetService(NS_WEBPROTOCOLHANDLERREGISTRAR_CONTRACTID);
  if (registrar) {
    aRv = registrar->RegisterProtocolHandler(aScheme, handlerURI, aTitle,
                                             docURI, mWindow->GetOuterWindow());
  }
}

Geolocation* Navigator::GetGeolocation(ErrorResult& aRv) {
  if (mGeolocation) {
    return mGeolocation;
  }

  if (!mWindow || !mWindow->GetOuterWindow() || !mWindow->GetDocShell()) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  mGeolocation = new Geolocation();
  if (NS_FAILED(mGeolocation->Init(mWindow))) {
    mGeolocation = nullptr;
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  return mGeolocation;
}

class BeaconStreamListener final : public nsIStreamListener {
  ~BeaconStreamListener() {}

 public:
  BeaconStreamListener() : mLoadGroup(nullptr) {}

  void SetLoadGroup(nsILoadGroup* aLoadGroup) { mLoadGroup = aLoadGroup; }

  NS_DECL_ISUPPORTS
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSIREQUESTOBSERVER

 private:
  nsCOMPtr<nsILoadGroup> mLoadGroup;
};

NS_IMPL_ISUPPORTS(BeaconStreamListener, nsIStreamListener, nsIRequestObserver)

NS_IMETHODIMP
BeaconStreamListener::OnStartRequest(nsIRequest* aRequest) {
  // release the loadgroup first
  mLoadGroup = nullptr;

  aRequest->Cancel(NS_ERROR_NET_INTERRUPT);
  return NS_BINDING_ABORTED;
}

NS_IMETHODIMP
BeaconStreamListener::OnStopRequest(nsIRequest* aRequest, nsresult aStatus) {
  return NS_OK;
}

NS_IMETHODIMP
BeaconStreamListener::OnDataAvailable(nsIRequest* aRequest,
                                      nsIInputStream* inStr,
                                      uint64_t sourceOffset, uint32_t count) {
  MOZ_ASSERT(false);
  return NS_OK;
}

bool Navigator::SendBeacon(const nsAString& aUrl,
                           const Nullable<fetch::BodyInit>& aData,
                           ErrorResult& aRv) {
  if (aData.IsNull()) {
    return SendBeaconInternal(aUrl, nullptr, eBeaconTypeOther, aRv);
  }

  if (aData.Value().IsArrayBuffer()) {
    BodyExtractor<const ArrayBuffer> body(&aData.Value().GetAsArrayBuffer());
    return SendBeaconInternal(aUrl, &body, eBeaconTypeArrayBuffer, aRv);
  }

  if (aData.Value().IsArrayBufferView()) {
    BodyExtractor<const ArrayBufferView> body(
        &aData.Value().GetAsArrayBufferView());
    return SendBeaconInternal(aUrl, &body, eBeaconTypeArrayBuffer, aRv);
  }

  if (aData.Value().IsBlob()) {
    BodyExtractor<const Blob> body(&aData.Value().GetAsBlob());
    return SendBeaconInternal(aUrl, &body, eBeaconTypeBlob, aRv);
  }

  if (aData.Value().IsFormData()) {
    BodyExtractor<const FormData> body(&aData.Value().GetAsFormData());
    return SendBeaconInternal(aUrl, &body, eBeaconTypeOther, aRv);
  }

  if (aData.Value().IsUSVString()) {
    BodyExtractor<const nsAString> body(&aData.Value().GetAsUSVString());
    return SendBeaconInternal(aUrl, &body, eBeaconTypeOther, aRv);
  }

  if (aData.Value().IsURLSearchParams()) {
    BodyExtractor<const URLSearchParams> body(
        &aData.Value().GetAsURLSearchParams());
    return SendBeaconInternal(aUrl, &body, eBeaconTypeOther, aRv);
  }

  MOZ_CRASH("Invalid data type.");
  return false;
}

bool Navigator::SendBeaconInternal(const nsAString& aUrl,
                                   BodyExtractorBase* aBody, BeaconType aType,
                                   ErrorResult& aRv) {
  if (!mWindow) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return false;
  }

  nsCOMPtr<Document> doc = mWindow->GetDoc();
  if (!doc) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return false;
  }

  nsIURI* documentURI = doc->GetDocumentURI();
  if (!documentURI) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return false;
  }

  nsCOMPtr<nsIURI> uri;
  nsresult rv = nsContentUtils::NewURIWithDocumentCharset(
      getter_AddRefs(uri), aUrl, doc, doc->GetDocBaseURI());
  if (NS_FAILED(rv)) {
    aRv.ThrowTypeError<MSG_INVALID_URL>(aUrl);
    return false;
  }

  // Spec disallows any schemes save for HTTP/HTTPs
  bool isValidScheme;
  if (!(NS_SUCCEEDED(uri->SchemeIs("http", &isValidScheme)) && isValidScheme) &&
      !(NS_SUCCEEDED(uri->SchemeIs("https", &isValidScheme)) &&
        isValidScheme)) {
    aRv.ThrowTypeError<MSG_INVALID_URL_SCHEME>(NS_LITERAL_STRING("Beacon"),
                                               aUrl);
    return false;
  }

  // No need to use CORS for sendBeacon unless it's a BLOB
  nsSecurityFlags securityFlags =
      aType == eBeaconTypeBlob
          ? nsILoadInfo::SEC_REQUIRE_CORS_DATA_INHERITS
          : nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_DATA_INHERITS;
  securityFlags |= nsILoadInfo::SEC_COOKIES_INCLUDE;

  nsCOMPtr<nsIChannel> channel;
  rv = NS_NewChannel(getter_AddRefs(channel), uri, doc, securityFlags,
                     nsIContentPolicy::TYPE_BEACON);

  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return false;
  }

  nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(channel);
  if (!httpChannel) {
    // Beacon spec only supports HTTP requests at this time
    aRv.Throw(NS_ERROR_DOM_BAD_URI);
    return false;
  }

  nsCOMPtr<nsIReferrerInfo> referrerInfo =
      new ReferrerInfo(doc->GetDocumentURI(), doc->GetReferrerPolicy());
  rv = httpChannel->SetReferrerInfoWithoutClone(referrerInfo);
  MOZ_ASSERT(NS_SUCCEEDED(rv));

  nsCOMPtr<nsIInputStream> in;
  nsAutoCString contentTypeWithCharset;
  nsAutoCString charset;
  uint64_t length = 0;

  if (aBody) {
    aRv = aBody->GetAsStream(getter_AddRefs(in), &length,
                             contentTypeWithCharset, charset);
    if (NS_WARN_IF(aRv.Failed())) {
      return false;
    }

    nsCOMPtr<nsIUploadChannel2> uploadChannel = do_QueryInterface(channel);
    if (!uploadChannel) {
      aRv.Throw(NS_ERROR_FAILURE);
      return false;
    }

    uploadChannel->ExplicitSetUploadStream(in, contentTypeWithCharset, length,
                                           NS_LITERAL_CSTRING("POST"), false);
  } else {
    rv = httpChannel->SetRequestMethod(NS_LITERAL_CSTRING("POST"));
    MOZ_ASSERT(NS_SUCCEEDED(rv));
  }

  nsCOMPtr<nsISupportsPriority> p = do_QueryInterface(channel);
  if (p) {
    p->SetPriority(nsISupportsPriority::PRIORITY_LOWEST);
  }

  nsCOMPtr<nsIClassOfService> cos(do_QueryInterface(channel));
  if (cos) {
    cos->AddClassFlags(nsIClassOfService::Background);
  }

  // The channel needs to have a loadgroup associated with it, so that we can
  // cancel the channel and any redirected channels it may create.
  nsCOMPtr<nsILoadGroup> loadGroup = do_CreateInstance(NS_LOADGROUP_CONTRACTID);
  nsCOMPtr<nsIInterfaceRequestor> callbacks =
      do_QueryInterface(mWindow->GetDocShell());
  loadGroup->SetNotificationCallbacks(callbacks);
  channel->SetLoadGroup(loadGroup);

  RefPtr<BeaconStreamListener> beaconListener = new BeaconStreamListener();
  rv = channel->AsyncOpen(beaconListener);
  // do not throw if security checks fail within asyncOpen
  NS_ENSURE_SUCCESS(rv, false);

  // make the beaconListener hold a strong reference to the loadgroup
  // which is released in ::OnStartRequest
  beaconListener->SetLoadGroup(loadGroup);

  return true;
}

MediaDevices* Navigator::GetMediaDevices(ErrorResult& aRv) {
  if (!mMediaDevices) {
    if (!mWindow || !mWindow->GetOuterWindow() ||
        mWindow->GetOuterWindow()->GetCurrentInnerWindow() != mWindow) {
      aRv.Throw(NS_ERROR_NOT_AVAILABLE);
      return nullptr;
    }
    mMediaDevices = new MediaDevices(mWindow);
  }
  return mMediaDevices;
}

void Navigator::MozGetUserMedia(const MediaStreamConstraints& aConstraints,
                                NavigatorUserMediaSuccessCallback& aOnSuccess,
                                NavigatorUserMediaErrorCallback& aOnError,
                                CallerType aCallerType, ErrorResult& aRv) {
  MOZ_ASSERT(NS_IsMainThread());

  if (!mWindow || !mWindow->GetOuterWindow() ||
      mWindow->GetOuterWindow()->GetCurrentInnerWindow() != mWindow) {
    aRv.Throw(NS_ERROR_NOT_AVAILABLE);
    return;
  }

  RefPtr<NavigatorUserMediaSuccessCallback> onsuccess(&aOnSuccess);
  RefPtr<NavigatorUserMediaErrorCallback> onerror(&aOnError);

  nsWeakPtr weakWindow = nsWeakPtr(do_GetWeakReference(mWindow));

  MediaManager::Get()
      ->GetUserMedia(mWindow, aConstraints, aCallerType)
      ->Then(
          GetMainThreadSerialEventTarget(), __func__,
          [weakWindow, onsuccess = std::move(onsuccess)](
              const RefPtr<DOMMediaStream>& aStream) MOZ_CAN_RUN_SCRIPT {
            nsCOMPtr<nsPIDOMWindowInner> window = do_QueryReferent(weakWindow);
            if (!window || !window->GetOuterWindow() ||
                window->GetOuterWindow()->GetCurrentInnerWindow() != window) {
              return;  // Leave Promise pending after navigation by design.
            }
            MediaManager::CallOnSuccess(*onsuccess, *aStream);
          },
          [weakWindow, onerror = std::move(onerror)](
              const RefPtr<MediaMgrError>& aError) MOZ_CAN_RUN_SCRIPT {
            nsCOMPtr<nsPIDOMWindowInner> window = do_QueryReferent(weakWindow);
            if (!window || !window->GetOuterWindow() ||
                window->GetOuterWindow()->GetCurrentInnerWindow() != window) {
              return;  // Leave Promise pending after navigation by design.
            }
            auto error = MakeRefPtr<MediaStreamError>(window, *aError);
            MediaManager::CallOnError(*onerror, *error);
          });
}

void Navigator::MozGetUserMediaDevices(
    const MediaStreamConstraints& aConstraints,
    MozGetUserMediaDevicesSuccessCallback& aOnSuccess,
    NavigatorUserMediaErrorCallback& aOnError, uint64_t aInnerWindowID,
    const nsAString& aCallID, ErrorResult& aRv) {
  if (!mWindow || !mWindow->GetOuterWindow() ||
      mWindow->GetOuterWindow()->GetCurrentInnerWindow() != mWindow) {
    aRv.Throw(NS_ERROR_NOT_AVAILABLE);
    return;
  }
  if (Document* doc = mWindow->GetExtantDoc()) {
    if (!mWindow->IsSecureContext()) {
      doc->SetDocumentAndPageUseCounter(
          eUseCounter_custom_MozGetUserMediaInsec);
    }
    nsINode* node = doc;
    while ((node = nsContentUtils::GetCrossDocParentNode(node))) {
      if (NS_FAILED(nsContentUtils::CheckSameOrigin(doc, node))) {
        doc->SetDocumentAndPageUseCounter(
            eUseCounter_custom_MozGetUserMediaXOrigin);
        break;
      }
    }
  }
  RefPtr<MediaManager> manager = MediaManager::Get();
  // XXXbz aOnError seems to be unused?
  nsCOMPtr<nsPIDOMWindowInner> window(mWindow);
  aRv = manager->GetUserMediaDevices(window, aConstraints, aOnSuccess,
                                     aInnerWindowID, aCallID);
}

//*****************************************************************************
//    Navigator::nsINavigatorBattery
//*****************************************************************************

Promise* Navigator::GetBattery(ErrorResult& aRv) {
  if (mBatteryPromise) {
    return mBatteryPromise;
  }

  if (!mWindow || !mWindow->GetDocShell()) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  RefPtr<Promise> batteryPromise = Promise::Create(mWindow->AsGlobal(), aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }
  mBatteryPromise = batteryPromise;

  if (!mBatteryManager) {
    mBatteryManager = new battery::BatteryManager(mWindow);
    mBatteryManager->Init();
  }

  mBatteryPromise->MaybeResolve(mBatteryManager);

  return mBatteryPromise;
}

already_AddRefed<LegacyMozTCPSocket> Navigator::MozTCPSocket() {
  RefPtr<LegacyMozTCPSocket> socket = new LegacyMozTCPSocket(GetWindow());
  return socket.forget();
}

void Navigator::GetGamepads(nsTArray<RefPtr<Gamepad>>& aGamepads,
                            ErrorResult& aRv) {
  if (!mWindow) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return;
  }
  NS_ENSURE_TRUE_VOID(mWindow->GetDocShell());
  nsGlobalWindowInner* win = nsGlobalWindowInner::Cast(mWindow);
  win->SetHasGamepadEventListener(true);
  win->GetGamepads(aGamepads);
}

GamepadServiceTest* Navigator::RequestGamepadServiceTest() {
  if (!mGamepadServiceTest) {
    mGamepadServiceTest = GamepadServiceTest::CreateTestService(mWindow);
  }
  return mGamepadServiceTest;
}

already_AddRefed<Promise> Navigator::GetVRDisplays(ErrorResult& aRv) {
  if (!mWindow || !mWindow->GetDocShell() || !mWindow->GetExtantDoc()) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  if (!FeaturePolicyUtils::IsFeatureAllowed(mWindow->GetExtantDoc(),
                                            NS_LITERAL_STRING("vr"))) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return nullptr;
  }

  nsGlobalWindowInner* win = nsGlobalWindowInner::Cast(mWindow);
  win->NotifyVREventListenerAdded();

  RefPtr<Promise> p = Promise::Create(mWindow->AsGlobal(), aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  // We pass mWindow's id to RefreshVRDisplays, so NotifyVRDisplaysUpdated will
  // be called asynchronously, resolving the promises in mVRGetDisplaysPromises.
  if (!VRDisplay::RefreshVRDisplays(win->WindowID())) {
    p->MaybeReject(NS_ERROR_FAILURE);
    return p.forget();
  }

  mVRGetDisplaysPromises.AppendElement(p);
  return p.forget();
}

void Navigator::GetActiveVRDisplays(
    nsTArray<RefPtr<VRDisplay>>& aDisplays) const {
  /**
   * Get only the active VR displays.
   * GetActiveVRDisplays should only enumerate displays that
   * are already active without causing any other hardware to be
   * activated.
   * We must not call nsGlobalWindow::NotifyVREventListenerAdded here,
   * as that would cause enumeration and activation of other VR hardware.
   * Activating VR hardware is intrusive to the end user, as it may
   * involve physically powering on devices that the user did not
   * intend to use.
   */
  if (!mWindow || !mWindow->GetDocShell()) {
    return;
  }
  nsGlobalWindowInner* win = nsGlobalWindowInner::Cast(mWindow);
  nsTArray<RefPtr<VRDisplay>> displays;
  if (win->UpdateVRDisplays(displays)) {
    for (auto display : displays) {
      if (display->IsPresenting()) {
        aDisplays.AppendElement(display);
      }
    }
  }
}

void Navigator::NotifyVRDisplaysUpdated() {
  // Synchronize the VR devices and resolve the promises in
  // mVRGetDisplaysPromises
  nsGlobalWindowInner* win = nsGlobalWindowInner::Cast(mWindow);

  nsTArray<RefPtr<VRDisplay>> vrDisplays;
  if (win->UpdateVRDisplays(vrDisplays)) {
    for (auto p : mVRGetDisplaysPromises) {
      p->MaybeResolve(vrDisplays);
    }
  } else {
    for (auto p : mVRGetDisplaysPromises) {
      p->MaybeReject(NS_ERROR_FAILURE);
    }
  }
  mVRGetDisplaysPromises.Clear();
}

void Navigator::NotifyActiveVRDisplaysChanged() {
  Navigator_Binding::ClearCachedActiveVRDisplaysValue(this);
}

VRServiceTest* Navigator::RequestVRServiceTest() {
  // Ensure that the Mock VR devices are not released prematurely
  nsGlobalWindowInner* win = nsGlobalWindowInner::Cast(mWindow);
  win->NotifyVREventListenerAdded();

  if (!mVRServiceTest) {
    mVRServiceTest = VRServiceTest::CreateTestService(mWindow);
  }
  return mVRServiceTest;
}

bool Navigator::IsWebVRContentDetected() const {
  nsGlobalWindowInner* win = nsGlobalWindowInner::Cast(mWindow);
  return win->IsVRContentDetected();
}

bool Navigator::IsWebVRContentPresenting() const {
  nsGlobalWindowInner* win = nsGlobalWindowInner::Cast(mWindow);
  return win->IsVRContentPresenting();
}

void Navigator::RequestVRPresentation(VRDisplay& aDisplay) {
  nsGlobalWindowInner* win = nsGlobalWindowInner::Cast(mWindow);
  win->DispatchVRDisplayActivate(aDisplay.DisplayId(),
                                 VRDisplayEventReason::Requested);
}

already_AddRefed<Promise> Navigator::RequestMIDIAccess(
    const MIDIOptions& aOptions, ErrorResult& aRv) {
  if (!mWindow) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }
  MIDIAccessManager* accessMgr = MIDIAccessManager::Get();
  return accessMgr->RequestMIDIAccess(mWindow, aOptions, aRv);
}

network::Connection* Navigator::GetConnection(ErrorResult& aRv) {
  if (!mConnection) {
    if (!mWindow) {
      aRv.Throw(NS_ERROR_UNEXPECTED);
      return nullptr;
    }
    mConnection = network::Connection::CreateForWindow(mWindow);
  }

  return mConnection;
}

already_AddRefed<ServiceWorkerContainer> Navigator::ServiceWorker() {
  MOZ_ASSERT(mWindow);

  if (!mServiceWorkerContainer) {
    mServiceWorkerContainer =
        ServiceWorkerContainer::Create(mWindow->AsGlobal());
  }

  RefPtr<ServiceWorkerContainer> ref = mServiceWorkerContainer;
  return ref.forget();
}

size_t Navigator::SizeOfIncludingThis(
    mozilla::MallocSizeOf aMallocSizeOf) const {
  size_t n = aMallocSizeOf(this);

  // TODO: add SizeOfIncludingThis() to nsMimeTypeArray, bug 674113.
  // TODO: add SizeOfIncludingThis() to nsPluginArray, bug 674114.
  // TODO: add SizeOfIncludingThis() to Geolocation, bug 674115.
  // TODO: add SizeOfIncludingThis() to DesktopNotificationCenter, bug 674116.

  return n;
}

void Navigator::SetWindow(nsPIDOMWindowInner* aInnerWindow) {
  mWindow = aInnerWindow;
}

void Navigator::OnNavigation() {
  if (!mWindow) {
    return;
  }

  // If MediaManager is open let it inform any live streams or pending callbacks
  MediaManager* manager = MediaManager::GetIfExists();
  if (manager) {
    manager->OnNavigation(mWindow->WindowID());
  }
}

JSObject* Navigator::WrapObject(JSContext* cx,
                                JS::Handle<JSObject*> aGivenProto) {
  return Navigator_Binding::Wrap(cx, this, aGivenProto);
}

/* static */
bool Navigator::HasUserMediaSupport(JSContext* cx, JSObject* obj) {
  // Make enabling peerconnection enable getUserMedia() as well.
  // Emulate [SecureContext] unless media.devices.insecure.enabled=true
  return (StaticPrefs::media_navigator_enabled() ||
          StaticPrefs::media_peerconnection_enabled()) &&
         (IsSecureContextOrObjectIsFromSecureContext(cx, obj) ||
          StaticPrefs::media_devices_insecure_enabled());
}

/* static */
already_AddRefed<nsPIDOMWindowInner> Navigator::GetWindowFromGlobal(
    JSObject* aGlobal) {
  nsCOMPtr<nsPIDOMWindowInner> win = xpc::WindowOrNull(aGlobal);
  return win.forget();
}

nsresult Navigator::GetPlatform(nsAString& aPlatform,
                                nsIPrincipal* aCallerPrincipal,
                                bool aUsePrefOverriddenValue) {
  MOZ_ASSERT(NS_IsMainThread());

  if (aUsePrefOverriddenValue) {
    // If fingerprinting resistance is on, we will spoof this value. See
    // nsRFPService.h for details about spoofed values.
    if (nsContentUtils::ShouldResistFingerprinting(aCallerPrincipal)) {
      aPlatform.AssignLiteral(SPOOFED_PLATFORM);
      return NS_OK;
    }
    nsAutoString override;
    nsresult rv =
        mozilla::Preferences::GetString("general.platform.override", override);

    if (NS_SUCCEEDED(rv)) {
      aPlatform = override;
      return NS_OK;
    }
  }

  nsresult rv;

  nsCOMPtr<nsIHttpProtocolHandler> service(
      do_GetService(NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "http", &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  // Sorry for the #if platform ugliness, but Communicator is likewise
  // hardcoded and we are seeking backward compatibility here (bug 47080).
#if defined(WIN32)
  aPlatform.AssignLiteral("Win32");
#elif defined(XP_MACOSX) && defined(__ppc__)
  aPlatform.AssignLiteral("MacPPC");
#elif defined(XP_MACOSX) && defined(__i386__)
  aPlatform.AssignLiteral("MacIntel");
#elif defined(XP_MACOSX) && defined(__x86_64__)
  aPlatform.AssignLiteral("MacIntel");
#else
  // XXX Communicator uses compiled-in build-time string defines
  // to indicate the platform it was compiled *for*, not what it is
  // currently running *on* which is what this does.
  nsAutoCString plat;
  rv = service->GetOscpu(plat);
  CopyASCIItoUTF16(plat, aPlatform);
#endif

  return rv;
}

/* static */
nsresult Navigator::GetAppVersion(nsAString& aAppVersion,
                                  nsIPrincipal* aCallerPrincipal,
                                  bool aUsePrefOverriddenValue) {
  MOZ_ASSERT(NS_IsMainThread());

  if (aUsePrefOverriddenValue) {
    // If fingerprinting resistance is on, we will spoof this value. See
    // nsRFPService.h for details about spoofed values.
    if (nsContentUtils::ShouldResistFingerprinting(aCallerPrincipal)) {
      aAppVersion.AssignLiteral(SPOOFED_APPVERSION);
      return NS_OK;
    }
    nsAutoString override;
    nsresult rv = mozilla::Preferences::GetString("general.appversion.override",
                                                  override);

    if (NS_SUCCEEDED(rv)) {
      aAppVersion = override;
      return NS_OK;
    }
  }

  nsresult rv;

  nsCOMPtr<nsIHttpProtocolHandler> service(
      do_GetService(NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "http", &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString str;
  rv = service->GetAppVersion(str);
  CopyASCIItoUTF16(str, aAppVersion);
  NS_ENSURE_SUCCESS(rv, rv);

  aAppVersion.AppendLiteral(" (");

  rv = service->GetPlatform(str);
  NS_ENSURE_SUCCESS(rv, rv);

  AppendASCIItoUTF16(str, aAppVersion);
  aAppVersion.Append(char16_t(')'));

  return rv;
}

/* static */
void Navigator::AppName(nsAString& aAppName, nsIPrincipal* aCallerPrincipal,
                        bool aUsePrefOverriddenValue) {
  MOZ_ASSERT(NS_IsMainThread());

  if (aUsePrefOverriddenValue) {
    // If fingerprinting resistance is on, we will spoof this value. See
    // nsRFPService.h for details about spoofed values.
    if (nsContentUtils::ShouldResistFingerprinting(aCallerPrincipal)) {
      aAppName.AssignLiteral(SPOOFED_APPNAME);
      return;
    }

    nsAutoString override;
    nsresult rv =
        mozilla::Preferences::GetString("general.appname.override", override);

    if (NS_SUCCEEDED(rv)) {
      aAppName = override;
      return;
    }
  }

  aAppName.AssignLiteral("Netscape");
}

void Navigator::ClearUserAgentCache() {
  Navigator_Binding::ClearCachedUserAgentValue(this);
}

nsresult Navigator::GetUserAgent(nsPIDOMWindowInner* aWindow,
                                 nsIPrincipal* aCallerPrincipal,
                                 bool aIsCallerChrome, nsAString& aUserAgent) {
  MOZ_ASSERT(NS_IsMainThread());

  // We will skip the override and pass to httpHandler to get spoofed userAgent
  // when 'privacy.resistFingerprinting' is true.
  if (!aIsCallerChrome &&
      !nsContentUtils::ShouldResistFingerprinting(aCallerPrincipal)) {
    nsAutoString override;
    nsresult rv =
        mozilla::Preferences::GetString("general.useragent.override", override);

    if (NS_SUCCEEDED(rv)) {
      aUserAgent = override;
      return NS_OK;
    }
  }

  // When the caller is content and 'privacy.resistFingerprinting' is true,
  // return a spoofed userAgent which reveals the platform but not the
  // specific OS version, etc.
  if (!aIsCallerChrome &&
      nsContentUtils::ShouldResistFingerprinting(aCallerPrincipal)) {
    nsAutoCString spoofedUA;
    nsRFPService::GetSpoofedUserAgent(spoofedUA, false);
    CopyASCIItoUTF16(spoofedUA, aUserAgent);
    return NS_OK;
  }

  nsresult rv;
  nsCOMPtr<nsIHttpProtocolHandler> service(
      do_GetService(NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "http", &rv));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsAutoCString ua;
  rv = service->GetUserAgent(ua);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  CopyASCIItoUTF16(ua, aUserAgent);

  // When the caller is content, we will always return spoofed userAgent and
  // ignore the User-Agent header from the document channel when
  // 'privacy.resistFingerprinting' is true.
  if (!aWindow ||
      (nsContentUtils::ShouldResistFingerprinting(aCallerPrincipal) &&
       !aIsCallerChrome)) {
    return NS_OK;
  }

  // Copy the User-Agent header from the document channel which has already been
  // subject to UA overrides.
  nsCOMPtr<Document> doc = aWindow->GetExtantDoc();
  if (!doc) {
    return NS_OK;
  }
  nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(doc->GetChannel());
  if (httpChannel) {
    nsAutoCString userAgent;
    rv = httpChannel->GetRequestHeader(NS_LITERAL_CSTRING("User-Agent"),
                                       userAgent);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    CopyASCIItoUTF16(userAgent, aUserAgent);
  }
  return NS_OK;
}

static nsCString RequestKeySystemAccessLogString(
    const nsAString& aKeySystem,
    const Sequence<MediaKeySystemConfiguration>& aConfigs,
    bool aIsSecureContext) {
  nsCString str;
  str.AppendPrintf(
      "Navigator::RequestMediaKeySystemAccess(keySystem='%s' options=",
      NS_ConvertUTF16toUTF8(aKeySystem).get());
  str.Append(MediaKeySystemAccess::ToCString(aConfigs));
  str.AppendLiteral(") secureContext=");
  str.AppendInt(aIsSecureContext);
  return str;
}

already_AddRefed<Promise> Navigator::RequestMediaKeySystemAccess(
    const nsAString& aKeySystem,
    const Sequence<MediaKeySystemConfiguration>& aConfigs, ErrorResult& aRv) {
  EME_LOG("%s", RequestKeySystemAccessLogString(aKeySystem, aConfigs,
                                                mWindow->IsSecureContext())
                    .get());

  Telemetry::Accumulate(Telemetry::MEDIA_EME_SECURE_CONTEXT,
                        mWindow->IsSecureContext());

  if (!mWindow->IsSecureContext()) {
    Document* doc = mWindow->GetExtantDoc();
    nsString uri;
    if (doc) {
      Unused << doc->GetDocumentURI(uri);
    }
    const char16_t* params[] = {uri.get()};
    nsContentUtils::ReportToConsole(nsIScriptError::warningFlag,
                                    NS_LITERAL_CSTRING("Media"), doc,
                                    nsContentUtils::eDOM_PROPERTIES,
                                    "MediaEMEInsecureContextDeprecatedWarning",
                                    params, ArrayLength(params));
  }

  Document* doc = mWindow->GetExtantDoc();
  if (doc && !FeaturePolicyUtils::IsFeatureAllowed(
                 doc, NS_LITERAL_STRING("encrypted-media"))) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return nullptr;
  }

  RefPtr<DetailedPromise> promise = DetailedPromise::Create(
      mWindow->AsGlobal(), aRv,
      NS_LITERAL_CSTRING("navigator.requestMediaKeySystemAccess"),
      Telemetry::VIDEO_EME_REQUEST_SUCCESS_LATENCY_MS,
      Telemetry::VIDEO_EME_REQUEST_FAILURE_LATENCY_MS);
  if (aRv.Failed()) {
    return nullptr;
  }

  if (!mMediaKeySystemAccessManager) {
    mMediaKeySystemAccessManager = new MediaKeySystemAccessManager(mWindow);
  }

  mMediaKeySystemAccessManager->Request(promise, aKeySystem, aConfigs);
  return promise.forget();
}

Presentation* Navigator::GetPresentation(ErrorResult& aRv) {
  if (!mPresentation) {
    if (!mWindow) {
      aRv.Throw(NS_ERROR_UNEXPECTED);
      return nullptr;
    }
    mPresentation = Presentation::Create(mWindow);
  }

  return mPresentation;
}

CredentialsContainer* Navigator::Credentials() {
  if (!mCredentials) {
    mCredentials = new CredentialsContainer(GetWindow());
  }
  return mCredentials;
}

dom::MediaCapabilities* Navigator::MediaCapabilities() {
  if (!mMediaCapabilities) {
    mMediaCapabilities = new dom::MediaCapabilities(GetWindow()->AsGlobal());
  }
  return mMediaCapabilities;
}

Clipboard* Navigator::Clipboard() {
  if (!mClipboard) {
    mClipboard = new dom::Clipboard(GetWindow());
  }
  return mClipboard;
}

/* static */
bool Navigator::Webdriver() {
  return Preferences::GetBool("marionette.enabled", false);
}

}  // namespace dom
}  // namespace mozilla
