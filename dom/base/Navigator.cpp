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
#include "mozilla/MemoryReporting.h"
#include "mozilla/dom/DesktopNotification.h"
#include "mozilla/dom/File.h"
#include "nsGeolocation.h"
#include "nsIClassOfService.h"
#include "nsIHttpProtocolHandler.h"
#include "nsIContentPolicy.h"
#include "nsIContentSecurityPolicy.h"
#include "nsContentPolicyUtils.h"
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
#include "mozilla/dom/DeviceStorageAreaListener.h"
#ifdef MOZ_GAMEPAD
#include "mozilla/dom/GamepadServiceTest.h"
#endif
#include "mozilla/dom/PowerManager.h"
#include "mozilla/dom/WakeLock.h"
#include "mozilla/dom/power/PowerManagerService.h"
#include "mozilla/dom/CellBroadcast.h"
#include "mozilla/dom/FlyWebPublishedServer.h"
#include "mozilla/dom/FlyWebService.h"
#include "mozilla/dom/IccManager.h"
#include "mozilla/dom/InputPortManager.h"
#include "mozilla/dom/MobileMessageManager.h"
#include "mozilla/dom/Permissions.h"
#include "mozilla/dom/Presentation.h"
#include "mozilla/dom/ServiceWorkerContainer.h"
#include "mozilla/dom/TCPSocket.h"
#include "mozilla/dom/Telephony.h"
#include "mozilla/dom/Voicemail.h"
#include "mozilla/dom/TVManager.h"
#include "mozilla/dom/VRDevice.h"
#include "mozilla/dom/workers/RuntimeService.h"
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
#include "mozilla/dom/MobileConnectionArray.h"
#endif
#include "nsIIdleObserver.h"
#include "nsIPermissionManager.h"
#include "nsMimeTypes.h"
#include "nsNetUtil.h"
#include "nsStringStream.h"
#include "nsComponentManagerUtils.h"
#include "nsIStringStream.h"
#include "nsIHttpChannel.h"
#include "nsIHttpChannelInternal.h"
#include "TimeManager.h"
#include "DeviceStorage.h"
#include "nsStreamUtils.h"
#include "nsIAppsService.h"
#include "mozIApplication.h"
#include "WidgetUtils.h"
#include "nsIPresentationService.h"

#include "mozilla/dom/MediaDevices.h"
#include "MediaManager.h"
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
#include "nsJSUtils.h"

#include "nsScriptNameSpaceManager.h"

#include "mozilla/dom/NavigatorBinding.h"
#include "mozilla/dom/Promise.h"

#include "nsIUploadChannel2.h"
#include "mozilla/dom/FormData.h"
#include "nsIDocShell.h"

#include "WorkerPrivate.h"
#include "WorkerRunnable.h"

#if defined(XP_LINUX)
#include "mozilla/Hal.h"
#endif
#include "mozilla/dom/ContentChild.h"

#ifdef MOZ_EME
#include "mozilla/EMEUtils.h"
#include "mozilla/DetailedPromise.h"
#endif

#ifdef MOZ_WIDGET_GONK
#include <cutils/properties.h>
#endif

#ifdef MOZ_PAY
#include "nsIPaymentContentHelperService.h"
#include "mozilla/dom/DOMRequest.h"
#endif

namespace mozilla {
namespace dom {

static bool sVibratorEnabled   = false;
static uint32_t sMaxVibrateMS  = 0;
static uint32_t sMaxVibrateListLen = 0;
static const char* kVibrationPermissionType = "vibration";

static void
AddPermission(nsIPrincipal* aPrincipal, const char* aType, uint32_t aPermission,
              uint32_t aExpireType, int64_t aExpireTime)
{
  MOZ_ASSERT(aType);
  MOZ_ASSERT(aPrincipal);

  nsCOMPtr<nsIPermissionManager> permMgr = services::GetPermissionManager();
  if (!permMgr) {
    return;
  }
  permMgr->AddFromPrincipal(aPrincipal, aType, aPermission, aExpireType,
                            aExpireTime);
}

static uint32_t
GetPermission(nsPIDOMWindowInner* aWindow, const char* aType)
{
  MOZ_ASSERT(aType);

  uint32_t permission = nsIPermissionManager::UNKNOWN_ACTION;

  nsCOMPtr<nsIPermissionManager> permMgr = services::GetPermissionManager();
  if (!permMgr) {
    return permission;
  }
  permMgr->TestPermissionFromWindow(aWindow, aType, &permission);
  return permission;
}

static uint32_t
GetPermission(nsIPrincipal* aPrincipal, const char* aType)
{
  MOZ_ASSERT(aType);
  MOZ_ASSERT(aPrincipal);

  uint32_t permission = nsIPermissionManager::UNKNOWN_ACTION;

  nsCOMPtr<nsIPermissionManager> permMgr = services::GetPermissionManager();
  if (!permMgr) {
    return permission;
  }
  permMgr->TestPermissionFromPrincipal(aPrincipal, aType, &permission);
  return permission;
}

/* static */
void
Navigator::Init()
{
  Preferences::AddBoolVarCache(&sVibratorEnabled,
                               "dom.vibrator.enabled", true);
  Preferences::AddUintVarCache(&sMaxVibrateMS,
                               "dom.vibrator.max_vibrate_ms", 10000);
  Preferences::AddUintVarCache(&sMaxVibrateListLen,
                               "dom.vibrator.max_vibrate_list_len", 128);
}

Navigator::Navigator(nsPIDOMWindowInner* aWindow)
  : mWindow(aWindow)
{
  MOZ_ASSERT(aWindow->IsInnerWindow(), "Navigator must get an inner window!");
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
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(Navigator)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mMimeTypes)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPlugins)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPermissions)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mGeolocation)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mNotification)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mBatteryManager)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mBatteryPromise)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPowerManager)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mCellBroadcast)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mIccManager)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mMobileMessageManager)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mTelephony)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mVoicemail)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mTVManager)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mInputPortManager)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mConnection)
#ifdef MOZ_B2G_RIL
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mMobileConnections)
#endif
#ifdef MOZ_B2G_BT
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mBluetooth)
#endif
#ifdef MOZ_AUDIO_CHANNEL_MANAGER
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mAudioChannelManager)
#endif
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mCameraManager)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mMediaDevices)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mTimeManager)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mServiceWorkerContainer)

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mWindow)
#ifdef MOZ_EME
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mMediaKeySystemAccessManager)
#endif
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDeviceStorageAreaListener)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPresentation)
#ifdef MOZ_GAMEPAD
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mGamepadServiceTest)
#endif
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mVRGetDevicesPromises)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_WRAPPERCACHE(Navigator)

void
Navigator::Invalidate()
{
  // Don't clear mWindow here so we know we've got a non-null mWindow
  // until we're unlinked.

  mMimeTypes = nullptr;

  if (mPlugins) {
    mPlugins->Invalidate();
    mPlugins = nullptr;
  }

  mPermissions = nullptr;

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

  mBatteryPromise = nullptr;

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

  if (mCellBroadcast) {
    mCellBroadcast = nullptr;
  }

  if (mIccManager) {
    mIccManager->Shutdown();
    mIccManager = nullptr;
  }

  if (mMobileMessageManager) {
    mMobileMessageManager->Shutdown();
    mMobileMessageManager = nullptr;
  }

  if (mTelephony) {
    mTelephony = nullptr;
  }

  if (mVoicemail) {
    mVoicemail->Shutdown();
    mVoicemail = nullptr;
  }

  if (mTVManager) {
    mTVManager = nullptr;
  }

  if (mInputPortManager) {
    mInputPortManager = nullptr;
  }

  if (mConnection) {
    mConnection->Shutdown();
    mConnection = nullptr;
  }

#ifdef MOZ_B2G_RIL
  if (mMobileConnections) {
    mMobileConnections = nullptr;
  }
#endif

#ifdef MOZ_B2G_BT
  if (mBluetooth) {
    mBluetooth = nullptr;
  }
#endif

  mCameraManager = nullptr;
  mMediaDevices = nullptr;

#ifdef MOZ_AUDIO_CHANNEL_MANAGER
  if (mAudioChannelManager) {
    mAudioChannelManager = nullptr;
  }
#endif

  uint32_t len = mDeviceStorageStores.Length();
  for (uint32_t i = 0; i < len; ++i) {
    RefPtr<nsDOMDeviceStorage> ds = do_QueryReferent(mDeviceStorageStores[i]);
    if (ds) {
      ds->Shutdown();
    }
  }
  mDeviceStorageStores.Clear();

  if (mTimeManager) {
    mTimeManager = nullptr;
  }

  if (mPresentation) {
    mPresentation = nullptr;
  }

  mServiceWorkerContainer = nullptr;

#ifdef MOZ_EME
  if (mMediaKeySystemAccessManager) {
    mMediaKeySystemAccessManager->Shutdown();
    mMediaKeySystemAccessManager = nullptr;
  }
#endif

  if (mDeviceStorageAreaListener) {
    mDeviceStorageAreaListener = nullptr;
  }

#ifdef MOZ_GAMEPAD
  if (mGamepadServiceTest) {
    mGamepadServiceTest->Shutdown();
    mGamepadServiceTest = nullptr;
  }
#endif

  mVRGetDevicesPromises.Clear();
}

//*****************************************************************************
//    Navigator::nsIDOMNavigator
//*****************************************************************************

NS_IMETHODIMP
Navigator::GetUserAgent(nsAString& aUserAgent)
{
  nsCOMPtr<nsIURI> codebaseURI;
  nsCOMPtr<nsPIDOMWindowInner> window;

  if (mWindow) {
    window = mWindow;
    nsIDocShell* docshell = window->GetDocShell();
    nsString customUserAgent;
    if (docshell) {
      docshell->GetCustomUserAgent(customUserAgent);

      if (!customUserAgent.IsEmpty()) {
        aUserAgent = customUserAgent;
        return NS_OK;
      }

      nsIDocument* doc = mWindow->GetExtantDoc();
      if (doc) {
        doc->NodePrincipal()->GetURI(getter_AddRefs(codebaseURI));
      }
    }
  }

  return GetUserAgent(window, codebaseURI, nsContentUtils::IsCallerChrome(),
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
  return GetAppVersion(aAppVersion, /* aUsePrefOverriddenValue */ true);
}

NS_IMETHODIMP
Navigator::GetAppName(nsAString& aAppName)
{
  AppName(aAppName, /* aUsePrefOverriddenValue */ true);
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
/* static */ void
Navigator::GetAcceptLanguages(nsTArray<nsString>& aLanguages)
{
  MOZ_ASSERT(NS_IsMainThread());

  aLanguages.Clear();

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
  return GetPlatform(aPlatform, /* aUsePrefOverriddenValue */ true);
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

Permissions*
Navigator::GetPermissions(ErrorResult& aRv)
{
  if (!mWindow) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  if (!mPermissions) {
    mPermissions = new Permissions(mWindow);
  }

  return mPermissions;
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
  if (mWindow && mWindow->GetDoc()) {
    return !NS_IsOffline() &&
      !NS_IsAppOffline(mWindow->GetDoc()->NodePrincipal());
  }

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
  bool doNotTrack = nsContentUtils::DoNotTrackEnabled();
  if (!doNotTrack) {
    nsCOMPtr<nsILoadContext> loadContext = do_GetInterface(mWindow);
    doNotTrack = loadContext && loadContext->UseTrackingProtection();
  }

  if (doNotTrack) {
    aResult.AssignLiteral("1");
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

uint64_t
Navigator::HardwareConcurrency()
{
  workers::RuntimeService* rts = workers::RuntimeService::GetOrCreateService();
  if (!rts) {
    return 1;
  }

  return rts->ClampedHardwareConcurrency();
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
  VibrateWindowListener(nsPIDOMWindowInner* aWindow, nsIDocument* aDocument)
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

static bool
MayVibrate(nsIDocument* doc) {
#if MOZ_WIDGET_GONK
  if (XRE_IsParentProcess()) {
    return true; // The system app can always vibrate
  }
#endif // MOZ_WIDGET_GONK

  // Hidden documents cannot start or stop a vibration.
  return (doc && !doc->Hidden());
}

NS_IMETHODIMP
VibrateWindowListener::HandleEvent(nsIDOMEvent* aEvent)
{
  nsCOMPtr<nsIDocument> doc =
    do_QueryInterface(aEvent->InternalDOMEvent()->GetTarget());

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

} // namespace

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

void
Navigator::SetVibrationPermission(bool aPermitted, bool aPersistent)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsTArray<uint32_t> pattern;
  pattern.SwapElements(mRequestedVibrationPattern);

  if (!mWindow) {
    return;
  }

  nsCOMPtr<nsIDocument> doc = mWindow->GetExtantDoc();

  if (!MayVibrate(doc)) {
    return;
  }

  if (aPermitted) {
    // Add a listener to cancel the vibration if the document becomes hidden,
    // and remove the old visibility listener, if there was one.
    if (!gVibrateWindowListener) {
      // If gVibrateWindowListener is null, this is the first time we've vibrated,
      // and we need to register a listener to clear gVibrateWindowListener on
      // shutdown.
      ClearOnShutdown(&gVibrateWindowListener);
    } else {
      gVibrateWindowListener->RemoveListener();
    }
    gVibrateWindowListener = new VibrateWindowListener(mWindow, doc);
    hal::Vibrate(pattern, mWindow);
  }

  if (aPersistent) {
    AddPermission(doc->NodePrincipal(), kVibrationPermissionType,
                  aPermitted ? nsIPermissionManager::ALLOW_ACTION :
                               nsIPermissionManager::DENY_ACTION,
                  nsIPermissionManager::EXPIRE_SESSION, 0);
  }
}

bool
Navigator::Vibrate(uint32_t aDuration)
{
  AutoTArray<uint32_t, 1> pattern;
  pattern.AppendElement(aDuration);
  return Vibrate(pattern);
}

bool
Navigator::Vibrate(const nsTArray<uint32_t>& aPattern)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!mWindow) {
    return false;
  }

  nsCOMPtr<nsIDocument> doc = mWindow->GetExtantDoc();

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
  uint32_t permission = GetPermission(mWindow, kVibrationPermissionType);

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

uint32_t
Navigator::MaxTouchPoints()
{
  nsCOMPtr<nsIWidget> widget = widget::WidgetUtils::DOMWindowToWidget(mWindow->GetOuterWindow());

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

DeviceStorageAreaListener*
Navigator::GetDeviceStorageAreaListener(ErrorResult& aRv)
{
  if (!mDeviceStorageAreaListener) {
    if (!mWindow || !mWindow->GetOuterWindow() || !mWindow->GetDocShell()) {
      aRv.Throw(NS_ERROR_FAILURE);
      return nullptr;
    }
    mDeviceStorageAreaListener = new DeviceStorageAreaListener(mWindow);
  }

  return mDeviceStorageAreaListener;
}

already_AddRefed<nsDOMDeviceStorage>
Navigator::FindDeviceStorage(const nsAString& aName, const nsAString& aType)
{
  auto i = mDeviceStorageStores.Length();
  while (i > 0) {
    --i;
    RefPtr<nsDOMDeviceStorage> storage =
      do_QueryReferent(mDeviceStorageStores[i]);
    if (storage) {
      if (storage->Equals(mWindow, aName, aType)) {
        return storage.forget();
      }
    } else {
      mDeviceStorageStores.RemoveElementAt(i);
    }
  }
  return nullptr;
}

already_AddRefed<nsDOMDeviceStorage>
Navigator::GetDeviceStorage(const nsAString& aType, ErrorResult& aRv)
{
  if (!mWindow || !mWindow->GetOuterWindow() || !mWindow->GetDocShell()) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsString name;
  nsDOMDeviceStorage::GetDefaultStorageName(aType, name);
  RefPtr<nsDOMDeviceStorage> storage = FindDeviceStorage(name, aType);
  if (storage) {
    return storage.forget();
  }

  nsDOMDeviceStorage::CreateDeviceStorageFor(mWindow, aType,
                                             getter_AddRefs(storage));

  if (!storage) {
    return nullptr;
  }

  mDeviceStorageStores.AppendElement(
    do_GetWeakReference(static_cast<DOMEventTargetHelper*>(storage)));
  return storage.forget();
}

void
Navigator::GetDeviceStorages(const nsAString& aType,
                             nsTArray<RefPtr<nsDOMDeviceStorage> >& aStores,
                             ErrorResult& aRv)
{
  if (!mWindow || !mWindow->GetOuterWindow() || !mWindow->GetDocShell()) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  nsDOMDeviceStorage::VolumeNameArray volumes;
  nsDOMDeviceStorage::GetOrderedVolumeNames(aType, volumes);
  if (volumes.IsEmpty()) {
    RefPtr<nsDOMDeviceStorage> storage = GetDeviceStorage(aType, aRv);
    if (storage) {
      aStores.AppendElement(storage.forget());
    }
  } else {
    uint32_t len = volumes.Length();
    aStores.SetCapacity(len);
    for (uint32_t i = 0; i < len; ++i) {
      RefPtr<nsDOMDeviceStorage> storage =
        GetDeviceStorageByNameAndType(volumes[i], aType, aRv);
      if (aRv.Failed()) {
        break;
      }

      if (storage) {
        aStores.AppendElement(storage.forget());
      }
    }
  }
}

already_AddRefed<nsDOMDeviceStorage>
Navigator::GetDeviceStorageByNameAndType(const nsAString& aName,
                                         const nsAString& aType,
                                         ErrorResult& aRv)
{
  if (!mWindow || !mWindow->GetOuterWindow() || !mWindow->GetDocShell()) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<nsDOMDeviceStorage> storage = FindDeviceStorage(aName, aType);
  if (storage) {
    return storage.forget();
  }
  nsDOMDeviceStorage::CreateDeviceStorageByNameAndType(mWindow, aName, aType,
                                                       getter_AddRefs(storage));

  if (!storage) {
    return nullptr;
  }

  mDeviceStorageStores.AppendElement(
    do_GetWeakReference(static_cast<DOMEventTargetHelper*>(storage)));
  return storage.forget();
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
  if (NS_FAILED(mGeolocation->Init(mWindow))) {
    mGeolocation = nullptr;
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  return mGeolocation;
}

class BeaconStreamListener final : public nsIStreamListener
{
    ~BeaconStreamListener() {}

  public:
    BeaconStreamListener() : mLoadGroup(nullptr) {}

    void SetLoadGroup(nsILoadGroup* aLoadGroup) {
      mLoadGroup = aLoadGroup;
    }

    NS_DECL_ISUPPORTS
    NS_DECL_NSISTREAMLISTENER
    NS_DECL_NSIREQUESTOBSERVER

  private:
    nsCOMPtr<nsILoadGroup> mLoadGroup;

};

NS_IMPL_ISUPPORTS(BeaconStreamListener,
                  nsIStreamListener,
                  nsIRequestObserver)

NS_IMETHODIMP
BeaconStreamListener::OnStartRequest(nsIRequest *aRequest,
                                     nsISupports *aContext)
{
  // release the loadgroup first
  mLoadGroup = nullptr;

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

  // Explicitly disallow loading data: URIs
  bool isDataScheme = false;
  rv = uri->SchemeIs("data", &isDataScheme);
  if (NS_FAILED(rv) || isDataScheme) {
    aRv.Throw(NS_ERROR_CONTENT_BLOCKED);
    return false;
  }

  nsLoadFlags loadFlags = nsIRequest::LOAD_NORMAL |
    nsIChannel::LOAD_CLASSIFY_URI;

  nsCOMPtr<nsIChannel> channel;
  rv = NS_NewChannel(getter_AddRefs(channel),
                     uri,
                     doc,
                     nsILoadInfo::SEC_REQUIRE_CORS_DATA_INHERITS |
                       nsILoadInfo::SEC_COOKIES_INCLUDE,
                     nsIContentPolicy::TYPE_BEACON,
                     nullptr, // aLoadGroup
                     nullptr, // aCallbacks
                     loadFlags);

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
  httpChannel->SetReferrer(documentURI);

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
      Blob& blob = aData.Value().GetAsBlob();
      blob.GetInternalStream(getter_AddRefs(in), aRv);
      if (NS_WARN_IF(aRv.Failed())) {
        return false;
      }

      nsAutoString type;
      blob.GetType(type);
      mimeType = NS_ConvertUTF16toUTF8(type);

    } else if (aData.Value().IsFormData()) {
      FormData& form = aData.Value().GetAsFormData();
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
  rv = channel->AsyncOpen2(beaconListener);
  // do not throw if security checks fail within asyncOpen2
  NS_ENSURE_SUCCESS(rv, false);

  // make the beaconListener hold a strong reference to the loadgroup
  // which is released in ::OnStartRequest
  beaconListener->SetLoadGroup(loadGroup);

  return true;
}

MediaDevices*
Navigator::GetMediaDevices(ErrorResult& aRv)
{
  if (!mMediaDevices) {
    if (!mWindow ||
        !mWindow->GetOuterWindow() ||
        mWindow->GetOuterWindow()->GetCurrentInnerWindow() != mWindow) {
      aRv.Throw(NS_ERROR_NOT_AVAILABLE);
      return nullptr;
    }
    mMediaDevices = new MediaDevices(mWindow);
  }
  return mMediaDevices;
}

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

  MediaManager* manager = MediaManager::Get();
  aRv = manager->GetUserMedia(mWindow, aConstraints, onsuccess, onerror);
}

void
Navigator::MozGetUserMediaDevices(const MediaStreamConstraints& aConstraints,
                                  MozGetUserMediaDevicesSuccessCallback& aOnSuccess,
                                  NavigatorUserMediaErrorCallback& aOnError,
                                  uint64_t aInnerWindowID,
                                  const nsAString& aCallID,
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
                                     aInnerWindowID, aCallID);
}

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

Promise*
Navigator::GetBattery(ErrorResult& aRv)
{
  if (mBatteryPromise) {
    return mBatteryPromise;
  }

  if (!mWindow || !mWindow->GetDocShell()) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  nsCOMPtr<nsIGlobalObject> go = do_QueryInterface(mWindow);
  RefPtr<Promise> batteryPromise = Promise::Create(go, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }
  mBatteryPromise = batteryPromise;

  // We just initialized mBatteryPromise, so we know this is the first time
  // this page has accessed navigator.getBattery(). 1 = navigator.getBattery()
  Telemetry::Accumulate(Telemetry::BATTERY_STATUS_COUNT, 1);

  if (!mBatteryManager) {
    mBatteryManager = new battery::BatteryManager(mWindow);
    mBatteryManager->Init();
  }

  mBatteryPromise->MaybeResolve(mBatteryManager);

  return mBatteryPromise;
}

already_AddRefed<Promise>
Navigator::PublishServer(const nsAString& aName,
                         const FlyWebPublishOptions& aOptions,
                         ErrorResult& aRv)
{
  RefPtr<FlyWebService> service = FlyWebService::GetOrCreate();
  if (!service) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<FlyWebPublishPromise> mozPromise =
    service->PublishServer(aName, aOptions, mWindow);
  MOZ_ASSERT(mozPromise);

  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(mWindow);
  ErrorResult result;
  RefPtr<Promise> domPromise = Promise::Create(global, result);
  if (result.Failed()) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  mozPromise->Then(AbstractThread::MainThread(),
                   __func__,
                   [domPromise] (FlyWebPublishedServer* aServer) {
                     domPromise->MaybeResolve(aServer);
                   },
                   [domPromise] (nsresult aStatus) {
                     domPromise->MaybeReject(aStatus);
                   });

  return domPromise.forget();
}

already_AddRefed<Promise>
Navigator::GetFeature(const nsAString& aName, ErrorResult& aRv)
{
  nsCOMPtr<nsIGlobalObject> go = do_QueryInterface(mWindow);
  RefPtr<Promise> p = Promise::Create(go, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

#if defined(XP_LINUX)
  if (aName.EqualsLiteral("hardware.memory")) {
    // with seccomp enabled, fopen() should be in a non-sandboxed process
    if (XRE_IsParentProcess()) {
      uint32_t memLevel = mozilla::hal::GetTotalSystemMemoryLevel();
      if (memLevel == 0) {
        p->MaybeReject(NS_ERROR_NOT_AVAILABLE);
        return p.forget();
      }
      p->MaybeResolve((int)memLevel);
    } else {
      mozilla::dom::ContentChild* cc =
        mozilla::dom::ContentChild::GetSingleton();
      RefPtr<Promise> ipcRef(p);
      cc->SendGetSystemMemory(reinterpret_cast<uint64_t>(ipcRef.forget().take()));
    }
    return p.forget();
  } // hardware.memory
#endif

#ifdef MOZ_WIDGET_GONK
  if (StringBeginsWith(aName, NS_LITERAL_STRING("acl.")) &&
      (aName.EqualsLiteral("acl.version") || CheckPermission("external-app"))) {
    char value[PROPERTY_VALUE_MAX];
    nsCString propertyKey("persist.");
    propertyKey.Append(NS_ConvertUTF16toUTF8(aName));
    uint32_t len = property_get(propertyKey.get(), value, nullptr);
    if (len > 0) {
      p->MaybeResolve(NS_ConvertUTF8toUTF16(value));
      return p.forget();
    }
  }
#endif

  // Mirror the dom.apps.developer_mode pref to let apps get it read-only.
  if (aName.EqualsLiteral("dom.apps.developer_mode")) {
    p->MaybeResolve(Preferences::GetBool("dom.apps.developer_mode", false));
    return p.forget();
  }

  p->MaybeResolve(JS::UndefinedHandleValue);
  return p.forget();
}

already_AddRefed<Promise>
Navigator::HasFeature(const nsAString& aName, ErrorResult& aRv)
{
  nsCOMPtr<nsIGlobalObject> go = do_QueryInterface(mWindow);
  RefPtr<Promise> p = Promise::Create(go, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  // Hardcoded extensions features which are b2g specific.
#ifdef MOZ_B2G
  if (aName.EqualsLiteral("web-extensions") ||
      aName.EqualsLiteral("late-customization")) {
    p->MaybeResolve(true);
    return p.forget();
  }
#endif

  // Hardcoded manifest features. Some are still b2g specific.
  const char manifestFeatures[][64] = {
    "manifest.origin"
  , "manifest.redirects"
#ifdef MOZ_B2G
  , "manifest.chrome.navigation"
  , "manifest.precompile"
  , "manifest.role.homescreen"
#endif
  };

  nsAutoCString feature = NS_ConvertUTF16toUTF8(aName);
  for (uint32_t i = 0; i < MOZ_ARRAY_LENGTH(manifestFeatures); i++) {
    if (feature.Equals(manifestFeatures[i])) {
      p->MaybeResolve(true);
      return p.forget();
    }
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

    p->MaybeResolve(JS::UndefinedHandleValue);
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

  RefPtr<power::PowerManagerService> pmService =
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

TVManager*
Navigator::GetTv()
{
  if (!mTVManager) {
    if (!mWindow) {
      return nullptr;
    }
    mTVManager = TVManager::Create(mWindow);
  }

  return mTVManager;
}

InputPortManager*
Navigator::GetInputPortManager(ErrorResult& aRv)
{
  if (!mInputPortManager) {
    if (!mWindow) {
      aRv.Throw(NS_ERROR_FAILURE);
      return nullptr;
    }
    mInputPortManager = InputPortManager::Create(mWindow, aRv);
    if (NS_WARN_IF(aRv.Failed())) {
      return nullptr;
    }
  }

  return mInputPortManager;
}

already_AddRefed<LegacyMozTCPSocket>
Navigator::MozTCPSocket()
{
  RefPtr<LegacyMozTCPSocket> socket = new LegacyMozTCPSocket(GetWindow());
  return socket.forget();
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

  RefPtr<Promise> p = static_cast<Promise*>(promise.get());
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

#endif // MOZ_B2G_RIL

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

    mVoicemail = Voicemail::Create(mWindow, aRv);
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

#ifdef MOZ_GAMEPAD
void
Navigator::GetGamepads(nsTArray<RefPtr<Gamepad> >& aGamepads,
                       ErrorResult& aRv)
{
  if (!mWindow) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return;
  }
  NS_ENSURE_TRUE_VOID(mWindow->GetDocShell());
  nsGlobalWindow* win = nsGlobalWindow::Cast(mWindow);
  win->SetHasGamepadEventListener(true);
  win->GetGamepads(aGamepads);
}

GamepadServiceTest*
Navigator::RequestGamepadServiceTest()
{
  if (!mGamepadServiceTest) {
    mGamepadServiceTest = GamepadServiceTest::CreateTestService(mWindow);
  }
  return mGamepadServiceTest;
}
#endif

already_AddRefed<Promise>
Navigator::GetVRDevices(ErrorResult& aRv)
{
  if (!mWindow || !mWindow->GetDocShell()) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  nsCOMPtr<nsIGlobalObject> go = do_QueryInterface(mWindow);
  RefPtr<Promise> p = Promise::Create(go, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  // We pass ourself to RefreshVRDevices, so NotifyVRDevicesUpdated will
  // be called asynchronously, resolving the promises in mVRGetDevicesPromises.
  if (!VRDevice::RefreshVRDevices(this)) {
    p->MaybeReject(NS_ERROR_FAILURE);
    return p.forget();
  }

  mVRGetDevicesPromises.AppendElement(p);
  return p.forget();
}

void
Navigator::NotifyVRDevicesUpdated()
{
  // Synchronize the VR devices and resolve the promises in
  // mVRGetDevicesPromises
  nsGlobalWindow* win = nsGlobalWindow::Cast(mWindow);

  nsTArray<RefPtr<VRDevice>> vrDevs;
  if (win->UpdateVRDevices(vrDevs)) {
    for (auto p: mVRGetDevicesPromises) {
      p->MaybeResolve(vrDevs);
    }
  } else {
    for (auto p: mVRGetDevicesPromises) {
      p->MaybeReject(NS_ERROR_FAILURE);
    }
  }
  mVRGetDevicesPromises.Clear();
}

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
    mConnection = new network::Connection(mWindow);
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

  RefPtr<ServiceWorkerContainer> ref = mServiceWorkerContainer;
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
Navigator::SetWindow(nsPIDOMWindowInner *aInnerWindow)
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

  // If MediaManager is open let it inform any live streams or pending callbacks
  MediaManager *manager = MediaManager::GetIfExists();
  if (manager) {
    manager->OnNavigation(mWindow->WindowID());
  }
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
Navigator::CheckPermission(nsPIDOMWindowInner* aWindow, const char* aType)
{
  if (!aWindow) {
    return false;
  }

  uint32_t permission = GetPermission(aWindow, aType);
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

JSObject*
Navigator::WrapObject(JSContext* cx, JS::Handle<JSObject*> aGivenProto)
{
  return NavigatorBinding::Wrap(cx, this, aGivenProto);
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
  nsCOMPtr<nsPIDOMWindowInner> win = GetWindowFromGlobal(aGlobal);
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
  uint32_t permission = GetPermission(principal, "wifi-manage");

  return permission == nsIPermissionManager::ALLOW_ACTION;
}

#ifdef MOZ_NFC
/* static */
bool
Navigator::HasNFCSupport(JSContext* /* unused */, JSObject* aGlobal)
{
  nsCOMPtr<nsPIDOMWindowInner> win = GetWindowFromGlobal(aGlobal);

  // Do not support NFC if NFC content helper does not exist.
  nsCOMPtr<nsISupports> contentHelper = do_GetService("@mozilla.org/nfc/content-helper;1");
  return !!contentHelper;
}
#endif // MOZ_NFC

/* static */
bool
Navigator::HasUserMediaSupport(JSContext* /* unused */,
                               JSObject* /* unused */)
{
  // Make enabling peerconnection enable getUserMedia() as well
  return Preferences::GetBool("media.navigator.enabled", false) ||
         Preferences::GetBool("media.peerconnection.enabled", false);
}

#ifdef MOZ_B2G
/* static */
bool
Navigator::HasMobileIdSupport(JSContext* aCx, JSObject* aGlobal)
{
  nsCOMPtr<nsPIDOMWindowInner> win = GetWindowFromGlobal(aGlobal);
  if (!win) {
    return false;
  }

  nsIDocument* doc = win->GetExtantDoc();
  if (!doc) {
    return false;
  }

  nsIPrincipal* principal = doc->NodePrincipal();
  uint32_t permission = GetPermission(principal, "mobileid");

  return permission == nsIPermissionManager::PROMPT_ACTION ||
         permission == nsIPermissionManager::ALLOW_ACTION;
}
#endif

/* static */
bool
Navigator::HasPresentationSupport(JSContext* aCx, JSObject* aGlobal)
{
  JS::Rooted<JSObject*> global(aCx, aGlobal);

  nsCOMPtr<nsPIDOMWindowInner> inner = GetWindowFromGlobal(global);
  if (NS_WARN_IF(!inner)) {
    return false;
  }

  // Grant access if it has the permission.
  if (CheckPermission(inner, "presentation")) {
    return true;
  }

  // Grant access to browser receiving pages and their same-origin iframes. (App
  // pages should be controlled by "presentation" permission in app manifests.)
  nsCOMPtr<nsIDocShell> docshell = inner->GetDocShell();
  if (!docshell) {
    return false;
  }

  if (!docshell->GetIsInMozBrowserOrApp()) {
    return false;
  }

  nsAutoString presentationURL;
  nsContentUtils::GetPresentationURL(docshell, presentationURL);

  if (presentationURL.IsEmpty()) {
    return false;
  }

  nsCOMPtr<nsIScriptSecurityManager> securityManager =
    do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID);
  if (!securityManager) {
    return false;
  }

  nsCOMPtr<nsIURI> presentationURI;
  nsresult rv = NS_NewURI(getter_AddRefs(presentationURI), presentationURL);
  if (NS_FAILED(rv)) {
    return false;
  }

  nsCOMPtr<nsIURI> docURI = inner->GetDocumentURI();
  return NS_SUCCEEDED(securityManager->CheckSameOriginURI(presentationURI,
                                                          docURI,
                                                          false));
}

/* static */
bool
Navigator::IsE10sEnabled(JSContext* aCx, JSObject* aGlobal)
{
  return XRE_IsContentProcess();
}

bool
Navigator::MozE10sEnabled()
{
  // This will only be called if IsE10sEnabled() is true.
  return true;
}

#ifdef MOZ_PAY
already_AddRefed<DOMRequest>
Navigator::MozPay(JSContext* aCx,
                  JS::Handle<JS::Value> aJwts,
                  ErrorResult& aRv)
{
  if (!mWindow || !mWindow->GetDocShell()) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  nsresult rv;
  nsCOMPtr<nsIPaymentContentHelperService> service =
    do_GetService("@mozilla.org/payment/content-helper-service;1", &rv);
  if (!service) {
    aRv.Throw(rv);
    return nullptr;
  }

  RefPtr<nsIDOMDOMRequest> request;
  rv = service->Pay(mWindow, aJwts, getter_AddRefs(request));
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return nullptr;
  }

  return request.forget().downcast<DOMRequest>();
}
#endif // MOZ_PAY

/* static */
already_AddRefed<nsPIDOMWindowInner>
Navigator::GetWindowFromGlobal(JSObject* aGlobal)
{
  nsCOMPtr<nsPIDOMWindowInner> win =
    do_QueryInterface(nsJSUtils::GetStaticScriptGlobal(aGlobal));
  MOZ_ASSERT(!win || win->IsInnerWindow());
  return win.forget();
}

nsresult
Navigator::GetPlatform(nsAString& aPlatform, bool aUsePrefOverriddenValue)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (aUsePrefOverriddenValue && !nsContentUtils::IsCallerChrome()) {
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

/* static */ nsresult
Navigator::GetAppVersion(nsAString& aAppVersion, bool aUsePrefOverriddenValue)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (aUsePrefOverriddenValue && !nsContentUtils::IsCallerChrome()) {
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

/* static */ void
Navigator::AppName(nsAString& aAppName, bool aUsePrefOverriddenValue)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (aUsePrefOverriddenValue && !nsContentUtils::IsCallerChrome()) {
    const nsAdoptingString& override =
      mozilla::Preferences::GetString("general.appname.override");

    if (override) {
      aAppName = override;
      return;
    }
  }

  aAppName.AssignLiteral("Netscape");
}

void
Navigator::ClearUserAgentCache()
{
  NavigatorBinding::ClearCachedUserAgentValue(this);
}

nsresult
Navigator::GetUserAgent(nsPIDOMWindowInner* aWindow, nsIURI* aURI,
                        bool aIsCallerChrome,
                        nsAString& aUserAgent)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!aIsCallerChrome) {
    const nsAdoptingString& override =
      mozilla::Preferences::GetString("general.useragent.override");

    if (override) {
      aUserAgent = override;
      return NS_OK;
    }
  }

  nsresult rv;
  nsCOMPtr<nsIHttpProtocolHandler>
    service(do_GetService(NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "http", &rv));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsAutoCString ua;
  rv = service->GetUserAgent(ua);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  CopyASCIItoUTF16(ua, aUserAgent);

  if (!aWindow || !aURI) {
    return NS_OK;
  }

  MOZ_ASSERT(aWindow->GetDocShell());

  nsCOMPtr<nsISiteSpecificUserAgent> siteSpecificUA =
    do_GetService("@mozilla.org/dom/site-specific-user-agent;1");
  if (!siteSpecificUA) {
    return NS_OK;
  }

  return siteSpecificUA->GetUserAgentForURIAndWindow(aURI, aWindow, aUserAgent);
}

#ifdef MOZ_EME
static nsCString
ToCString(const nsString& aString)
{
  nsCString str("'");
  str.Append(NS_ConvertUTF16toUTF8(aString));
  str.AppendLiteral("'");
  return str;
}

static nsCString
ToCString(const MediaKeysRequirement aValue)
{
  nsCString str("'");
  str.Append(nsDependentCString(MediaKeysRequirementValues::strings[static_cast<uint32_t>(aValue)].value));
  str.AppendLiteral("'");
  return str;
}

static nsCString
ToCString(const MediaKeySystemMediaCapability& aValue)
{
  nsCString str;
  str.AppendLiteral("{contentType=");
  str.Append(ToCString(aValue.mContentType));
  str.AppendLiteral(", robustness=");
  str.Append(ToCString(aValue.mRobustness));
  str.AppendLiteral("}");
  return str;
}

template<class Type>
static nsCString
ToCString(const Sequence<Type>& aSequence)
{
  nsCString str;
  str.AppendLiteral("[");
  for (size_t i = 0; i < aSequence.Length(); i++) {
    if (i != 0) {
      str.AppendLiteral(",");
    }
    str.Append(ToCString(aSequence[i]));
  }
  str.AppendLiteral("]");
  return str;
}

template<class Type>
static nsCString
ToCString(const Optional<Sequence<Type>>& aOptional)
{
  nsCString str;
  if (aOptional.WasPassed()) {
    str.Append(ToCString(aOptional.Value()));
  } else {
    str.AppendLiteral("[]");
  }
  return str;
}

static nsCString
ToCString(const MediaKeySystemConfiguration& aConfig)
{
  nsCString str;
  str.AppendLiteral("{label=");
  str.Append(ToCString(aConfig.mLabel));

  str.AppendLiteral(", initDataTypes=");
  str.Append(ToCString(aConfig.mInitDataTypes));

  str.AppendLiteral(", audioCapabilities=");
  str.Append(ToCString(aConfig.mAudioCapabilities));

  str.AppendLiteral(", videoCapabilities=");
  str.Append(ToCString(aConfig.mVideoCapabilities));

  str.AppendLiteral(", distinctiveIdentifier=");
  str.Append(ToCString(aConfig.mDistinctiveIdentifier));

  str.AppendLiteral(", persistentState=");
  str.Append(ToCString(aConfig.mPersistentState));

  str.AppendLiteral(", sessionTypes=");
  str.Append(ToCString(aConfig.mSessionTypes));

  str.AppendLiteral("}");

  return str;
}

static nsCString
RequestKeySystemAccessLogString(const nsAString& aKeySystem,
                                const Sequence<MediaKeySystemConfiguration>& aConfigs)
{
  nsCString str;
  str.AppendPrintf("Navigator::RequestMediaKeySystemAccess(keySystem='%s' options=",
                   NS_ConvertUTF16toUTF8(aKeySystem).get());
  str.Append(ToCString(aConfigs));
  str.AppendLiteral(")");
  return str;
}

already_AddRefed<Promise>
Navigator::RequestMediaKeySystemAccess(const nsAString& aKeySystem,
                                       const Sequence<MediaKeySystemConfiguration>& aConfigs,
                                       ErrorResult& aRv)
{
  EME_LOG("%s", RequestKeySystemAccessLogString(aKeySystem, aConfigs).get());

  nsCOMPtr<nsIGlobalObject> go = do_QueryInterface(mWindow);
  RefPtr<DetailedPromise> promise =
    DetailedPromise::Create(go, aRv,
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
#endif

Presentation*
Navigator::GetPresentation(ErrorResult& aRv)
{
  if (!mPresentation) {
    if (!mWindow) {
      aRv.Throw(NS_ERROR_UNEXPECTED);
      return nullptr;
    }
    mPresentation = Presentation::Create(mWindow);
  }

  return mPresentation;
}

} // namespace dom
} // namespace mozilla
