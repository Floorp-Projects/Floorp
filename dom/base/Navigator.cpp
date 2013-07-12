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
#include "nsICachingChannel.h"
#include "nsIDocShell.h"
#include "nsIWebContentHandlerRegistrar.h"
#include "nsICookiePermission.h"
#include "nsIScriptSecurityManager.h"
#include "nsCharSeparatedTokenizer.h"
#include "nsContentUtils.h"
#include "nsUnicharUtils.h"
#include "nsVariant.h"
#include "mozilla/Preferences.h"
#include "mozilla/Telemetry.h"
#include "BatteryManager.h"
#include "PowerManager.h"
#include "nsIDOMWakeLock.h"
#include "nsIPowerManagerService.h"
#include "mozilla/dom/SmsManager.h"
#include "mozilla/dom/MobileMessageManager.h"
#include "nsISmsService.h"
#include "mozilla/Hal.h"
#include "nsIWebNavigation.h"
#include "nsISiteSpecificUserAgent.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/StaticPtr.h"
#include "Connection.h"
#include "nsDOMClassInfo.h"
#include "nsDOMEvent.h"
#include "nsGlobalWindow.h"
#ifdef MOZ_B2G_RIL
#include "IccManager.h"
#include "MobileConnection.h"
#include "mozilla/dom/CellBroadcast.h"
#include "mozilla/dom/Voicemail.h"
#endif
#include "nsIIdleObserver.h"
#include "nsIPermissionManager.h"
#include "nsNetUtil.h"
#include "nsIHttpChannel.h"
#include "TimeManager.h"

#ifdef MOZ_MEDIA_NAVIGATOR
#include "MediaManager.h"
#endif
#ifdef MOZ_B2G_RIL
#include "TelephonyFactory.h"
#endif
#ifdef MOZ_B2G_BT
#include "nsIDOMBluetoothManager.h"
#include "BluetoothManager.h"
#endif
#include "nsIDOMCameraManager.h"
#include "DOMCameraManager.h"

#ifdef MOZ_AUDIO_CHANNEL_MANAGER
#include "AudioChannelManager.h"
#endif

#include "nsIDOMGlobalPropertyInitializer.h"
#include "nsJSUtils.h"

#include "mozilla/dom/NavigatorBinding.h"

using namespace mozilla::dom::power;

// This should not be in the namespace.
DOMCI_DATA(Navigator, mozilla::dom::Navigator)

namespace mozilla {
namespace dom {

static bool sDoNotTrackEnabled = false;
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
  NS_ASSERTION(aWindow->IsInnerWindow(),
               "Navigator must get an inner window!");
}

Navigator::~Navigator()
{
  Invalidate();
}

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(Navigator)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMNavigator)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNavigator)
  NS_INTERFACE_MAP_ENTRY(nsIDOMClientInformation)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNavigatorDeviceStorage)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNavigatorGeolocation)
  NS_INTERFACE_MAP_ENTRY(nsINavigatorBattery)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNavigatorDesktopNotification)
  NS_INTERFACE_MAP_ENTRY(nsIDOMMozNavigatorSms)
  NS_INTERFACE_MAP_ENTRY(nsIDOMMozNavigatorMobileMessage)
#ifdef MOZ_MEDIA_NAVIGATOR
  NS_INTERFACE_MAP_ENTRY(nsINavigatorUserMedia)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNavigatorUserMedia)
#endif
#ifdef MOZ_B2G_RIL
  NS_INTERFACE_MAP_ENTRY(nsIDOMNavigatorTelephony)
#endif
#ifdef MOZ_GAMEPAD
  NS_INTERFACE_MAP_ENTRY(nsINavigatorGamepads)
#endif
  NS_INTERFACE_MAP_ENTRY(nsIDOMMozNavigatorNetwork)
#ifdef MOZ_B2G_RIL
  NS_INTERFACE_MAP_ENTRY(nsIMozNavigatorMobileConnection)
  NS_INTERFACE_MAP_ENTRY(nsIMozNavigatorCellBroadcast)
  NS_INTERFACE_MAP_ENTRY(nsIMozNavigatorVoicemail)
  NS_INTERFACE_MAP_ENTRY(nsIMozNavigatorIccManager)
#endif
#ifdef MOZ_B2G_BT
  NS_INTERFACE_MAP_ENTRY(nsIDOMNavigatorBluetooth)
#endif
  NS_INTERFACE_MAP_ENTRY(nsIDOMNavigatorCamera)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNavigatorSystemMessages)
#ifdef MOZ_TIME_MANAGER
  NS_INTERFACE_MAP_ENTRY(nsIDOMMozNavigatorTime)
#endif
#ifdef MOZ_AUDIO_CHANNEL_MANAGER
  NS_INTERFACE_MAP_ENTRY(nsIMozNavigatorAudioChannelManager)
#endif
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(Navigator)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(Navigator)
NS_IMPL_CYCLE_COLLECTING_RELEASE(Navigator)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(Navigator)
  tmp->Invalidate();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mWindow)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(Navigator)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPlugins)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mMimeTypes)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mGeolocation)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mNotification)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mBatteryManager)
  // mPowerManager isn't cycle collected
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mSmsManager)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mMobileMessageManager)
#ifdef MOZ_B2G_RIL
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mTelephony)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mVoicemail)
#endif
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mConnection)
#ifdef MOZ_B2G_RIL
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mMobileConnection)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mCellBroadcast)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mIccManager)
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

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mWindow)
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

  if (mPowerManager) {
    mPowerManager->Shutdown();
    mPowerManager = nullptr;
  }

  if (mSmsManager) {
    mSmsManager->Shutdown();
    mSmsManager = nullptr;
  }

  if (mMobileMessageManager) {
    mMobileMessageManager->Shutdown();
    mMobileMessageManager = nullptr;
  }

#ifdef MOZ_B2G_RIL
  if (mTelephony) {
    mTelephony = nullptr;
  }

  if (mVoicemail) {
    mVoicemail = nullptr;
  }
#endif

  if (mConnection) {
    mConnection->Shutdown();
    mConnection = nullptr;
  }

#ifdef MOZ_B2G_RIL
  if (mMobileConnection) {
    mMobileConnection->Shutdown();
    mMobileConnection = nullptr;
  }

  if (mCellBroadcast) {
    mCellBroadcast = nullptr;
  }

  if (mIccManager) {
    mIccManager->Shutdown();
    mIccManager = nullptr;
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
 * JS property navigator.language, exposed to web content.
 * Take first value from Accept-Languages (HTTP header), which is
 * the "content language" freely set by the user in the Pref window.
 *
 * Do not use UI language (chosen app locale) here.
 * See RFC 2616, Section 15.1.4 "Privacy Issues Connected to Accept Headers"
 *
 * "en", "en-US" and "i-cherokee" and "" are valid.
 * Fallback in case of invalid pref should be "" (empty string), to
 * let site do fallback, e.g. to site's local language.
 */
NS_IMETHODIMP
Navigator::GetLanguage(nsAString& aLanguage)
{
  // E.g. "de-de, en-us,en".
  const nsAdoptingString& acceptLang =
    Preferences::GetLocalizedString("intl.accept_languages");

  // Take everything before the first "," or ";", without trailing space.
  nsCharSeparatedTokenizer langTokenizer(acceptLang, ',');
  const nsSubstring &firstLangPart = langTokenizer.nextToken();
  nsCharSeparatedTokenizer qTokenizer(firstLangPart, ';');
  aLanguage.Assign(qTokenizer.nextToken());

  // Checks and fixups:
  // replace "_" with "-" to avoid POSIX/Windows "en_US" notation.
  if (aLanguage.Length() > 2 && aLanguage[2] == PRUnichar('_')) {
    aLanguage.Replace(2, 1, PRUnichar('-')); // TODO replace all
  }

  // Use uppercase for country part, e.g. "en-US", not "en-us", see BCP47
  // only uppercase 2-letter country codes, not "zh-Hant", "de-DE-x-goethe".
  if (aLanguage.Length() <= 2) {
    return NS_OK;
  }

  nsCharSeparatedTokenizer localeTokenizer(aLanguage, '-');
  int32_t pos = 0;
  bool first = true;
  while (localeTokenizer.hasMoreTokens()) {
    const nsSubstring& code = localeTokenizer.nextToken();

    if (code.Length() == 2 && !first) {
      nsAutoString upper(code);
      ToUpperCase(upper);
      aLanguage.Replace(pos, code.Length(), upper);
    }

    pos += code.Length() + 1; // 1 is the separator
    first = false;
  }

  return NS_OK;
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

NS_IMETHODIMP
Navigator::GetMimeTypes(nsISupports** aMimeTypes)
{
  ErrorResult rv;
  NS_IF_ADDREF(*aMimeTypes = GetMimeTypes(rv));
  return rv.ErrorCode();
}

nsMimeTypeArray*
Navigator::GetMimeTypes(ErrorResult& aRv)
{
  if (!mMimeTypes) {
    if (!mWindow) {
      aRv.Throw(NS_ERROR_UNEXPECTED);
      return nullptr;
    }
    nsWeakPtr win = do_GetWeakReference(mWindow);
    mMimeTypes = new nsMimeTypeArray(win);
  }

  return mMimeTypes;
}

NS_IMETHODIMP
Navigator::GetPlugins(nsISupports** aPlugins)
{
  ErrorResult rv;
  NS_IF_ADDREF(*aPlugins = static_cast<nsIObserver*>(GetPlugins(rv)));
  return rv.ErrorCode();
}

nsPluginArray*
Navigator::GetPlugins(ErrorResult& aRv)
{
  if (!mPlugins) {
    if (!mWindow) {
      aRv.Throw(NS_ERROR_UNEXPECTED);
      return nullptr;
    }
    nsWeakPtr win = do_GetWeakReference(mWindow);
    mPlugins = new nsPluginArray(win);
    mPlugins->Init();
  }

  return mPlugins;
}

// Values for the network.cookie.cookieBehavior pref are documented in
// nsCookieService.cpp.
#define COOKIE_BEHAVIOR_REJECT 2

NS_IMETHODIMP
Navigator::GetCookieEnabled(bool* aCookieEnabled)
{
  *aCookieEnabled = CookieEnabled();
  return NS_OK;
}

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

NS_IMETHODIMP
Navigator::GetOnLine(bool* aOnline)
{
  NS_PRECONDITION(aOnline, "Null out param");

  *aOnline = OnLine();
  return NS_OK;
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
    aResult.AssignLiteral("yes");
  } else {
    aResult.AssignLiteral("unspecified");
  }

  return NS_OK;
}

NS_IMETHODIMP
Navigator::JavaEnabled(bool* aReturn)
{
  ErrorResult rv;
  *aReturn = JavaEnabled(rv);
  return rv.ErrorCode();
}

bool
Navigator::JavaEnabled(ErrorResult& aRv)
{
  Telemetry::AutoTimer<Telemetry::CHECK_JAVA_ENABLED> telemetryTimer;
  // Return true if we have a handler for "application/x-java-vm",
  // otherwise return false.
  if (!mMimeTypes) {
    if (!mWindow) {
      aRv.Throw(NS_ERROR_UNEXPECTED);
      return false;
    }
    nsWeakPtr win = do_GetWeakReference(mWindow);
    mMimeTypes = new nsMimeTypeArray(win);
  }

  RefreshMIMEArray();

  nsMimeType *mimeType =
    mMimeTypes->NamedItem(NS_LITERAL_STRING("application/x-java-vm"));

  return mimeType && mimeType->GetEnabledPlugin();
}

NS_IMETHODIMP
Navigator::TaintEnabled(bool *aReturn)
{
  *aReturn = TaintEnabled();
  return NS_OK;
}

void
Navigator::RefreshMIMEArray()
{
  if (mMimeTypes) {
    mMimeTypes->Refresh();
  }
}

bool
Navigator::HasDesktopNotificationSupport()
{
  return Preferences::GetBool("notification.feature.enabled", false);
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

  virtual ~VibrateWindowListener()
  {
  }

  void RemoveListener();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMEVENTLISTENER

private:
  nsWeakPtr mWindow;
  nsWeakPtr mDocument;
};

NS_IMPL_ISUPPORTS1(VibrateWindowListener, nsIDOMEventListener)

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
    gVibrateWindowListener = NULL;
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

/**
 * Converts a JS::Value into a vibration duration, checking that the duration is in
 * bounds (non-negative and not larger than sMaxVibrateMS).
 *
 * Returns true on success, false on failure.
 */
bool
GetVibrationDurationFromJsval(const JS::Value& aJSVal, JSContext* cx,
                              int32_t* aOut)
{
  return JS_ValueToInt32(cx, aJSVal, aOut) &&
         *aOut >= 0 && static_cast<uint32_t>(*aOut) <= sMaxVibrateMS;
}

} // anonymous namespace

NS_IMETHODIMP
Navigator::AddIdleObserver(nsIIdleObserver* aIdleObserver)
{
  NS_ENSURE_STATE(mWindow);

  if (!nsContentUtils::IsIdleObserverAPIEnabled()) {
    NS_WARNING("The IdleObserver API has been disabled.");
    return NS_OK;
  }

  NS_ENSURE_ARG_POINTER(aIdleObserver);

  if (!CheckPermission("idle")) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  AddIdleObserver(*aIdleObserver);
  return NS_OK;
}

void
Navigator::AddIdleObserver(nsIIdleObserver& aIdleObserver)
{
  // Callers (either the XPCOM method or the WebIDL binding) are responsible for
  // the permission check here.
  if (NS_FAILED(mWindow->RegisterIdleObserver(&aIdleObserver))) {
    NS_WARNING("Failed to add idle observer.");
  }
}

void
Navigator::AddIdleObserver(MozIdleObserver& aIdleObserver, ErrorResult& aRv)
{
  if (!mWindow) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return;
  }
  CallbackObjectHolder<MozIdleObserver, nsIIdleObserver> holder(&aIdleObserver);
  nsCOMPtr<nsIIdleObserver> obs = holder.ToXPCOMCallback();
  return AddIdleObserver(*obs);
}

NS_IMETHODIMP
Navigator::RemoveIdleObserver(nsIIdleObserver* aIdleObserver)
{
  NS_ENSURE_STATE(mWindow);

  if (!nsContentUtils::IsIdleObserverAPIEnabled()) {
    NS_WARNING("The IdleObserver API has been disabled");
    return NS_OK;
  }

  NS_ENSURE_ARG_POINTER(aIdleObserver);

  RemoveIdleObserver(*aIdleObserver);
  return NS_OK;
}

void
Navigator::RemoveIdleObserver(nsIIdleObserver& aIdleObserver)
{
  // Callers (either the XPCOM method or the WebIDL binding) are responsible for
  // the permission check here.
  if (NS_FAILED(mWindow->UnregisterIdleObserver(&aIdleObserver))) {
    NS_WARNING("Failed to remove idle observer.");
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
  return RemoveIdleObserver(*obs);
}

NS_IMETHODIMP
Navigator::Vibrate(const JS::Value& aPattern, JSContext* cx)
{
  nsAutoTArray<uint32_t, 8> pattern;

  // null or undefined pattern is an error.
  if (JSVAL_IS_NULL(aPattern) || JSVAL_IS_VOID(aPattern)) {
    return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
  }

  if (JSVAL_IS_PRIMITIVE(aPattern)) {
    int32_t p;
    if (GetVibrationDurationFromJsval(aPattern, cx, &p)) {
      pattern.AppendElement(p);
    }
    else {
      return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
    }
  }
  else {
    JS::Rooted<JSObject*> obj(cx, aPattern.toObjectOrNull());
    uint32_t length;
    if (!JS_GetArrayLength(cx, obj, &length) || length > sMaxVibrateListLen) {
      return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
    }
    pattern.SetLength(length);

    for (uint32_t i = 0; i < length; ++i) {
      JS::Rooted<JS::Value> v(cx);
      int32_t pv;
      if (JS_GetElement(cx, obj, i, v.address()) &&
          GetVibrationDurationFromJsval(v, cx, &pv)) {
        pattern[i] = pv;
      }
      else {
        return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
      }
    }
  }

  ErrorResult rv;
  Vibrate(pattern, rv);
  return rv.ErrorCode();
}

void
Navigator::Vibrate(uint32_t aDuration, ErrorResult& aRv)
{
  nsAutoTArray<uint32_t, 1> pattern;
  pattern.AppendElement(aDuration);
  Vibrate(pattern, aRv);
}

void
Navigator::Vibrate(const nsTArray<uint32_t>& aPattern, ErrorResult& aRv)
{
  if (!mWindow) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return;
  }
  nsCOMPtr<nsIDocument> doc = mWindow->GetExtantDoc();
  if (!doc) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }
  if (doc->Hidden()) {
    // Hidden documents cannot start or stop a vibration.
    return;
  }

  if (aPattern.Length() > sMaxVibrateListLen) {
    // XXXbz this should be returning false instead
    aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
    return;
  }

  for (size_t i = 0; i < aPattern.Length(); ++i) {
    if (aPattern[i] > sMaxVibrateMS) {
      // XXXbz this should be returning false instead
      aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
      return;
    }
  }

  // The spec says we check sVibratorEnabled after we've done the sanity
  // checking on the pattern.
  if (!sVibratorEnabled) {
    return;
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

  hal::Vibrate(aPattern, mWindow);
}

//*****************************************************************************
//    Navigator::nsIDOMClientInformation
//*****************************************************************************

NS_IMETHODIMP
Navigator::RegisterContentHandler(const nsAString& aMIMEType,
                                  const nsAString& aURI,
                                  const nsAString& aTitle)
{
  if (!mWindow || !mWindow->GetOuterWindow() || !mWindow->GetDocShell()) {
    return NS_OK;
  }

  nsCOMPtr<nsIWebContentHandlerRegistrar> registrar =
    do_GetService(NS_WEBCONTENTHANDLERREGISTRAR_CONTRACTID);
  if (!registrar) {
    return NS_OK;
  }

  return registrar->RegisterContentHandler(aMIMEType, aURI, aTitle,
                                           mWindow->GetOuterWindow());
}

NS_IMETHODIMP
Navigator::RegisterProtocolHandler(const nsAString& aProtocol,
                                   const nsAString& aURI,
                                   const nsAString& aTitle)
{
  if (!mWindow || !mWindow->GetOuterWindow() || !mWindow->GetDocShell()) {
    return NS_OK;
  }

  nsCOMPtr<nsIWebContentHandlerRegistrar> registrar =
    do_GetService(NS_WEBCONTENTHANDLERREGISTRAR_CONTRACTID);
  if (!registrar) {
    return NS_OK;
  }

  return registrar->RegisterProtocolHandler(aProtocol, aURI, aTitle,
                                            mWindow->GetOuterWindow());
}

NS_IMETHODIMP
Navigator::MozIsLocallyAvailable(const nsAString &aURI,
                                 bool aWhenOffline,
                                 bool* aIsAvailable)
{
  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), aURI);
  NS_ENSURE_SUCCESS(rv, rv);

  // This method of checking the cache will only work for http/https urls.
  bool match;
  rv = uri->SchemeIs("http", &match);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!match) {
    rv = uri->SchemeIs("https", &match);
    NS_ENSURE_SUCCESS(rv, rv);
    if (!match) {
      return NS_ERROR_DOM_BAD_URI;
    }
  }

  // Same origin check.
  JSContext *cx = nsContentUtils::GetCurrentJSContext();
  NS_ENSURE_TRUE(cx, NS_ERROR_FAILURE);

  rv = nsContentUtils::GetSecurityManager()->CheckSameOrigin(cx, uri);
  NS_ENSURE_SUCCESS(rv, rv);

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

  NS_ENSURE_STATE(mWindow);

  nsCOMPtr<nsILoadGroup> loadGroup;
  nsCOMPtr<nsIDocument> doc = mWindow->GetDoc();
  if (doc) {
    loadGroup = doc->GetDocumentLoadGroup();
  }

  nsCOMPtr<nsIChannel> channel;
  rv = NS_NewChannel(getter_AddRefs(channel), uri,
                     nullptr, loadGroup, nullptr, loadFlags);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIInputStream> stream;
  rv = channel->Open(getter_AddRefs(stream));
  NS_ENSURE_SUCCESS(rv, rv);

  stream->Close();

  nsresult status;
  rv = channel->GetStatus(&status);
  NS_ENSURE_SUCCESS(rv, rv);

  if (NS_FAILED(status)) {
    *aIsAvailable = false;
    return NS_OK;
  }

  nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(channel);
  return httpChannel->GetRequestSucceeded(aIsAvailable);
}

//*****************************************************************************
//    Navigator::nsIDOMNavigatorDeviceStorage
//*****************************************************************************

NS_IMETHODIMP Navigator::GetDeviceStorage(const nsAString &aType, nsIDOMDeviceStorage** _retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = nullptr;

  if (!Preferences::GetBool("device.storage.enabled", false)) {
    return NS_OK;
  }

  if (!mWindow || !mWindow->GetOuterWindow() || !mWindow->GetDocShell()) {
    return NS_ERROR_FAILURE;
  }

  nsRefPtr<nsDOMDeviceStorage> storage;
  nsDOMDeviceStorage::CreateDeviceStorageFor(mWindow, aType,
                                             getter_AddRefs(storage));

  if (!storage) {
    return NS_OK;
  }

  NS_ADDREF(*_retval = storage.get());
  mDeviceStorageStores.AppendElement(storage);
  return NS_OK;
}

NS_IMETHODIMP Navigator::GetDeviceStorages(const nsAString &aType, nsIVariant** _retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = nullptr;

  if (!Preferences::GetBool("device.storage.enabled", false)) {
    return NS_OK;
  }

  if (!mWindow || !mWindow->GetOuterWindow() || !mWindow->GetDocShell()) {
    return NS_ERROR_FAILURE;
  }

  nsTArray<nsRefPtr<nsDOMDeviceStorage> > stores;
  nsDOMDeviceStorage::CreateDeviceStoragesFor(mWindow, aType, stores, false);

  nsCOMPtr<nsIWritableVariant> result = do_CreateInstance("@mozilla.org/variant;1");
  NS_ENSURE_TRUE(result, NS_ERROR_FAILURE);

  if (stores.Length() == 0) {
    result->SetAsEmptyArray();
  } else {
    result->SetAsArray(nsIDataType::VTYPE_INTERFACE,
                       &NS_GET_IID(nsIDOMDeviceStorage),
                       stores.Length(),
                       const_cast<void*>(static_cast<const void*>(stores.Elements())));
  }
  result.forget(_retval);

  mDeviceStorageStores.AppendElements(stores);
  return NS_OK;
}

//*****************************************************************************
//    Navigator::nsIDOMNavigatorGeolocation
//*****************************************************************************

NS_IMETHODIMP Navigator::GetGeolocation(nsIDOMGeoGeolocation** _retval)
{
  ErrorResult rv;
  NS_IF_ADDREF(*_retval = GetGeolocation(rv));
  return rv.ErrorCode();
}

Geolocation*
Navigator::GetGeolocation(ErrorResult& aRv)
{
  if (!Preferences::GetBool("geo.enabled", true)) {
    return nullptr;
  }

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

//*****************************************************************************
//    Navigator::nsIDOMNavigatorUserMedia (mozGetUserMedia)
//*****************************************************************************
#ifdef MOZ_MEDIA_NAVIGATOR
NS_IMETHODIMP
Navigator::MozGetUserMedia(nsIMediaStreamOptions* aParams,
                           nsIDOMGetUserMediaSuccessCallback* aOnSuccess,
                           nsIDOMGetUserMediaErrorCallback* aOnError)
{
  // Make enabling peerconnection enable getUserMedia() as well
  if (!(Preferences::GetBool("media.navigator.enabled", false) ||
        Preferences::GetBool("media.peerconnection.enabled", false))) {
    return NS_OK;
  }

  if (!mWindow || !mWindow->GetOuterWindow() ||
      mWindow->GetOuterWindow()->GetCurrentInnerWindow() != mWindow) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  bool privileged = nsContentUtils::IsChromeDoc(mWindow->GetExtantDoc());

  MediaManager* manager = MediaManager::Get();
  return manager->GetUserMedia(privileged, mWindow, aParams, aOnSuccess,
                               aOnError);
}

//*****************************************************************************
//    Navigator::nsINavigatorUserMedia (mozGetUserMediaDevices)
//*****************************************************************************
NS_IMETHODIMP
Navigator::MozGetUserMediaDevices(nsIGetUserMediaDevicesSuccessCallback* aOnSuccess,
                                  nsIDOMGetUserMediaErrorCallback* aOnError)
{
  if (!mWindow || !mWindow->GetOuterWindow() ||
      mWindow->GetOuterWindow()->GetCurrentInnerWindow() != mWindow) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  // Check if the caller is chrome privileged, bail if not
  if (!nsContentUtils::IsCallerChrome()) {
    return NS_ERROR_FAILURE;
  }

  MediaManager* manager = MediaManager::Get();
  return manager->GetUserMediaDevices(mWindow, aOnSuccess, aOnError);
}
#endif

//*****************************************************************************
//    Navigator::nsIDOMNavigatorDesktopNotification
//*****************************************************************************

NS_IMETHODIMP Navigator::GetMozNotification(nsISupports** aRetVal)
{
  NS_ENSURE_ARG_POINTER(aRetVal);
  *aRetVal = nullptr;

  if (mNotification) {
    NS_ADDREF(*aRetVal = mNotification);
    return NS_OK;
  }

  NS_ENSURE_TRUE(mWindow && mWindow->GetDocShell(), NS_ERROR_FAILURE);

  mNotification = new DesktopNotificationCenter(mWindow);

  NS_ADDREF(*aRetVal = mNotification);
  return NS_OK;
}

//*****************************************************************************
//    Navigator::nsINavigatorBattery
//*****************************************************************************

NS_IMETHODIMP
Navigator::GetBattery(nsISupports** aBattery)
{
  ErrorResult rv;
  NS_IF_ADDREF(*aBattery = GetBattery(rv));
  return rv.ErrorCode();
}

battery::BatteryManager*
Navigator::GetBattery(ErrorResult& aRv)
{
  if (!mBatteryManager) {
    if (!mWindow) {
      aRv.Throw(NS_ERROR_UNEXPECTED);
      return nullptr;
    }
    NS_ENSURE_TRUE(mWindow->GetDocShell(), nullptr);

    mBatteryManager = new battery::BatteryManager();
    mBatteryManager->Init(mWindow);
  }

  return mBatteryManager;
}

NS_IMETHODIMP
Navigator::GetMozPower(nsIDOMMozPowerManager** aPower)
{
  if (!PowerManager::CheckPermission(mWindow)) {
    *aPower = nullptr;
    return NS_OK;
  }
  ErrorResult rv;
  NS_IF_ADDREF(*aPower = GetMozPower(rv));
  return rv.ErrorCode();
}

nsIDOMMozPowerManager*
Navigator::GetMozPower(ErrorResult& aRv)
{
  // Callers (either the XPCOM method or the WebIDL binding) are responsible for
  // the permission check here.
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

NS_IMETHODIMP
Navigator::RequestWakeLock(const nsAString &aTopic, nsIDOMMozWakeLock **aWakeLock)
{
  ErrorResult rv;
  *aWakeLock = RequestWakeLock(aTopic, rv).get();
  return rv.ErrorCode();
}

already_AddRefed<nsIDOMMozWakeLock>
Navigator::RequestWakeLock(const nsAString &aTopic, ErrorResult& aRv)
{
  if (!mWindow) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  nsCOMPtr<nsIPowerManagerService> pmService =
    do_GetService(POWERMANAGERSERVICE_CONTRACTID);
  // Maybe it went away for some reason... Or maybe we're just called
  // from our XPCOM method.
  NS_ENSURE_TRUE(pmService, nullptr);

  nsCOMPtr<nsIDOMMozWakeLock> wakelock;
  aRv = pmService->NewWakeLock(aTopic, mWindow, getter_AddRefs(wakelock));
  return wakelock.forget();
}

//*****************************************************************************
//    Navigator::nsIDOMNavigatorSms
//*****************************************************************************

NS_IMETHODIMP
Navigator::GetMozSms(nsIDOMMozSmsManager** aSmsManager)
{
  *aSmsManager = nullptr;

  if (!mSmsManager) {
    NS_ENSURE_STATE(mWindow);
    NS_ENSURE_TRUE(mWindow->GetDocShell(), NS_OK);

    mSmsManager = SmsManager::CreateInstanceIfAllowed(mWindow);
    NS_ENSURE_TRUE(mSmsManager, NS_OK);
  }

  NS_ADDREF(*aSmsManager = mSmsManager);

  return NS_OK;
}

//*****************************************************************************
//    Navigator::nsIDOMNavigatorMobileMessage
//*****************************************************************************

NS_IMETHODIMP
Navigator::GetMozMobileMessage(nsIDOMMozMobileMessageManager** aMobileMessageManager)
{
  *aMobileMessageManager = nullptr;

#ifndef MOZ_WEBSMS_BACKEND
  return NS_OK;
#endif

  // First of all, the general pref has to be turned on.
  bool enabled = false;
  Preferences::GetBool("dom.sms.enabled", &enabled);
  NS_ENSURE_TRUE(enabled, NS_OK);

  if (!mMobileMessageManager) {
    NS_ENSURE_STATE(mWindow);
    NS_ENSURE_TRUE(mWindow->GetDocShell(), NS_OK);

    if (!CheckPermission("sms")) {
      return NS_OK;
    }

    mMobileMessageManager = new MobileMessageManager();
    mMobileMessageManager->Init(mWindow);
  }

  NS_ADDREF(*aMobileMessageManager = mMobileMessageManager);

  return NS_OK;
}

#ifdef MOZ_B2G_RIL

//*****************************************************************************
//    Navigator::nsIMozNavigatorCellBroadcast
//*****************************************************************************

NS_IMETHODIMP
Navigator::GetMozCellBroadcast(nsIDOMMozCellBroadcast** aCellBroadcast)
{
  *aCellBroadcast = nullptr;

  if (!mCellBroadcast) {
    NS_ENSURE_STATE(mWindow);

    if (!CheckPermission("cellbroadcast")) {
      return NS_OK;
    }

    nsresult rv = NS_NewCellBroadcast(mWindow, getter_AddRefs(mCellBroadcast));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  NS_ADDREF(*aCellBroadcast = mCellBroadcast);
  return NS_OK;
}

//*****************************************************************************
//    nsNavigator::nsIDOMNavigatorTelephony
//*****************************************************************************

NS_IMETHODIMP
Navigator::GetMozTelephony(nsIDOMTelephony** aTelephony)
{
  nsCOMPtr<nsIDOMTelephony> telephony = mTelephony;

  if (!telephony) {
    NS_ENSURE_STATE(mWindow);
    nsresult rv = NS_NewTelephony(mWindow, getter_AddRefs(mTelephony));
    NS_ENSURE_SUCCESS(rv, rv);

    // mTelephony may be null here!
    telephony = mTelephony;
  }

  telephony.forget(aTelephony);
  return NS_OK;
}

//*****************************************************************************
//    nsNavigator::nsINavigatorVoicemail
//*****************************************************************************

NS_IMETHODIMP
Navigator::GetMozVoicemail(nsIDOMMozVoicemail** aVoicemail)
{
  *aVoicemail = nullptr;

  if (!mVoicemail) {
    NS_ENSURE_STATE(mWindow);
    if (!CheckPermission("voicemail")) {
      return NS_OK;
    }

    nsresult rv = NS_NewVoicemail(mWindow, getter_AddRefs(mVoicemail));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  NS_ADDREF(*aVoicemail = mVoicemail);
  return NS_OK;
}

//*****************************************************************************
//    nsNavigator::nsIMozNavigatorIccManager
//*****************************************************************************

NS_IMETHODIMP
Navigator::GetMozIccManager(nsIDOMMozIccManager** aIccManager)
{
  *aIccManager = nullptr;

  if (!mIccManager) {
    NS_ENSURE_STATE(mWindow);
    NS_ENSURE_TRUE(mWindow->GetDocShell(), NS_OK);

    if (!CheckPermission("mobileconnection")) {
      return NS_OK;
    }

    mIccManager = new icc::IccManager();
    mIccManager->Init(mWindow);
  }

  NS_ADDREF(*aIccManager = mIccManager);
  return NS_OK;
}

#endif // MOZ_B2G_RIL

#ifdef MOZ_GAMEPAD
//*****************************************************************************
//    Navigator::nsINavigatorGamepads
//*****************************************************************************

NS_IMETHODIMP
Navigator::GetGamepads(nsIVariant** aRetVal)
{
  NS_ENSURE_ARG_POINTER(aRetVal);
  *aRetVal = nullptr;

  NS_ENSURE_STATE(mWindow);
  NS_ENSURE_TRUE(mWindow->GetDocShell(), NS_OK);
  nsGlobalWindow* win = static_cast<nsGlobalWindow*>(mWindow.get());

  nsAutoTArray<nsRefPtr<Gamepad>, 2> gamepads;
  win->GetGamepads(gamepads);

  nsRefPtr<nsVariant> out = new nsVariant();
  NS_ENSURE_STATE(out);

  if (gamepads.Length() == 0) {
    nsresult rv = out->SetAsEmptyArray();
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    out->SetAsArray(nsIDataType::VTYPE_INTERFACE,
                    &NS_GET_IID(nsISupports),
                    gamepads.Length(),
                    const_cast<void*>(static_cast<const void*>(gamepads.Elements())));
  }
  out.forget(aRetVal);

  return NS_OK;
}
#endif

//*****************************************************************************
//    Navigator::nsIDOMNavigatorNetwork
//*****************************************************************************

NS_IMETHODIMP
Navigator::GetMozConnection(nsIDOMMozConnection** aConnection)
{
  *aConnection = nullptr;

  if (!mConnection) {
    NS_ENSURE_STATE(mWindow);
    NS_ENSURE_TRUE(mWindow->GetDocShell(), NS_OK);

    mConnection = new network::Connection();
    mConnection->Init(mWindow);
  }

  NS_ADDREF(*aConnection = mConnection);
  return NS_OK;
}

#ifdef MOZ_B2G_RIL
//*****************************************************************************
//    Navigator::nsINavigatorMobileConnection
//*****************************************************************************
NS_IMETHODIMP
Navigator::GetMozMobileConnection(nsIDOMMozMobileConnection** aMobileConnection)
{
  *aMobileConnection = nullptr;

  if (!mMobileConnection) {
    NS_ENSURE_STATE(mWindow);
    if (!CheckPermission("mobileconnection") &&
        !CheckPermission("mobilenetwork")) {
      return NS_OK;
    }

    mMobileConnection = new network::MobileConnection();
    mMobileConnection->Init(mWindow);
  }

  NS_ADDREF(*aMobileConnection = mMobileConnection);
  return NS_OK;
}
#endif // MOZ_B2G_RIL

#ifdef MOZ_B2G_BT
//*****************************************************************************
//    nsNavigator::nsIDOMNavigatorBluetooth
//*****************************************************************************

NS_IMETHODIMP
Navigator::GetMozBluetooth(nsIDOMBluetoothManager** aBluetooth)
{
  nsCOMPtr<nsIDOMBluetoothManager> bluetooth = mBluetooth;

  if (!bluetooth) {
    NS_ENSURE_STATE(mWindow);
    nsresult rv = NS_NewBluetoothManager(mWindow, getter_AddRefs(mBluetooth));
    NS_ENSURE_SUCCESS(rv, rv);

    bluetooth = mBluetooth;
  }

  bluetooth.forget(aBluetooth);
  return NS_OK;
}
#endif //MOZ_B2G_BT

//*****************************************************************************
//    nsNavigator::nsIDOMNavigatorSystemMessages
//*****************************************************************************
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
  rv = gpi->Init(mWindow, prop_val.address());
  NS_ENSURE_SUCCESS(rv, rv);

  mMessagesManager = messageManager.forget();

  return NS_OK;
}

NS_IMETHODIMP
Navigator::MozHasPendingMessage(const nsAString& aType, bool *aResult)
{
  if (!Preferences::GetBool("dom.sysmsg.enabled", false)) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  *aResult = false;
  nsresult rv = EnsureMessagesManager();
  NS_ENSURE_SUCCESS(rv, rv);

  return mMessagesManager->MozHasPendingMessage(aType, aResult);
}

NS_IMETHODIMP
Navigator::MozSetMessageHandler(const nsAString& aType,
                                nsIDOMSystemMessageCallback *aCallback)
{
  if (!Preferences::GetBool("dom.sysmsg.enabled", false)) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  nsresult rv = EnsureMessagesManager();
  NS_ENSURE_SUCCESS(rv, rv);

  return mMessagesManager->MozSetMessageHandler(aType, aCallback);
}

//*****************************************************************************
//    Navigator::nsIDOMNavigatorTime
//*****************************************************************************
#ifdef MOZ_TIME_MANAGER
NS_IMETHODIMP
Navigator::GetMozTime(nsISupports** aTime)
{
  *aTime = nullptr;

  NS_ENSURE_STATE(mWindow);
  if (!CheckPermission("time")) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  if (!mTimeManager) {
    mTimeManager = new time::TimeManager(mWindow);
  }

  NS_ADDREF(*aTime = mTimeManager);
  return NS_OK;
}
#endif

//*****************************************************************************
//    nsNavigator::nsIDOMNavigatorCamera
//*****************************************************************************

NS_IMETHODIMP
Navigator::GetMozCameras(nsISupports** aCameraManager)
{
  if (!mCameraManager) {
    if (!mWindow ||
        !mWindow->GetOuterWindow() ||
        mWindow->GetOuterWindow()->GetCurrentInnerWindow() != mWindow) {
      return NS_ERROR_NOT_AVAILABLE;
    }

    mCameraManager =
      nsDOMCameraManager::CheckPermissionAndCreateInstance(mWindow);
    NS_ENSURE_TRUE(mCameraManager, NS_OK);
  }

  nsCOMPtr<nsIObserver> cameraManager = mCameraManager.get();
  cameraManager.forget(aCameraManager);

  return NS_OK;
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
    do_GetService(NS_PERMISSIONMANAGER_CONTRACTID);
  NS_ENSURE_TRUE(permMgr, false);

  uint32_t permission = nsIPermissionManager::DENY_ACTION;
  permMgr->TestPermissionFromWindow(aWindow, aType, &permission);
  return permission == nsIPermissionManager::ALLOW_ACTION;
}

//*****************************************************************************
//    Navigator::nsINavigatorAudioChannelManager
//*****************************************************************************
#ifdef MOZ_AUDIO_CHANNEL_MANAGER
NS_IMETHODIMP
Navigator::GetMozAudioChannelManager(nsISupports** aAudioChannelManager)
{
  *aAudioChannelManager = nullptr;

  if (!mAudioChannelManager) {
    NS_ENSURE_STATE(mWindow);
    mAudioChannelManager = new system::AudioChannelManager();
    mAudioChannelManager->Init(mWindow);
  }

  NS_ADDREF(*aAudioChannelManager = mAudioChannelManager);
  return NS_OK;
}
#endif

/* static */
bool
Navigator::HasBatterySupport(JSContext* /* unused*/, JSObject* /*unused */)
{
  return battery::BatteryManager::HasSupport();
}

/* static */
bool
Navigator::HasPowerSupport(JSContext* /* unused */, JSObject* aGlobal)
{
  nsCOMPtr<nsPIDOMWindow> win = GetWindowFromGlobal(aGlobal);
  return win && PowerManager::CheckPermission(win);
}

/* static */
bool
Navigator::HasIdleSupport(JSContext*  /* unused */, JSObject* aGlobal)
{
  if (!nsContentUtils::IsIdleObserverAPIEnabled()) {
    return false;
  }

  nsCOMPtr<nsPIDOMWindow> win = GetWindowFromGlobal(aGlobal);
  return CheckPermission(win, "idle");
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
#elif defined(XP_OS2)
  aPlatform.AssignLiteral("OS/2");
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
  aAppVersion.Append(PRUnichar(')'));

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
    }
  }

  aAppName.AssignLiteral("Netscape");
}
