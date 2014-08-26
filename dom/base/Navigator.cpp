/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Needs to be first.
#include "base/basictypes.h"

#include "Navigator.h"
#include "nsIXULAppInfo.h"
#include "nsPluginArray.h"
#include "nsMimeTypeArray.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/dom/DesktopNotification.h"
#include "nsGeolocation.h"
#include "nsIHttpProtocolHandler.h"
#include "nsIContentPolicy.h"
#include "nsIContentSecurityPolicy.h"
#include "nsContentPolicyUtils.h"
#include "nsCrossSiteListenerProxy.h"
#include "nsISupportsPriority.h"
#include "nsICachingChannel.h"
#include "nsIWebContentHandlerRegistrar.h"
#include "nsICookiePermission.h"
#include "nsIScriptSecurityManager.h"
#include "nsCharSeparatedTokenizer.h"
#include "nsContentUtils.h"
#include "nsUnicharUtils.h"
#include "mozilla/Preferences.h"
#include "mozilla/Telemetry.h"
#include "BatteryManager.h"
#include "mozilla/dom/PowerManager.h"
#include "mozilla/dom/WakeLock.h"
#include "mozilla/dom/power/PowerManagerService.h"
#include "mozilla/dom/MobileMessageManager.h"
#include "mozilla/dom/ServiceWorkerContainer.h"
#include "mozilla/dom/Telephony.h"
#include "mozilla/Hal.h"
#include "nsISiteSpecificUserAgent.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/StaticPtr.h"
#include "Connection.h"
#include "mozilla/dom/Event.h" // for nsIDOMEvent::InternalDOMEvent()
#include "nsGlobalWindow.h"
#ifdef MOZ_B2G
#include "nsIMobileIdentityService.h"
#endif
#ifdef MOZ_B2G_RIL
#include "mozilla/dom/IccManager.h"
#include "mozilla/dom/CellBroadcast.h"
#include "mozilla/dom/MobileConnectionArray.h"
#include "mozilla/dom/Voicemail.h"
#endif
#include "nsIIdleObserver.h"
#include "nsIPermissionManager.h"
#include "nsMimeTypes.h"
#include "nsNetUtil.h"
#include "nsIHttpChannel.h"
#include "nsIHttpChannelInternal.h"
#include "TimeManager.h"
#include "DeviceStorage.h"
#include "nsIDOMNavigatorSystemMessages.h"
#include "nsStreamUtils.h"
#include "nsIAppsService.h"
#include "mozIApplication.h"
#include "WidgetUtils.h"
#include "mozIThirdPartyUtil.h"
#include "nsChannelPolicy.h"

#ifdef MOZ_MEDIA_NAVIGATOR
#include "MediaManager.h"
#endif
#ifdef MOZ_B2G_BT
#include "BluetoothManager.h"
#endif
#include "DOMCameraManager.h"

#ifdef MOZ_AUDIO_CHANNEL_MANAGER
#include "AudioChannelManager.h"
#endif

#ifdef MOZ_B2G_FM
#include "mozilla/dom/FMRadio.h"
#endif

#include "nsIDOMGlobalPropertyInitializer.h"
#include "mozilla/dom/DataStoreService.h"
#include "nsJSUtils.h"

#include "nsScriptNameSpaceManager.h"

#include "mozilla/dom/NavigatorBinding.h"
#include "mozilla/dom/Promise.h"

#include "nsIUploadChannel2.h"
#include "nsFormData.h"
#include "nsIPrivateBrowsingChannel.h"
#include "nsIDocShell.h"

#include "WorkerPrivate.h"
#include "WorkerRunnable.h"

#if defined(XP_LINUX)
#include "mozilla/Hal.h"
#endif
#include "mozilla/dom/ContentChild.h"

#include "mozilla/dom/FeatureList.h"

namespace mozilla {
namespace dom {

static bool sDoNotTrackEnabled = false;
static uint32_t sDoNotTrackValue = 1;
static bool sVibratorEnabled   = false;
static uint32_t sMaxVibrateMS  = 0;
static uint32_t sMaxVibrateListLen = 0;

/* static */
void
Navigator::Init()
{
  Preferences::AddBoolVarCache(&sDoNotTrackEnabled,
                               "privacy.donottrackheader.enabled",
                               false);
  Preferences::AddUintVarCache(&sDoNotTrackValue,
                               "privacy.donottrackheader.value",
                               1);
  Preferences::AddBoolVarCache(&sVibratorEnabled,
                               "dom.vibrator.enabled", true);
  Preferences::AddUintVarCache(&sMaxVibrateMS,
                               "dom.vibrator.max_vibrate_ms", 10000);
  Preferences::AddUintVarCache(&sMaxVibrateListLen,
                               "dom.vibrator.max_vibrate_list_len", 128);
}

Navigator::Navigator(nsPIDOMWindow* aWindow)
  : mWindow(aWindow)
{
  MOZ_ASSERT(aWindow->IsInnerWindow(), "Navigator must get an inner window!");
  SetIsDOMBinding();
}

Navigator::~Navigator()
{
  Invalidate();
}

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(Navigator)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMNavigator)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNavigator)
  NS_INTERFACE_MAP_ENTRY(nsIMozNavigatorNetwork)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(Navigator)
NS_IMPL_CYCLE_COLLECTING_RELEASE(Navigator)

NS_IMPL_CYCLE_COLLECTION_CLASS(Navigator)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(Navigator)
  tmp->Invalidate();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mWindow)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mCachedResolveResults)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(Navigator)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPlugins)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mMimeTypes)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mGeolocation)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mNotification)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mBatteryManager)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPowerManager)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mMobileMessageManager)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mTelephony)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mConnection)
#ifdef MOZ_B2G_RIL
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mMobileConnections)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mCellBroadcast)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mIccManager)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mVoicemail)
#endif
#ifdef MOZ_B2G_BT
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mBluetooth)
#endif
#ifdef MOZ_AUDIO_CHANNEL_MANAGER
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mAudioChannelManager)
#endif
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mCameraManager)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mMessagesManager)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDeviceStorageStores)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mTimeManager)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mServiceWorkerContainer)

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mWindow)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mCachedResolveResults)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_WRAPPERCACHE(Navigator)

void
Navigator::Invalidate()
{
  // Don't clear mWindow here so we know we've got a non-null mWindow
  // until we're unlinked.

  if (mPlugins) {
    mPlugins->Invalidate();
    mPlugins = nullptr;
  }

  mMimeTypes = nullptr;

  // If there is a page transition, make sure delete the geolocation object.
  if (mGeolocation) {
    mGeolocation->Shutdown();
    mGeolocation = nullptr;
  }

  if (mNotification) {
    mNotification->Shutdown();
    mNotification = nullptr;
  }

  if (mBatteryManager) {
    mBatteryManager->Shutdown();
    mBatteryManager = nullptr;
  }

#ifdef MOZ_B2G_FM
  if (mFMRadio) {
    mFMRadio->Shutdown();
    mFMRadio = nullptr;
  }
#endif

  if (mPowerManager) {
    mPowerManager->Shutdown();
    mPowerManager = nullptr;
  }

  if (mMobileMessageManager) {
    mMobileMessageManager->Shutdown();
    mMobileMessageManager = nullptr;
  }

  if (mTelephony) {
    mTelephony = nullptr;
  }

  if (mConnection) {
    mConnection->Shutdown();
    mConnection = nullptr;
  }

#ifdef MOZ_B2G_RIL
  if (mMobileConnections) {
    mMobileConnections = nullptr;
  }

  if (mCellBroadcast) {
    mCellBroadcast = nullptr;
  }

  if (mIccManager) {
    mIccManager->Shutdown();
    mIccManager = nullptr;
  }

  if (mVoicemail) {
    mVoicemail = nullptr;
  }
#endif

#ifdef MOZ_B2G_BT
  if (mBluetooth) {
    mBluetooth = nullptr;
  }
#endif

  mCameraManager = nullptr;

  if (mMessagesManager) {
    mMessagesManager = nullptr;
  }

#ifdef MOZ_AUDIO_CHANNEL_MANAGER
  if (mAudioChannelManager) {
    mAudioChannelManager = nullptr;
  }
#endif

  uint32_t len = mDeviceStorageStores.Length();
  for (uint32_t i = 0; i < len; ++i) {
    mDeviceStorageStores[i]->Shutdown();
  }
  mDeviceStorageStores.Clear();

  if (mTimeManager) {
    mTimeManager = nullptr;
  }

  mServiceWorkerContainer = nullptr;
}

//*****************************************************************************
//    Navigator::nsIDOMNavigator
//*****************************************************************************

NS_IMETHODIMP
Navigator::GetUserAgent(nsAString& aUserAgent)
{
  nsresult rv = NS_GetNavigatorUserAgent(aUserAgent);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!mWindow || !mWindow->GetDocShell()) {
    return NS_OK;
  }

  nsIDocument* doc = mWindow->GetExtantDoc();
  if (!doc) {
    return NS_OK;
  }

  nsCOMPtr<nsIURI> codebaseURI;
  doc->NodePrincipal()->GetURI(getter_AddRefs(codebaseURI));
  if (!codebaseURI) {
    return NS_OK;
  }

  nsCOMPtr<nsISiteSpecificUserAgent> siteSpecificUA =
    do_GetService("@mozilla.org/dom/site-specific-user-agent;1");
  NS_ENSURE_TRUE(siteSpecificUA, NS_OK);

  return siteSpecificUA->GetUserAgentForURIAndWindow(codebaseURI, mWindow,
                                                     aUserAgent);
}

NS_IMETHODIMP
Navigator::GetAppCodeName(nsAString& aAppCodeName)
{
  nsresult rv;

  nsCOMPtr<nsIHttpProtocolHandler>
    service(do_GetService(NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "http", &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString appName;
  rv = service->GetAppName(appName);
  CopyASCIItoUTF16(appName, aAppCodeName);

  return rv;
}

NS_IMETHODIMP
Navigator::GetAppVersion(nsAString& aAppVersion)
{
  return NS_GetNavigatorAppVersion(aAppVersion);
}

NS_IMETHODIMP
Navigator::GetAppName(nsAString& aAppName)
{
  NS_GetNavigatorAppName(aAppName);
  return NS_OK;
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
void
Navigator::GetAcceptLanguages(nsTArray<nsString>& aLanguages)
{
  // E.g. "de-de, en-us,en".
  const nsAdoptingString& acceptLang =
    Preferences::GetLocalizedString("intl.accept_languages");

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
        const nsSubstring& code = localeTokenizer.nextToken();

        if (code.Length() == 2 && !first) {
          nsAutoString upper(code);
          ToUpperCase(upper);
          lang.Replace(pos, code.Length(), upper);
        }

        pos += code.Length() + 1; // 1 is the separator
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
NS_IMETHODIMP
Navigator::GetLanguage(nsAString& aLanguage)
{
  nsTArray<nsString> languages;
  GetLanguages(languages);
  if (languages.Length() >= 1) {
    aLanguage.Assign(languages[0]);
  } else {
    aLanguage.Truncate();
  }

    return NS_OK;
}

void
Navigator::GetLanguages(nsTArray<nsString>& aLanguages)
{
  GetAcceptLanguages(aLanguages);

  // The returned value is cached by the binding code. The window listen to the
  // accept languages change and will clear the cache when needed. It has to
  // take care of dispatching the DOM event already and the invalidation and the
  // event has to be timed correctly.
}

NS_IMETHODIMP
Navigator::GetPlatform(nsAString& aPlatform)
{
  return NS_GetNavigatorPlatform(aPlatform);
}

NS_IMETHODIMP
Navigator::GetOscpu(nsAString& aOSCPU)
{
  if (!nsContentUtils::IsCallerChrome()) {
    const nsAdoptingString& override =
      Preferences::GetString("general.oscpu.override");

    if (override) {
      aOSCPU = override;
      return NS_OK;
    }
  }

  nsresult rv;

  nsCOMPtr<nsIHttpProtocolHandler>
    service(do_GetService(NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "http", &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString oscpu;
  rv = service->GetOscpu(oscpu);
  CopyASCIItoUTF16(oscpu, aOSCPU);

  return rv;
}

NS_IMETHODIMP
Navigator::GetVendor(nsAString& aVendor)
{
  aVendor.Truncate();
  return NS_OK;
}

NS_IMETHODIMP
Navigator::GetVendorSub(nsAString& aVendorSub)
{
  aVendorSub.Truncate();
  return NS_OK;
}

NS_IMETHODIMP
Navigator::GetProduct(nsAString& aProduct)
{
  aProduct.AssignLiteral("Gecko");
  return NS_OK;
}

NS_IMETHODIMP
Navigator::GetProductSub(nsAString& aProductSub)
{
  // Legacy build ID hardcoded for backward compatibility (bug 776376)
  aProductSub.AssignLiteral("20100101");
  return NS_OK;
}

nsMimeTypeArray*
Navigator::GetMimeTypes(ErrorResult& aRv)
{
  if (!mMimeTypes) {
    if (!mWindow) {
      aRv.Throw(NS_ERROR_UNEXPECTED);
      return nullptr;
    }
    mMimeTypes = new nsMimeTypeArray(mWindow);
  }

  return mMimeTypes;
}

nsPluginArray*
Navigator::GetPlugins(ErrorResult& aRv)
{
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

// Values for the network.cookie.cookieBehavior pref are documented in
// nsCookieService.cpp.
#define COOKIE_BEHAVIOR_REJECT 2

bool
Navigator::CookieEnabled()
{
  bool cookieEnabled =
    (Preferences::GetInt("network.cookie.cookieBehavior",
                         COOKIE_BEHAVIOR_REJECT) != COOKIE_BEHAVIOR_REJECT);

  // Check whether an exception overrides the global cookie behavior
  // Note that the code for getting the URI here matches that in
  // nsHTMLDocument::SetCookie.
  if (!mWindow || !mWindow->GetDocShell()) {
    return cookieEnabled;
  }

  nsCOMPtr<nsIDocument> doc = mWindow->GetExtantDoc();
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

  nsCOMPtr<nsICookiePermission> permMgr =
    do_GetService(NS_COOKIEPERMISSION_CONTRACTID);
  NS_ENSURE_TRUE(permMgr, cookieEnabled);

  // Pass null for the channel, just like the cookie service does.
  nsCookieAccess access;
  nsresult rv = permMgr->CanAccess(codebaseURI, nullptr, &access);
  NS_ENSURE_SUCCESS(rv, cookieEnabled);

  if (access != nsICookiePermission::ACCESS_DEFAULT) {
    cookieEnabled = access != nsICookiePermission::ACCESS_DENY;
  }

  return cookieEnabled;
}

bool
Navigator::OnLine()
{
  return !NS_IsOffline();
}

NS_IMETHODIMP
Navigator::GetBuildID(nsAString& aBuildID)
{
  if (!nsContentUtils::IsCallerChrome()) {
    const nsAdoptingString& override =
      Preferences::GetString("general.buildID.override");

    if (override) {
      aBuildID = override;
      return NS_OK;
    }
  }

  nsCOMPtr<nsIXULAppInfo> appInfo =
    do_GetService("@mozilla.org/xre/app-info;1");
  if (!appInfo) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  nsAutoCString buildID;
  nsresult rv = appInfo->GetAppBuildID(buildID);
  if (NS_FAILED(rv)) {
    return rv;
  }

  aBuildID.Truncate();
  AppendASCIItoUTF16(buildID, aBuildID);
  return NS_OK;
}

NS_IMETHODIMP
Navigator::GetDoNotTrack(nsAString &aResult)
{
  if (sDoNotTrackEnabled) {
    if (sDoNotTrackValue == 0) {
      aResult.AssignLiteral("0");
    } else {
      aResult.AssignLiteral("1");
    }
  } else {
    aResult.AssignLiteral("unspecified");
  }

  return NS_OK;
}

bool
Navigator::JavaEnabled(ErrorResult& aRv)
{
  Telemetry::AutoTimer<Telemetry::CHECK_JAVA_ENABLED> telemetryTimer;

  // Return true if we have a handler for the java mime
  nsAdoptingString javaMIME = Preferences::GetString("plugin.java.mime");
  NS_ENSURE_TRUE(!javaMIME.IsEmpty(), false);

  if (!mMimeTypes) {
    if (!mWindow) {
      aRv.Throw(NS_ERROR_UNEXPECTED);
      return false;
    }
    mMimeTypes = new nsMimeTypeArray(mWindow);
  }

  RefreshMIMEArray();

  nsMimeType *mimeType = mMimeTypes->NamedItem(javaMIME);

  return mimeType && mimeType->GetEnabledPlugin();
}

void
Navigator::RefreshMIMEArray()
{
  if (mMimeTypes) {
    mMimeTypes->Refresh();
  }
}

namespace {

class VibrateWindowListener : public nsIDOMEventListener
{
public:
  VibrateWindowListener(nsIDOMWindow* aWindow, nsIDocument* aDocument)
  {
    mWindow = do_GetWeakReference(aWindow);
    mDocument = do_GetWeakReference(aDocument);

    NS_NAMED_LITERAL_STRING(visibilitychange, "visibilitychange");
    aDocument->AddSystemEventListener(visibilitychange,
                                      this, /* listener */
                                      true, /* use capture */
                                      false /* wants untrusted */);
  }

  void RemoveListener();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMEVENTLISTENER

private:
  virtual ~VibrateWindowListener()
  {
  }

  nsWeakPtr mWindow;
  nsWeakPtr mDocument;
};

NS_IMPL_ISUPPORTS(VibrateWindowListener, nsIDOMEventListener)

StaticRefPtr<VibrateWindowListener> gVibrateWindowListener;

NS_IMETHODIMP
VibrateWindowListener::HandleEvent(nsIDOMEvent* aEvent)
{
  nsCOMPtr<nsIDocument> doc =
    do_QueryInterface(aEvent->InternalDOMEvent()->GetTarget());

  if (!doc || doc->Hidden()) {
    // It's important that we call CancelVibrate(), not Vibrate() with an
    // empty list, because Vibrate() will fail if we're no longer focused, but
    // CancelVibrate() will succeed, so long as nobody else has started a new
    // vibration pattern.
    nsCOMPtr<nsIDOMWindow> window = do_QueryReferent(mWindow);
    hal::CancelVibrate(window);
    RemoveListener();
    gVibrateWindowListener = nullptr;
    // Careful: The line above might have deleted |this|!
  }

  return NS_OK;
}

void
VibrateWindowListener::RemoveListener()
{
  nsCOMPtr<EventTarget> target = do_QueryReferent(mDocument);
  if (!target) {
    return;
  }
  NS_NAMED_LITERAL_STRING(visibilitychange, "visibilitychange");
  target->RemoveSystemEventListener(visibilitychange, this,
                                    true /* use capture */);
}

} // anonymous namespace

void
Navigator::AddIdleObserver(MozIdleObserver& aIdleObserver, ErrorResult& aRv)
{
  if (!mWindow) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return;
  }
  CallbackObjectHolder<MozIdleObserver, nsIIdleObserver> holder(&aIdleObserver);
  nsCOMPtr<nsIIdleObserver> obs = holder.ToXPCOMCallback();
  if (NS_FAILED(mWindow->RegisterIdleObserver(obs))) {
    NS_WARNING("Failed to add idle observer.");
  }
}

void
Navigator::RemoveIdleObserver(MozIdleObserver& aIdleObserver, ErrorResult& aRv)
{
  if (!mWindow) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return;
  }
  CallbackObjectHolder<MozIdleObserver, nsIIdleObserver> holder(&aIdleObserver);
  nsCOMPtr<nsIIdleObserver> obs = holder.ToXPCOMCallback();
  if (NS_FAILED(mWindow->UnregisterIdleObserver(obs))) {
    NS_WARNING("Failed to remove idle observer.");
  }
}

bool
Navigator::Vibrate(uint32_t aDuration)
{
  nsAutoTArray<uint32_t, 1> pattern;
  pattern.AppendElement(aDuration);
  return Vibrate(pattern);
}

bool
Navigator::Vibrate(const nsTArray<uint32_t>& aPattern)
{
  if (!mWindow) {
    return false;
  }

  nsCOMPtr<nsIDocument> doc = mWindow->GetExtantDoc();
  if (!doc) {
    return false;
  }

  if (doc->Hidden()) {
    // Hidden documents cannot start or stop a vibration.
    return false;
  }

  nsTArray<uint32_t> pattern(aPattern);

  if (pattern.Length() > sMaxVibrateListLen) {
    pattern.SetLength(sMaxVibrateListLen);
  }

  for (size_t i = 0; i < pattern.Length(); ++i) {
    if (pattern[i] > sMaxVibrateMS) {
      pattern[i] = sMaxVibrateMS;
    }
  }

  // The spec says we check sVibratorEnabled after we've done the sanity
  // checking on the pattern.
  if (!sVibratorEnabled) {
    return true;
  }

  // Add a listener to cancel the vibration if the document becomes hidden,
  // and remove the old visibility listener, if there was one.

  if (!gVibrateWindowListener) {
    // If gVibrateWindowListener is null, this is the first time we've vibrated,
    // and we need to register a listener to clear gVibrateWindowListener on
    // shutdown.
    ClearOnShutdown(&gVibrateWindowListener);
  }
  else {
    gVibrateWindowListener->RemoveListener();
  }
  gVibrateWindowListener = new VibrateWindowListener(mWindow, doc);

  hal::Vibrate(pattern, mWindow);
  return true;
}

//*****************************************************************************
//  Pointer Events interface
//*****************************************************************************

uint32_t
Navigator::MaxTouchPoints()
{
  nsCOMPtr<nsIWidget> widget = widget::WidgetUtils::DOMWindowToWidget(mWindow);

  NS_ENSURE_TRUE(widget, 0);
  return widget->GetMaxTouchPoints();
}

//*****************************************************************************
//    Navigator::nsIDOMClientInformation
//*****************************************************************************

void
Navigator::RegisterContentHandler(const nsAString& aMIMEType,
                                  const nsAString& aURI,
                                  const nsAString& aTitle,
                                  ErrorResult& aRv)
{
  if (!mWindow || !mWindow->GetOuterWindow() || !mWindow->GetDocShell()) {
    return;
  }

  nsCOMPtr<nsIWebContentHandlerRegistrar> registrar =
    do_GetService(NS_WEBCONTENTHANDLERREGISTRAR_CONTRACTID);
  if (!registrar) {
    return;
  }

  aRv = registrar->RegisterContentHandler(aMIMEType, aURI, aTitle,
                                          mWindow->GetOuterWindow());
}

void
Navigator::RegisterProtocolHandler(const nsAString& aProtocol,
                                   const nsAString& aURI,
                                   const nsAString& aTitle,
                                   ErrorResult& aRv)
{
  if (!mWindow || !mWindow->GetOuterWindow() || !mWindow->GetDocShell()) {
    return;
  }

  nsCOMPtr<nsIWebContentHandlerRegistrar> registrar =
    do_GetService(NS_WEBCONTENTHANDLERREGISTRAR_CONTRACTID);
  if (!registrar) {
    return;
  }

  aRv = registrar->RegisterProtocolHandler(aProtocol, aURI, aTitle,
                                           mWindow->GetOuterWindow());
}

bool
Navigator::MozIsLocallyAvailable(const nsAString &aURI,
                                 bool aWhenOffline,
                                 ErrorResult& aRv)
{
  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), aURI);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return false;
  }

  // This method of checking the cache will only work for http/https urls.
  bool match;
  rv = uri->SchemeIs("http", &match);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return false;
  }

  if (!match) {
    rv = uri->SchemeIs("https", &match);
    if (NS_FAILED(rv)) {
      aRv.Throw(rv);
      return false;
    }
    if (!match) {
      aRv.Throw(NS_ERROR_DOM_BAD_URI);
      return false;
    }
  }

  // Same origin check.
  JSContext *cx = nsContentUtils::GetCurrentJSContext();
  if (!cx) {
    aRv.Throw(NS_ERROR_FAILURE);
    return false;
  }

  rv = nsContentUtils::GetSecurityManager()->CheckSameOrigin(cx, uri);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return false;
  }

  // These load flags cause an error to be thrown if there is no
  // valid cache entry, and skip the load if there is.
  // If the cache is busy, assume that it is not yet available rather
  // than waiting for it to become available.
  uint32_t loadFlags = nsIChannel::INHIBIT_CACHING |
                       nsICachingChannel::LOAD_NO_NETWORK_IO |
                       nsICachingChannel::LOAD_ONLY_IF_MODIFIED |
                       nsICachingChannel::LOAD_BYPASS_LOCAL_CACHE_IF_BUSY;

  if (aWhenOffline) {
    loadFlags |= nsICachingChannel::LOAD_CHECK_OFFLINE_CACHE |
                 nsICachingChannel::LOAD_ONLY_FROM_CACHE |
                 nsIRequest::LOAD_FROM_CACHE;
  }

  if (!mWindow) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return false;
  }

  nsCOMPtr<nsILoadGroup> loadGroup;
  nsCOMPtr<nsIDocument> doc = mWindow->GetDoc();
  if (doc) {
    loadGroup = doc->GetDocumentLoadGroup();
  }

  nsCOMPtr<nsIChannel> channel;
  rv = NS_NewChannel(getter_AddRefs(channel), uri,
                     nullptr, loadGroup, nullptr, loadFlags);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return false;
  }

  nsCOMPtr<nsIInputStream> stream;
  rv = channel->Open(getter_AddRefs(stream));
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return false;
  }

  stream->Close();

  nsresult status;
  rv = channel->GetStatus(&status);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return false;
  }

  if (NS_FAILED(status)) {
    return false;
  }

  nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(channel);
  bool isAvailable;
  rv = httpChannel->GetRequestSucceeded(&isAvailable);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return false;
  }
  return isAvailable;
}

nsDOMDeviceStorage*
Navigator::GetDeviceStorage(const nsAString& aType, ErrorResult& aRv)
{
  if (!mWindow || !mWindow->GetOuterWindow() || !mWindow->GetDocShell()) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsRefPtr<nsDOMDeviceStorage> storage;
  nsDOMDeviceStorage::CreateDeviceStorageFor(mWindow, aType,
                                             getter_AddRefs(storage));

  if (!storage) {
    return nullptr;
  }

  mDeviceStorageStores.AppendElement(storage);
  return storage;
}

void
Navigator::GetDeviceStorages(const nsAString& aType,
                             nsTArray<nsRefPtr<nsDOMDeviceStorage> >& aStores,
                             ErrorResult& aRv)
{
  if (!mWindow || !mWindow->GetOuterWindow() || !mWindow->GetDocShell()) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  nsDOMDeviceStorage::CreateDeviceStoragesFor(mWindow, aType, aStores);

  mDeviceStorageStores.AppendElements(aStores);
}

Geolocation*
Navigator::GetGeolocation(ErrorResult& aRv)
{
  if (mGeolocation) {
    return mGeolocation;
  }

  if (!mWindow || !mWindow->GetOuterWindow() || !mWindow->GetDocShell()) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  mGeolocation = new Geolocation();
  if (NS_FAILED(mGeolocation->Init(mWindow->GetOuterWindow()))) {
    mGeolocation = nullptr;
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  return mGeolocation;
}

class BeaconStreamListener MOZ_FINAL : public nsIStreamListener
{
    ~BeaconStreamListener() {}

  public:
    BeaconStreamListener() {}

    NS_DECL_ISUPPORTS
    NS_DECL_NSISTREAMLISTENER
    NS_DECL_NSIREQUESTOBSERVER
};

NS_IMPL_ISUPPORTS(BeaconStreamListener,
                  nsIStreamListener,
                  nsIRequestObserver)


NS_IMETHODIMP
BeaconStreamListener::OnStartRequest(nsIRequest *aRequest,
                                     nsISupports *aContext)
{
  aRequest->Cancel(NS_ERROR_NET_INTERRUPT);
  return NS_BINDING_ABORTED;
}

NS_IMETHODIMP
BeaconStreamListener::OnStopRequest(nsIRequest *aRequest,
                                    nsISupports *aContext,
                                    nsresult aStatus)
{
  return NS_OK;
}

NS_IMETHODIMP
BeaconStreamListener::OnDataAvailable(nsIRequest *aRequest,
                                      nsISupports *ctxt,
                                      nsIInputStream *inStr,
                                      uint64_t sourceOffset,
                                      uint32_t count)
{
  MOZ_ASSERT(false);
  return NS_OK;
}

bool
Navigator::SendBeacon(const nsAString& aUrl,
                      const Nullable<ArrayBufferViewOrBlobOrStringOrFormData>& aData,
                      ErrorResult& aRv)
{
  if (!mWindow) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return false;
  }

  nsCOMPtr<nsIDocument> doc = mWindow->GetDoc();
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
                  getter_AddRefs(uri),
                  aUrl,
                  doc,
                  doc->GetDocBaseURI());
  if (NS_FAILED(rv)) {
    aRv.Throw(NS_ERROR_DOM_URL_MISMATCH_ERR);
    return false;
  }

  // Check whether this is a sane URI to load
  // Explicitly disallow things like chrome:, javascript:, and data: URIs
  nsCOMPtr<nsIPrincipal> principal = doc->NodePrincipal();
  nsCOMPtr<nsIScriptSecurityManager> secMan = nsContentUtils::GetSecurityManager();
  uint32_t flags = nsIScriptSecurityManager::DISALLOW_INHERIT_PRINCIPAL
                   & nsIScriptSecurityManager::DISALLOW_SCRIPT;
  rv = secMan->CheckLoadURIWithPrincipal(principal,
                                         uri,
                                         flags);
  if (NS_FAILED(rv)) {
    // Bad URI
    aRv.Throw(rv);
    return false;
  }

  // Check whether the CSP allows us to load
  int16_t shouldLoad = nsIContentPolicy::ACCEPT;
  rv = NS_CheckContentLoadPolicy(nsIContentPolicy::TYPE_BEACON,
                                 uri,
                                 principal,
                                 doc,
                                 EmptyCString(), //mime guess
                                 nullptr,         //extra
                                 &shouldLoad,
                                 nsContentUtils::GetContentPolicy(),
                                 nsContentUtils::GetSecurityManager());
  if (NS_FAILED(rv) || NS_CP_REJECTED(shouldLoad)) {
    // Disallowed by content policy
    aRv.Throw(NS_ERROR_CONTENT_BLOCKED);
    return false;
  }

  nsCOMPtr<nsIChannel> channel;
  nsCOMPtr<nsIChannelPolicy> channelPolicy;
  nsCOMPtr<nsIContentSecurityPolicy> csp;
  rv = principal->GetCsp(getter_AddRefs(csp));
  if (NS_FAILED(rv)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return false;
  }

  if (csp) {
    channelPolicy = do_CreateInstance(NSCHANNELPOLICY_CONTRACTID);
    channelPolicy->SetContentSecurityPolicy(csp);
    channelPolicy->SetLoadType(nsIContentPolicy::TYPE_BEACON);
  }
  rv = NS_NewChannel(getter_AddRefs(channel),
                     uri,
                     nullptr,
                     nullptr,
                     nullptr,
                     nsIRequest::LOAD_NORMAL,
                     channelPolicy);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return false;
  }

  nsCOMPtr<nsIPrivateBrowsingChannel> pbChannel = do_QueryInterface(channel);
  if (pbChannel) {
    nsIDocShell* docShell = mWindow->GetDocShell();
    nsCOMPtr<nsILoadContext> loadContext = do_QueryInterface(docShell);
    if (loadContext) {
      rv = pbChannel->SetPrivate(loadContext->UsePrivateBrowsing());
      if (NS_FAILED(rv)) {
        NS_WARNING("Setting the privacy status on the beacon channel failed");
      }
    }
  }

  nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(channel);
  if (!httpChannel) {
    // Beacon spec only supports HTTP requests at this time
    aRv.Throw(NS_ERROR_DOM_BAD_URI);
    return false;
  }
  httpChannel->SetReferrer(documentURI);

  // Anything that will need to refer to the window during the request
  // will need to be done now.  For example, detection of whether any
  // cookies set by this request are foreign.  Note that ThirdPartyUtil
  // (nsIThirdPartyUtil.isThirdPartyChannel) does a secondary check between
  // the channel URI and the cookie URI even when forceAllowThirdPartyCookie
  // is set, so this is safe with regard to redirects.
  nsCOMPtr<nsIHttpChannelInternal> httpChannelInternal(do_QueryInterface(channel));
  nsCOMPtr<mozIThirdPartyUtil> thirdPartyUtil = do_GetService(THIRDPARTYUTIL_CONTRACTID);
  if (!httpChannelInternal) {
    aRv.Throw(NS_ERROR_DOM_BAD_URI);
    return false;
  }
  bool isForeign = true;
  thirdPartyUtil->IsThirdPartyWindow(mWindow, uri, &isForeign);
  httpChannelInternal->SetForceAllowThirdPartyCookie(!isForeign);

  nsCString mimeType;
  if (!aData.IsNull()) {
    nsCOMPtr<nsIInputStream> in;

    if (aData.Value().IsString()) {
      nsCString stringData = NS_ConvertUTF16toUTF8(aData.Value().GetAsString());
      nsCOMPtr<nsIStringInputStream> strStream = do_CreateInstance(NS_STRINGINPUTSTREAM_CONTRACTID, &rv);
      if (NS_FAILED(rv)) {
        aRv.Throw(NS_ERROR_FAILURE);
        return false;
      }
      rv = strStream->SetData(stringData.BeginReading(), stringData.Length());
      if (NS_FAILED(rv)) {
        aRv.Throw(NS_ERROR_FAILURE);
        return false;
      }
      mimeType.AssignLiteral("text/plain;charset=UTF-8");
      in = strStream;

    } else if (aData.Value().IsArrayBufferView()) {

      nsCOMPtr<nsIStringInputStream> strStream = do_CreateInstance(NS_STRINGINPUTSTREAM_CONTRACTID, &rv);
      if (NS_FAILED(rv)) {
        aRv.Throw(NS_ERROR_FAILURE);
        return false;
      }

      const ArrayBufferView& view = aData.Value().GetAsArrayBufferView();
      view.ComputeLengthAndData();
      rv = strStream->SetData(reinterpret_cast<char*>(view.Data()),
                              view.Length());

      if (NS_FAILED(rv)) {
        aRv.Throw(NS_ERROR_FAILURE);
        return false;
      }
      mimeType.AssignLiteral("application/octet-stream");
      in = strStream;

    } else if (aData.Value().IsBlob()) {
      nsCOMPtr<nsIDOMBlob> blob = aData.Value().GetAsBlob();
      rv = blob->GetInternalStream(getter_AddRefs(in));
      if (NS_FAILED(rv)) {
        aRv.Throw(NS_ERROR_FAILURE);
        return false;
      }
      nsAutoString type;
      rv = blob->GetType(type);
      if (NS_FAILED(rv)) {
        aRv.Throw(NS_ERROR_FAILURE);
        return false;
      }
      mimeType = NS_ConvertUTF16toUTF8(type);

    } else if (aData.Value().IsFormData()) {
      nsFormData& form = aData.Value().GetAsFormData();
      uint64_t len;
      nsAutoCString charset;
      form.GetSendInfo(getter_AddRefs(in),
                       &len,
                       mimeType,
                       charset);
    } else {
      MOZ_ASSERT(false, "switch statements not in sync");
      aRv.Throw(NS_ERROR_FAILURE);
      return false;
    }

    nsCOMPtr<nsIUploadChannel2> uploadChannel = do_QueryInterface(channel);
    if (!uploadChannel) {
      aRv.Throw(NS_ERROR_FAILURE);
      return false;
    }
    uploadChannel->ExplicitSetUploadStream(in, mimeType, -1,
                                           NS_LITERAL_CSTRING("POST"),
                                           false);
  } else {
    httpChannel->SetRequestMethod(NS_LITERAL_CSTRING("POST"));
  }

  nsCOMPtr<nsISupportsPriority> p = do_QueryInterface(channel);
  if (p) {
    p->SetPriority(nsISupportsPriority::PRIORITY_LOWEST);
  }

  nsRefPtr<nsCORSListenerProxy> cors = new nsCORSListenerProxy(new BeaconStreamListener(),
                                                               principal,
                                                               true);

  // Start a preflight if cross-origin and content type is not whitelisted
  rv = secMan->CheckSameOriginURI(documentURI, uri, false);
  bool crossOrigin = NS_FAILED(rv);
  nsAutoCString contentType, parsedCharset;
  rv = NS_ParseContentType(mimeType, contentType, parsedCharset);
  if (crossOrigin &&
      contentType.Length() > 0 &&
      !contentType.Equals(APPLICATION_WWW_FORM_URLENCODED) &&
      !contentType.Equals(MULTIPART_FORM_DATA) &&
      !contentType.Equals(TEXT_PLAIN)) {
    nsCOMPtr<nsIChannel> preflightChannel;
    nsTArray<nsCString> unsafeHeaders;
    unsafeHeaders.AppendElement(NS_LITERAL_CSTRING("Content-Type"));
    rv = NS_StartCORSPreflight(channel,
                               cors,
                               principal,
                               true,
                               unsafeHeaders,
                               getter_AddRefs(preflightChannel));
  } else {
    rv = channel->AsyncOpen(cors, nullptr);
  }
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return false;
  }
  return true;
}

#ifdef MOZ_MEDIA_NAVIGATOR
void
Navigator::MozGetUserMedia(const MediaStreamConstraints& aConstraints,
                           NavigatorUserMediaSuccessCallback& aOnSuccess,
                           NavigatorUserMediaErrorCallback& aOnError,
                           ErrorResult& aRv)
{
  CallbackObjectHolder<NavigatorUserMediaSuccessCallback,
                       nsIDOMGetUserMediaSuccessCallback> holder1(&aOnSuccess);
  nsCOMPtr<nsIDOMGetUserMediaSuccessCallback> onsuccess =
    holder1.ToXPCOMCallback();

  CallbackObjectHolder<NavigatorUserMediaErrorCallback,
                       nsIDOMGetUserMediaErrorCallback> holder2(&aOnError);
  nsCOMPtr<nsIDOMGetUserMediaErrorCallback> onerror = holder2.ToXPCOMCallback();

  if (!mWindow || !mWindow->GetOuterWindow() ||
      mWindow->GetOuterWindow()->GetCurrentInnerWindow() != mWindow) {
    aRv.Throw(NS_ERROR_NOT_AVAILABLE);
    return;
  }

  bool privileged = nsContentUtils::IsChromeDoc(mWindow->GetExtantDoc());

  MediaManager* manager = MediaManager::Get();
  aRv = manager->GetUserMedia(privileged, mWindow, aConstraints,
                              onsuccess, onerror);
}

void
Navigator::MozGetUserMediaDevices(const MediaStreamConstraints& aConstraints,
                                  MozGetUserMediaDevicesSuccessCallback& aOnSuccess,
                                  NavigatorUserMediaErrorCallback& aOnError,
                                  uint64_t aInnerWindowID,
                                  ErrorResult& aRv)
{
  CallbackObjectHolder<MozGetUserMediaDevicesSuccessCallback,
                       nsIGetUserMediaDevicesSuccessCallback> holder1(&aOnSuccess);
  nsCOMPtr<nsIGetUserMediaDevicesSuccessCallback> onsuccess =
    holder1.ToXPCOMCallback();

  CallbackObjectHolder<NavigatorUserMediaErrorCallback,
                       nsIDOMGetUserMediaErrorCallback> holder2(&aOnError);
  nsCOMPtr<nsIDOMGetUserMediaErrorCallback> onerror = holder2.ToXPCOMCallback();

  if (!mWindow || !mWindow->GetOuterWindow() ||
      mWindow->GetOuterWindow()->GetCurrentInnerWindow() != mWindow) {
    aRv.Throw(NS_ERROR_NOT_AVAILABLE);
    return;
  }

  MediaManager* manager = MediaManager::Get();
  aRv = manager->GetUserMediaDevices(mWindow, aConstraints, onsuccess, onerror,
                                     aInnerWindowID);
}
#endif

DesktopNotificationCenter*
Navigator::GetMozNotification(ErrorResult& aRv)
{
  if (mNotification) {
    return mNotification;
  }

  if (!mWindow || !mWindow->GetDocShell()) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  mNotification = new DesktopNotificationCenter(mWindow);
  return mNotification;
}

#ifdef MOZ_B2G_FM

using mozilla::dom::FMRadio;

FMRadio*
Navigator::GetMozFMRadio(ErrorResult& aRv)
{
  if (!mFMRadio) {
    if (!mWindow) {
      aRv.Throw(NS_ERROR_UNEXPECTED);
      return nullptr;
    }

    NS_ENSURE_TRUE(mWindow->GetDocShell(), nullptr);

    mFMRadio = new FMRadio();
    mFMRadio->Init(mWindow);
  }

  return mFMRadio;
}

#endif  // MOZ_B2G_FM

//*****************************************************************************
//    Navigator::nsINavigatorBattery
//*****************************************************************************

battery::BatteryManager*
Navigator::GetBattery(ErrorResult& aRv)
{
  if (!mBatteryManager) {
    if (!mWindow) {
      aRv.Throw(NS_ERROR_UNEXPECTED);
      return nullptr;
    }
    NS_ENSURE_TRUE(mWindow->GetDocShell(), nullptr);

    mBatteryManager = new battery::BatteryManager(mWindow);
    mBatteryManager->Init();
  }

  return mBatteryManager;
}

/* static */ already_AddRefed<Promise>
Navigator::GetDataStores(nsPIDOMWindow* aWindow,
                         const nsAString& aName,
                         const nsAString& aOwner,
                         ErrorResult& aRv)
{
  if (!aWindow || !aWindow->GetDocShell()) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  nsRefPtr<DataStoreService> service = DataStoreService::GetOrCreate();
  if (!service) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsCOMPtr<nsISupports> promise;
  aRv = service->GetDataStores(aWindow, aName, aOwner, getter_AddRefs(promise));

  nsRefPtr<Promise> p = static_cast<Promise*>(promise.get());
  return p.forget();
}

already_AddRefed<Promise>
Navigator::GetDataStores(const nsAString& aName,
                         const nsAString& aOwner,
                         ErrorResult& aRv)
{
  return GetDataStores(mWindow, aName, aOwner, aRv);
}

already_AddRefed<Promise>
Navigator::GetFeature(const nsAString& aName, ErrorResult& aRv)
{
  nsCOMPtr<nsIGlobalObject> go = do_QueryInterface(mWindow);
  nsRefPtr<Promise> p = Promise::Create(go, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

#if defined(XP_LINUX)
  if (aName.EqualsLiteral("hardware.memory")) {
    // with seccomp enabled, fopen() should be in a non-sandboxed process
    if (XRE_GetProcessType() == GeckoProcessType_Default) {
      uint32_t memLevel = mozilla::hal::GetTotalSystemMemoryLevel();
      if (memLevel == 0) {
        p->MaybeReject(NS_ERROR_NOT_AVAILABLE);
        return p.forget();
      }
      p->MaybeResolve((int)memLevel);
    } else {
      mozilla::dom::ContentChild* cc =
        mozilla::dom::ContentChild::GetSingleton();
      nsRefPtr<Promise> ipcRef(p);
      cc->SendGetSystemMemory(reinterpret_cast<uint64_t>(ipcRef.forget().take()));
    }
    return p.forget();
  } // hardware.memory
#endif

  // Hardcoded manifest features. Some are still b2g specific.
  const char manifestFeatures[][64] = {
    "manifest.origin"
  , "manifest.redirects"
#ifdef MOZ_B2G
  , "manifest.chrome.navigation"
  , "manifest.precompile"
#endif
  };

  nsAutoCString feature = NS_ConvertUTF16toUTF8(aName);
  for (uint32_t i = 0; i < MOZ_ARRAY_LENGTH(manifestFeatures); i++) {
    if (feature.Equals(manifestFeatures[i])) {
      p->MaybeResolve(true);
      return p.forget();
    }
  }

  p->MaybeResolve(JS::UndefinedHandleValue);
  return p.forget();
}

already_AddRefed<Promise>
Navigator::HasFeature(const nsAString& aName, ErrorResult& aRv)
{
  nsCOMPtr<nsIGlobalObject> go = do_QueryInterface(mWindow);
  nsRefPtr<Promise> p = Promise::Create(go, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  NS_NAMED_LITERAL_STRING(apiWindowPrefix, "api.window.");
  if (StringBeginsWith(aName, apiWindowPrefix)) {
    const nsAString& featureName = Substring(aName, apiWindowPrefix.Length());

    // Temporary hardcoded entry points due to technical constraints
    if (featureName.EqualsLiteral("Navigator.mozTCPSocket")) {
      p->MaybeResolve(Preferences::GetBool("dom.mozTCPSocket.enabled"));
      return p.forget();
    }

    if (featureName.EqualsLiteral("Navigator.mozMobileConnections") ||
        featureName.EqualsLiteral("MozMobileNetworkInfo")) {
      p->MaybeResolve(Preferences::GetBool("dom.mobileconnection.enabled"));
      return p.forget();
    }

    if (featureName.EqualsLiteral("Navigator.mozInputMethod")) {
      p->MaybeResolve(Preferences::GetBool("dom.mozInputMethod.enabled"));
      return p.forget();
    }

    if (featureName.EqualsLiteral("Navigator.mozContacts")) {
      p->MaybeResolve(true);
      return p.forget();
    }

    if (featureName.EqualsLiteral("Navigator.getDeviceStorage")) {
      p->MaybeResolve(Preferences::GetBool("device.storage.enabled"));
      return p.forget();
    }

    if (featureName.EqualsLiteral("Navigator.mozNetworkStats")) {
      p->MaybeResolve(Preferences::GetBool("dom.mozNetworkStats.enabled"));
      return p.forget();
    }

    if (featureName.EqualsLiteral("Navigator.push")) {
      p->MaybeResolve(Preferences::GetBool("services.push.enabled"));
      return p.forget();
    }

    if (featureName.EqualsLiteral("Navigator.mozAlarms")) {
      p->MaybeResolve(Preferences::GetBool("dom.mozAlarms.enabled"));
      return p.forget();
    }

    if (featureName.EqualsLiteral("Navigator.mozCameras")) {
      p->MaybeResolve(true);
      return p.forget();
    }

#ifdef MOZ_B2G
    if (featureName.EqualsLiteral("Navigator.getMobileIdAssertion")) {
      p->MaybeResolve(true);
      return p.forget();
    }
#endif 

    if (featureName.EqualsLiteral("XMLHttpRequest.mozSystem")) {
      p->MaybeResolve(true);
      return p.forget();
    }

    if (IsFeatureDetectible(featureName)) {
      p->MaybeResolve(true);
    } else {
      p->MaybeResolve(JS::UndefinedHandleValue);
    }
    return p.forget();
  }

  // resolve with <undefined> because the feature name is not supported
  p->MaybeResolve(JS::UndefinedHandleValue);

  return p.forget();
}

PowerManager*
Navigator::GetMozPower(ErrorResult& aRv)
{
  if (!mPowerManager) {
    if (!mWindow) {
      aRv.Throw(NS_ERROR_UNEXPECTED);
      return nullptr;
    }
    mPowerManager = PowerManager::CreateInstance(mWindow);
    if (!mPowerManager) {
      // We failed to get the power manager service?
      aRv.Throw(NS_ERROR_UNEXPECTED);
    }
  }

  return mPowerManager;
}

already_AddRefed<WakeLock>
Navigator::RequestWakeLock(const nsAString &aTopic, ErrorResult& aRv)
{
  if (!mWindow) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  nsRefPtr<power::PowerManagerService> pmService =
    power::PowerManagerService::GetInstance();
  // Maybe it went away for some reason... Or maybe we're just called
  // from our XPCOM method.
  if (!pmService) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  return pmService->NewWakeLock(aTopic, mWindow, aRv);
}

MobileMessageManager*
Navigator::GetMozMobileMessage()
{
  if (!mMobileMessageManager) {
    // Check that our window has not gone away
    NS_ENSURE_TRUE(mWindow, nullptr);
    NS_ENSURE_TRUE(mWindow->GetDocShell(), nullptr);

    mMobileMessageManager = new MobileMessageManager(mWindow);
    mMobileMessageManager->Init();
  }

  return mMobileMessageManager;
}

Telephony*
Navigator::GetMozTelephony(ErrorResult& aRv)
{
  if (!mTelephony) {
    if (!mWindow) {
      aRv.Throw(NS_ERROR_UNEXPECTED);
      return nullptr;
    }
    mTelephony = Telephony::Create(mWindow, aRv);
  }

  return mTelephony;
}

#ifdef MOZ_B2G
already_AddRefed<Promise>
Navigator::GetMobileIdAssertion(const MobileIdOptions& aOptions,
                                ErrorResult& aRv)
{
  if (!mWindow || !mWindow->GetDocShell()) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  nsCOMPtr<nsIMobileIdentityService> service =
    do_GetService("@mozilla.org/mobileidentity-service;1");
  if (!service) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  JSContext *cx = nsContentUtils::GetCurrentJSContext();
  if (!cx) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  JS::Rooted<JS::Value> optionsValue(cx);
  if (!ToJSValue(cx, aOptions, &optionsValue)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsCOMPtr<nsISupports> promise;
  aRv = service->GetMobileIdAssertion(mWindow,
                                      optionsValue,
                                      getter_AddRefs(promise));

  nsRefPtr<Promise> p = static_cast<Promise*>(promise.get());
  return p.forget();
}
#endif // MOZ_B2G

#ifdef MOZ_B2G_RIL

MobileConnectionArray*
Navigator::GetMozMobileConnections(ErrorResult& aRv)
{
  if (!mMobileConnections) {
    if (!mWindow) {
      aRv.Throw(NS_ERROR_UNEXPECTED);
      return nullptr;
    }
    mMobileConnections = new MobileConnectionArray(mWindow);
  }

  return mMobileConnections;
}

CellBroadcast*
Navigator::GetMozCellBroadcast(ErrorResult& aRv)
{
  if (!mCellBroadcast) {
    if (!mWindow) {
      aRv.Throw(NS_ERROR_UNEXPECTED);
      return nullptr;
    }
    mCellBroadcast = CellBroadcast::Create(mWindow, aRv);
  }

  return mCellBroadcast;
}

Voicemail*
Navigator::GetMozVoicemail(ErrorResult& aRv)
{
  if (!mVoicemail) {
    if (!mWindow) {
      aRv.Throw(NS_ERROR_UNEXPECTED);
      return nullptr;
    }

    aRv = NS_NewVoicemail(mWindow, getter_AddRefs(mVoicemail));
    if (aRv.Failed()) {
      return nullptr;
    }
  }

  return mVoicemail;
}

IccManager*
Navigator::GetMozIccManager(ErrorResult& aRv)
{
  if (!mIccManager) {
    if (!mWindow) {
      aRv.Throw(NS_ERROR_UNEXPECTED);
      return nullptr;
    }
    NS_ENSURE_TRUE(mWindow->GetDocShell(), nullptr);

    mIccManager = new IccManager(mWindow);
  }

  return mIccManager;
}
#endif // MOZ_B2G_RIL

#ifdef MOZ_GAMEPAD
void
Navigator::GetGamepads(nsTArray<nsRefPtr<Gamepad> >& aGamepads,
                       ErrorResult& aRv)
{
  if (!mWindow) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return;
  }
  NS_ENSURE_TRUE_VOID(mWindow->GetDocShell());
  nsGlobalWindow* win = static_cast<nsGlobalWindow*>(mWindow.get());
  win->SetHasGamepadEventListener(true);
  win->GetGamepads(aGamepads);
}
#endif

//*****************************************************************************
//    Navigator::nsIMozNavigatorNetwork
//*****************************************************************************

NS_IMETHODIMP
Navigator::GetProperties(nsINetworkProperties** aProperties)
{
  ErrorResult rv;
  NS_IF_ADDREF(*aProperties = GetConnection(rv));
  return NS_OK;
}

network::Connection*
Navigator::GetConnection(ErrorResult& aRv)
{
  if (!mConnection) {
    if (!mWindow) {
      aRv.Throw(NS_ERROR_UNEXPECTED);
      return nullptr;
    }
    mConnection = new network::Connection();
    mConnection->Init(mWindow);
  }

  return mConnection;
}

#ifdef MOZ_B2G_BT
bluetooth::BluetoothManager*
Navigator::GetMozBluetooth(ErrorResult& aRv)
{
  if (!mBluetooth) {
    if (!mWindow) {
      aRv.Throw(NS_ERROR_UNEXPECTED);
      return nullptr;
    }
    mBluetooth = bluetooth::BluetoothManager::Create(mWindow);
  }

  return mBluetooth;
}
#endif //MOZ_B2G_BT

nsresult
Navigator::EnsureMessagesManager()
{
  if (mMessagesManager) {
    return NS_OK;
  }

  NS_ENSURE_STATE(mWindow);

  nsresult rv;
  nsCOMPtr<nsIDOMNavigatorSystemMessages> messageManager =
    do_CreateInstance("@mozilla.org/system-message-manager;1", &rv);

  nsCOMPtr<nsIDOMGlobalPropertyInitializer> gpi =
    do_QueryInterface(messageManager);
  NS_ENSURE_TRUE(gpi, NS_ERROR_FAILURE);

  // We don't do anything with the return value.
  AutoJSContext cx;
  JS::Rooted<JS::Value> prop_val(cx);
  rv = gpi->Init(mWindow, &prop_val);
  NS_ENSURE_SUCCESS(rv, rv);

  mMessagesManager = messageManager.forget();

  return NS_OK;
}

bool
Navigator::MozHasPendingMessage(const nsAString& aType, ErrorResult& aRv)
{
  // The WebIDL binding is responsible for the pref check here.
  nsresult rv = EnsureMessagesManager();
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return false;
  }

  bool result = false;
  rv = mMessagesManager->MozHasPendingMessage(aType, &result);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return false;
  }
  return result;
}

void
Navigator::MozSetMessageHandler(const nsAString& aType,
                                systemMessageCallback* aCallback,
                                ErrorResult& aRv)
{
  // The WebIDL binding is responsible for the pref check here.
  nsresult rv = EnsureMessagesManager();
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return;
  }

  CallbackObjectHolder<systemMessageCallback, nsIDOMSystemMessageCallback>
    holder(aCallback);
  nsCOMPtr<nsIDOMSystemMessageCallback> callback = holder.ToXPCOMCallback();

  rv = mMessagesManager->MozSetMessageHandler(aType, callback);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
  }
}

#ifdef MOZ_TIME_MANAGER
time::TimeManager*
Navigator::GetMozTime(ErrorResult& aRv)
{
  if (!mWindow) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  if (!mTimeManager) {
    mTimeManager = new time::TimeManager(mWindow);
  }

  return mTimeManager;
}
#endif

nsDOMCameraManager*
Navigator::GetMozCameras(ErrorResult& aRv)
{
  if (!mCameraManager) {
    if (!mWindow ||
        !mWindow->GetOuterWindow() ||
        mWindow->GetOuterWindow()->GetCurrentInnerWindow() != mWindow) {
      aRv.Throw(NS_ERROR_NOT_AVAILABLE);
      return nullptr;
    }

    mCameraManager = nsDOMCameraManager::CreateInstance(mWindow);
  }

  return mCameraManager;
}

already_AddRefed<ServiceWorkerContainer>
Navigator::ServiceWorker()
{
  MOZ_ASSERT(mWindow);

  if (!mServiceWorkerContainer) {
    mServiceWorkerContainer = new ServiceWorkerContainer(mWindow);
  }

  nsRefPtr<ServiceWorkerContainer> ref = mServiceWorkerContainer;
  return ref.forget();
}

size_t
Navigator::SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
{
  size_t n = aMallocSizeOf(this);

  // TODO: add SizeOfIncludingThis() to nsMimeTypeArray, bug 674113.
  // TODO: add SizeOfIncludingThis() to nsPluginArray, bug 674114.
  // TODO: add SizeOfIncludingThis() to Geolocation, bug 674115.
  // TODO: add SizeOfIncludingThis() to DesktopNotificationCenter, bug 674116.

  return n;
}

void
Navigator::SetWindow(nsPIDOMWindow *aInnerWindow)
{
  NS_ASSERTION(aInnerWindow->IsInnerWindow(),
               "Navigator must get an inner window!");
  mWindow = aInnerWindow;
}

void
Navigator::OnNavigation()
{
  if (!mWindow) {
    return;
  }

#ifdef MOZ_MEDIA_NAVIGATOR
  // Inform MediaManager in case there are live streams or pending callbacks.
  MediaManager *manager = MediaManager::Get();
  if (manager) {
    manager->OnNavigation(mWindow->WindowID());
  }
#endif
  if (mCameraManager) {
    mCameraManager->OnNavigation(mWindow->WindowID());
  }
}

bool
Navigator::CheckPermission(const char* type)
{
  return CheckPermission(mWindow, type);
}

/* static */
bool
Navigator::CheckPermission(nsPIDOMWindow* aWindow, const char* aType)
{
  if (!aWindow) {
    return false;
  }

  nsCOMPtr<nsIPermissionManager> permMgr =
    services::GetPermissionManager();
  NS_ENSURE_TRUE(permMgr, false);

  uint32_t permission = nsIPermissionManager::DENY_ACTION;
  permMgr->TestPermissionFromWindow(aWindow, aType, &permission);
  return permission == nsIPermissionManager::ALLOW_ACTION;
}

#ifdef MOZ_AUDIO_CHANNEL_MANAGER
system::AudioChannelManager*
Navigator::GetMozAudioChannelManager(ErrorResult& aRv)
{
  if (!mAudioChannelManager) {
    if (!mWindow) {
      aRv.Throw(NS_ERROR_UNEXPECTED);
      return nullptr;
    }
    mAudioChannelManager = new system::AudioChannelManager();
    mAudioChannelManager->Init(mWindow);
  }

  return mAudioChannelManager;
}
#endif

bool
Navigator::DoNewResolve(JSContext* aCx, JS::Handle<JSObject*> aObject,
                        JS::Handle<jsid> aId,
                        JS::MutableHandle<JSPropertyDescriptor> aDesc)
{
  if (!JSID_IS_STRING(aId)) {
    return true;
  }

  nsScriptNameSpaceManager* nameSpaceManager = GetNameSpaceManager();
  if (!nameSpaceManager) {
    return Throw(aCx, NS_ERROR_NOT_INITIALIZED);
  }

  nsAutoJSString name;
  if (!name.init(aCx, JSID_TO_STRING(aId))) {
    return false;
  }

  const nsGlobalNameStruct* name_struct =
    nameSpaceManager->LookupNavigatorName(name);
  if (!name_struct) {
    return true;
  }

  JS::Rooted<JSObject*> naviObj(aCx,
                                js::CheckedUnwrap(aObject,
                                                  /* stopAtOuter = */ false));
  if (!naviObj) {
    return Throw(aCx, NS_ERROR_DOM_SECURITY_ERR);
  }

  if (name_struct->mType == nsGlobalNameStruct::eTypeNewDOMBinding) {
    ConstructNavigatorProperty construct = name_struct->mConstructNavigatorProperty;
    MOZ_ASSERT(construct);

    JS::Rooted<JSObject*> domObject(aCx);
    {
      // Make sure to do the creation of our object in the compartment
      // of naviObj, especially since we plan to cache that object.
      JSAutoCompartment ac(aCx, naviObj);

      // Check whether our constructor is enabled after we unwrap Xrays, since
      // we don't want to define an interface on the Xray if it's disabled in
      // the target global, even if it's enabled in the Xray's global.
      if (name_struct->mConstructorEnabled &&
          !(*name_struct->mConstructorEnabled)(aCx, naviObj)) {
        return true;
      }

      if (name.EqualsLiteral("mozSettings")) {
        bool hasPermission = CheckPermission("settings-read") ||
                             CheckPermission("settings-write");
        if (!hasPermission) {
          FillPropertyDescriptor(aDesc, aObject, JS::NullValue(), false);
          return true;
        }
      }

      if (name.EqualsLiteral("mozDownloadManager")) {
        if (!CheckPermission("downloads")) {
          FillPropertyDescriptor(aDesc, aObject, JS::NullValue(), false);
          return true;
        }
      }

      nsISupports* existingObject = mCachedResolveResults.GetWeak(name);
      if (existingObject) {
        // We know all of our WebIDL objects here are wrappercached, so just go
        // ahead and WrapObject() them.  We can't use WrapNewBindingObject,
        // because we don't have the concrete type.
        JS::Rooted<JS::Value> wrapped(aCx);
        if (!dom::WrapObject(aCx, existingObject, &wrapped)) {
          return false;
        }
        domObject = &wrapped.toObject();
      } else {
        domObject = construct(aCx, naviObj);
        if (!domObject) {
          return Throw(aCx, NS_ERROR_FAILURE);
        }

        // Store the value in our cache
        nsISupports* native = UnwrapDOMObjectToISupports(domObject);
        MOZ_ASSERT(native);
        mCachedResolveResults.Put(name, native);
      }
    }

    if (!JS_WrapObject(aCx, &domObject)) {
      return false;
    }

    FillPropertyDescriptor(aDesc, aObject, JS::ObjectValue(*domObject), false);
    return true;
  }

  NS_ASSERTION(name_struct->mType == nsGlobalNameStruct::eTypeNavigatorProperty,
               "unexpected type");

  nsresult rv = NS_OK;

  nsCOMPtr<nsISupports> native;
  bool hadCachedNative = mCachedResolveResults.Get(name, getter_AddRefs(native));
  bool okToUseNative;
  JS::Rooted<JS::Value> prop_val(aCx);
  if (hadCachedNative) {
    okToUseNative = true;
  } else {
    native = do_CreateInstance(name_struct->mCID, &rv);
    if (NS_FAILED(rv)) {
      return Throw(aCx, rv);
    }

    nsCOMPtr<nsIDOMGlobalPropertyInitializer> gpi(do_QueryInterface(native));

    if (gpi) {
      if (!mWindow) {
        return Throw(aCx, NS_ERROR_UNEXPECTED);
      }

      rv = gpi->Init(mWindow, &prop_val);
      if (NS_FAILED(rv)) {
        return Throw(aCx, rv);
      }
    }

    okToUseNative = !prop_val.isObjectOrNull();
  }

  if (okToUseNative) {
    // Make sure to do the creation of our object in the compartment
    // of naviObj, especially since we plan to cache that object.
    JSAutoCompartment ac(aCx, naviObj);

    rv = nsContentUtils::WrapNative(aCx, native, &prop_val);

    if (NS_FAILED(rv)) {
      return Throw(aCx, rv);
    }

    // Now that we know we managed to wrap this thing properly, go ahead and
    // cache it as needed.
    if (!hadCachedNative) {
      mCachedResolveResults.Put(name, native);
    }
  }

  if (!JS_WrapValue(aCx, &prop_val)) {
    return Throw(aCx, NS_ERROR_UNEXPECTED);
  }

  FillPropertyDescriptor(aDesc, aObject, prop_val, false);
  return true;
}

struct NavigatorNameEnumeratorClosure
{
  NavigatorNameEnumeratorClosure(JSContext* aCx, JSObject* aWrapper,
                                 nsTArray<nsString>& aNames)
    : mCx(aCx),
      mWrapper(aCx, aWrapper),
      mNames(aNames)
  {
  }

  JSContext* mCx;
  JS::Rooted<JSObject*> mWrapper;
  nsTArray<nsString>& mNames;
};

static PLDHashOperator
SaveNavigatorName(const nsAString& aName,
                  const nsGlobalNameStruct& aNameStruct,
                  void* aClosure)
{
  NavigatorNameEnumeratorClosure* closure =
    static_cast<NavigatorNameEnumeratorClosure*>(aClosure);
  if (!aNameStruct.mConstructorEnabled ||
      aNameStruct.mConstructorEnabled(closure->mCx, closure->mWrapper)) {
    closure->mNames.AppendElement(aName);
  }
  return PL_DHASH_NEXT;
}

void
Navigator::GetOwnPropertyNames(JSContext* aCx, nsTArray<nsString>& aNames,
                               ErrorResult& aRv)
{
  nsScriptNameSpaceManager *nameSpaceManager = GetNameSpaceManager();
  if (!nameSpaceManager) {
    NS_ERROR("Can't get namespace manager.");
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return;
  }

  NavigatorNameEnumeratorClosure closure(aCx, GetWrapper(), aNames);
  nameSpaceManager->EnumerateNavigatorNames(SaveNavigatorName, &closure);
}

JSObject*
Navigator::WrapObject(JSContext* cx)
{
  return NavigatorBinding::Wrap(cx, this);
}

/* static */
bool
Navigator::HasWakeLockSupport(JSContext* /* unused*/, JSObject* /*unused */)
{
  nsCOMPtr<nsIPowerManagerService> pmService =
    do_GetService(POWERMANAGERSERVICE_CONTRACTID);
  // No service means no wake lock support
  return !!pmService;
}

/* static */
bool
Navigator::HasCameraSupport(JSContext* /* unused */, JSObject* aGlobal)
{
  nsCOMPtr<nsPIDOMWindow> win = GetWindowFromGlobal(aGlobal);
  return win && nsDOMCameraManager::CheckPermission(win);
}

/* static */
bool
Navigator::HasWifiManagerSupport(JSContext* /* unused */,
                                 JSObject* aGlobal)
{
  // On XBL scope, the global object is NOT |window|. So we have
  // to use nsContentUtils::GetObjectPrincipal to get the principal
  // and test directly with permission manager.

  nsIPrincipal* principal = nsContentUtils::ObjectPrincipal(aGlobal);

  nsCOMPtr<nsIPermissionManager> permMgr =
    services::GetPermissionManager();
  NS_ENSURE_TRUE(permMgr, false);

  uint32_t permission = nsIPermissionManager::DENY_ACTION;
  permMgr->TestPermissionFromPrincipal(principal, "wifi-manage", &permission);
  return nsIPermissionManager::ALLOW_ACTION == permission;
}

#ifdef MOZ_NFC
/* static */
bool
Navigator::HasNFCSupport(JSContext* /* unused */, JSObject* aGlobal)
{
  nsCOMPtr<nsPIDOMWindow> win = GetWindowFromGlobal(aGlobal);

  // Do not support NFC if NFC content helper does not exist.
  nsCOMPtr<nsISupports> contentHelper = do_GetService("@mozilla.org/nfc/content-helper;1");
  return !!contentHelper;
}
#endif // MOZ_NFC

#ifdef MOZ_MEDIA_NAVIGATOR
/* static */
bool
Navigator::HasUserMediaSupport(JSContext* /* unused */,
                               JSObject* /* unused */)
{
  // Make enabling peerconnection enable getUserMedia() as well
  return Preferences::GetBool("media.navigator.enabled", false) ||
         Preferences::GetBool("media.peerconnection.enabled", false);
}
#endif // MOZ_MEDIA_NAVIGATOR

/* static */
bool
Navigator::HasInputMethodSupport(JSContext* /* unused */,
                                 JSObject* aGlobal)
{
  nsCOMPtr<nsPIDOMWindow> win = GetWindowFromGlobal(aGlobal);
  if (!win || !Preferences::GetBool("dom.mozInputMethod.enabled", false)) {
    return false;
  }

  if (Preferences::GetBool("dom.mozInputMethod.testing", false)) {
    return true;
  }

  return CheckPermission(win, "input") ||
         CheckPermission(win, "input-manage");
}

/* static */
bool
Navigator::HasDataStoreSupport(nsIPrincipal* aPrincipal)
{
  workers::AssertIsOnMainThread();

  return DataStoreService::CheckPermission(aPrincipal);
}

// A WorkerMainThreadRunnable to synchronously dispatch the call of
// HasDataStoreSupport() from the worker thread to the main thread.
class HasDataStoreSupportRunnable MOZ_FINAL
  : public workers::WorkerMainThreadRunnable
{
public:
  bool mResult;

  HasDataStoreSupportRunnable(workers::WorkerPrivate* aWorkerPrivate)
    : workers::WorkerMainThreadRunnable(aWorkerPrivate)
    , mResult(false)
  {
    MOZ_ASSERT(aWorkerPrivate);
    aWorkerPrivate->AssertIsOnWorkerThread();
  }

protected:
  virtual bool
  MainThreadRun() MOZ_OVERRIDE
  {
    workers::AssertIsOnMainThread();

    mResult = Navigator::HasDataStoreSupport(mWorkerPrivate->GetPrincipal());

    return true;
  }
};

/* static */
bool
Navigator::HasDataStoreSupport(JSContext* aCx, JSObject* aGlobal)
{
  // If the caller is on the worker thread, dispatch this to the main thread.
  if (!NS_IsMainThread()) {
    workers::WorkerPrivate* workerPrivate =
      workers::GetWorkerPrivateFromContext(aCx);
    workerPrivate->AssertIsOnWorkerThread();

    nsRefPtr<HasDataStoreSupportRunnable> runnable =
      new HasDataStoreSupportRunnable(workerPrivate);
    runnable->Dispatch(aCx);

    return runnable->mResult;
  }

  workers::AssertIsOnMainThread();

  JS::Rooted<JSObject*> global(aCx, aGlobal);

  nsCOMPtr<nsPIDOMWindow> win = GetWindowFromGlobal(global);
  if (!win) {
    return false;
  }

  nsIDocument* doc = win->GetExtantDoc();
  if (!doc || !doc->NodePrincipal()) {
    return false;
  }

  return HasDataStoreSupport(doc->NodePrincipal());
}

#ifdef MOZ_B2G
/* static */
bool
Navigator::HasMobileIdSupport(JSContext* aCx, JSObject* aGlobal)
{
  nsCOMPtr<nsPIDOMWindow> win = GetWindowFromGlobal(aGlobal);
  if (!win) {
    return false;
  }

  nsIDocument* doc = win->GetExtantDoc();
  if (!doc) {
    return false;
  }

  nsIPrincipal* principal = doc->NodePrincipal();

  nsCOMPtr<nsIPermissionManager> permMgr = services::GetPermissionManager();
  NS_ENSURE_TRUE(permMgr, false);

  uint32_t permission = nsIPermissionManager::UNKNOWN_ACTION;
  permMgr->TestPermissionFromPrincipal(principal, "mobileid", &permission);
  return permission == nsIPermissionManager::PROMPT_ACTION ||
         permission == nsIPermissionManager::ALLOW_ACTION;
}
#endif

/* static */
already_AddRefed<nsPIDOMWindow>
Navigator::GetWindowFromGlobal(JSObject* aGlobal)
{
  nsCOMPtr<nsPIDOMWindow> win =
    do_QueryInterface(nsJSUtils::GetStaticScriptGlobal(aGlobal));
  MOZ_ASSERT(!win || win->IsInnerWindow());
  return win.forget();
}

} // namespace dom
} // namespace mozilla

nsresult
NS_GetNavigatorUserAgent(nsAString& aUserAgent)
{
  nsresult rv;

  nsCOMPtr<nsIHttpProtocolHandler>
    service(do_GetService(NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "http", &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString ua;
  rv = service->GetUserAgent(ua);
  CopyASCIItoUTF16(ua, aUserAgent);

  return rv;
}

nsresult
NS_GetNavigatorPlatform(nsAString& aPlatform)
{
  if (!nsContentUtils::IsCallerChrome()) {
    const nsAdoptingString& override =
      mozilla::Preferences::GetString("general.platform.override");

    if (override) {
      aPlatform = override;
      return NS_OK;
    }
  }

  nsresult rv;

  nsCOMPtr<nsIHttpProtocolHandler>
    service(do_GetService(NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "http", &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  // Sorry for the #if platform ugliness, but Communicator is likewise
  // hardcoded and we are seeking backward compatibility here (bug 47080).
#if defined(_WIN64)
  aPlatform.AssignLiteral("Win64");
#elif defined(WIN32)
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
nsresult
NS_GetNavigatorAppVersion(nsAString& aAppVersion)
{
  if (!nsContentUtils::IsCallerChrome()) {
    const nsAdoptingString& override =
      mozilla::Preferences::GetString("general.appversion.override");

    if (override) {
      aAppVersion = override;
      return NS_OK;
    }
  }

  nsresult rv;

  nsCOMPtr<nsIHttpProtocolHandler>
    service(do_GetService(NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "http", &rv));
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

void
NS_GetNavigatorAppName(nsAString& aAppName)
{
  if (!nsContentUtils::IsCallerChrome()) {
    const nsAdoptingString& override =
      mozilla::Preferences::GetString("general.appname.override");

    if (override) {
      aAppName = override;
      return;
    }
  }

  aAppName.AssignLiteral("Netscape");
}
