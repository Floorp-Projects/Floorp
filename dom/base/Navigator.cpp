/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=2 et tw=78: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Travis Bogard <travis@netscape.com>
 *   Brendan Eich <brendan@mozilla.org>
 *   David Hyatt (hyatt@netscape.com)
 *   Dan Rosen <dr@netscape.com>
 *   Vidur Apparao <vidur@netscape.com>
 *   Johnny Stenback <jst@netscape.com>
 *   Mark Hammond <mhammond@skippinet.com.au>
 *   Ryan Jones <sciguyryan@gmail.com>
 *   Jeff Walden <jwalden+code@mit.edu>
 *   Ben Bucksch <ben.bucksch  beonex.com>
 *   Emanuele Costa <emanuele.costa@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

// Needs to be first.
#include "base/basictypes.h"

#include "Navigator.h"
#include "nsIXULAppInfo.h"
#include "nsPluginArray.h"
#include "nsMimeTypeArray.h"
#include "nsDesktopNotification.h"
#include "nsGeolocation.h"
#include "nsIHttpProtocolHandler.h"
#include "nsICachingChannel.h"
#include "nsIDocShell.h"
#include "nsIWebContentHandlerRegistrar.h"
#include "nsICookiePermission.h"
#include "nsIScriptSecurityManager.h"
#include "nsIJSContextStack.h"
#include "nsCharSeparatedTokenizer.h"
#include "nsContentUtils.h"
#include "nsUnicharUtils.h"
#include "mozilla/Preferences.h"
#include "mozilla/Telemetry.h"
#include "BatteryManager.h"
#include "PowerManager.h"
#include "nsIDOMWakeLock.h"
#include "nsIPowerManagerService.h"
#include "SmsManager.h"
#include "nsISmsService.h"
#include "mozilla/Hal.h"
#include "nsIWebNavigation.h"
#include "mozilla/ClearOnShutdown.h"
#include "Connection.h"

#ifdef MOZ_B2G_RIL
#include "TelephonyFactory.h"
#endif
#ifdef MOZ_B2G_BT
#include "nsIDOMBluetoothAdapter.h"
#include "BluetoothAdapter.h"
#endif

// This should not be in the namespace.
DOMCI_DATA(Navigator, mozilla::dom::Navigator)

namespace mozilla {
namespace dom {

static const char sJSStackContractID[] = "@mozilla.org/js/xpc/ContextStack;1";

static bool sDoNotTrackEnabled = false;
static bool sVibratorEnabled   = false;
static PRUint32 sMaxVibrateMS  = 0;
static PRUint32 sMaxVibrateListLen = 0;

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
  : mWindow(do_GetWeakReference(aWindow))
{
  NS_ASSERTION(aWindow->IsInnerWindow(),
               "Navigator must get an inner window!");
}

Navigator::~Navigator()
{
  Invalidate();
}

NS_INTERFACE_MAP_BEGIN(Navigator)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMNavigator)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNavigator)
  NS_INTERFACE_MAP_ENTRY(nsIDOMClientInformation)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNavigatorGeolocation)
  NS_INTERFACE_MAP_ENTRY(nsIDOMMozNavigatorBattery)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNavigatorDesktopNotification)
  NS_INTERFACE_MAP_ENTRY(nsIDOMMozNavigatorSms)
#ifdef MOZ_B2G_RIL
  NS_INTERFACE_MAP_ENTRY(nsIDOMNavigatorTelephony)
#endif
  NS_INTERFACE_MAP_ENTRY(nsIDOMMozNavigatorNetwork)
#ifdef MOZ_B2G_BT
  NS_INTERFACE_MAP_ENTRY(nsIDOMNavigatorBluetooth)
#endif
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(Navigator)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(Navigator)
NS_IMPL_RELEASE(Navigator)

void
Navigator::Invalidate()
{
  mWindow = nsnull;

  if (mPlugins) {
    mPlugins->Invalidate();
    mPlugins = nsnull;
  }

  // If there is a page transition, make sure delete the geolocation object.
  if (mGeolocation) {
    mGeolocation->Shutdown();
    mGeolocation = nsnull;
  }

  if (mNotification) {
    mNotification->Shutdown();
    mNotification = nsnull;
  }

  if (mBatteryManager) {
    mBatteryManager->Shutdown();
    mBatteryManager = nsnull;
  }

  if (mPowerManager) {
    mPowerManager->Shutdown();
    mPowerManager = nsnull;
  }

  if (mSmsManager) {
    mSmsManager->Shutdown();
    mSmsManager = nsnull;
  }

#ifdef MOZ_B2G_RIL
  if (mTelephony) {
    mTelephony = nsnull;
  }
#endif

  if (mConnection) {
    mConnection->Shutdown();
    mConnection = nsnull;
  }

#ifdef MOZ_B2G_BT
  if (mBluetooth) {
    mBluetooth = nsnull;
  }
#endif
}

nsPIDOMWindow *
Navigator::GetWindow()
{
  nsCOMPtr<nsPIDOMWindow> win(do_QueryReferent(mWindow));

  return win;
}


//*****************************************************************************
//    Navigator::nsIDOMNavigator
//*****************************************************************************

NS_IMETHODIMP
Navigator::GetUserAgent(nsAString& aUserAgent)
{
  return NS_GetNavigatorUserAgent(aUserAgent);
}

NS_IMETHODIMP
Navigator::GetAppCodeName(nsAString& aAppCodeName)
{
  nsresult rv;

  nsCOMPtr<nsIHttpProtocolHandler>
    service(do_GetService(NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "http", &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCAutoString appName;
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
  return NS_GetNavigatorAppName(aAppName);
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
  PRInt32 pos = 0;
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
  if (!nsContentUtils::IsCallerTrustedForRead()) {
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

  nsCAutoString oscpu;
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
  nsresult rv;

  nsCOMPtr<nsIHttpProtocolHandler>
    service(do_GetService(NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "http", &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCAutoString product;
  rv = service->GetProduct(product);
  CopyASCIItoUTF16(product, aProduct);

  return rv;
}

NS_IMETHODIMP
Navigator::GetProductSub(nsAString& aProductSub)
{
  if (!nsContentUtils::IsCallerTrustedForRead()) {
    const nsAdoptingString& override =
      Preferences::GetString("general.productSub.override");

    if (override) {
      aProductSub = override;
      return NS_OK;
    }

    // 'general.useragent.productSub' backwards compatible with 1.8 branch.
    const nsAdoptingString& override2 =
      Preferences::GetString("general.useragent.productSub");

    if (override2) {
      aProductSub = override2;
      return NS_OK;
    }
  }

  nsresult rv;

  nsCOMPtr<nsIHttpProtocolHandler>
    service(do_GetService(NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "http", &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCAutoString productSub;
  rv = service->GetProductSub(productSub);
  CopyASCIItoUTF16(productSub, aProductSub);

  return rv;
}

NS_IMETHODIMP
Navigator::GetMimeTypes(nsIDOMMimeTypeArray** aMimeTypes)
{
  if (!mMimeTypes) {
    mMimeTypes = new nsMimeTypeArray(this);
  }

  NS_ADDREF(*aMimeTypes = mMimeTypes);

  return NS_OK;
}

NS_IMETHODIMP
Navigator::GetPlugins(nsIDOMPluginArray** aPlugins)
{
  if (!mPlugins) {
    nsCOMPtr<nsPIDOMWindow> win(do_QueryReferent(mWindow));

    mPlugins = new nsPluginArray(this, win ? win->GetDocShell() : nsnull);
  }

  NS_ADDREF(*aPlugins = mPlugins);

  return NS_OK;
}

// Values for the network.cookie.cookieBehavior pref are documented in
// nsCookieService.cpp.
#define COOKIE_BEHAVIOR_REJECT 2

NS_IMETHODIMP
Navigator::GetCookieEnabled(bool* aCookieEnabled)
{
  *aCookieEnabled =
    (Preferences::GetInt("network.cookie.cookieBehavior",
                         COOKIE_BEHAVIOR_REJECT) != COOKIE_BEHAVIOR_REJECT);

  // Check whether an exception overrides the global cookie behavior
  // Note that the code for getting the URI here matches that in
  // nsHTMLDocument::SetCookie.
  nsCOMPtr<nsPIDOMWindow> win(do_QueryReferent(mWindow));

  if (!win || !win->GetDocShell()) {
    return NS_OK;
  }

  nsCOMPtr<nsIDocument> doc = do_QueryInterface(win->GetExtantDocument());
  if (!doc) {
    return NS_OK;
  }

  nsCOMPtr<nsIURI> codebaseURI;
  doc->NodePrincipal()->GetURI(getter_AddRefs(codebaseURI));

  if (!codebaseURI) {
    // Not a codebase, so technically can't set cookies, but let's
    // just return the default value.
    return NS_OK;
  }

  nsCOMPtr<nsICookiePermission> permMgr =
    do_GetService(NS_COOKIEPERMISSION_CONTRACTID);
  NS_ENSURE_TRUE(permMgr, NS_OK);

  // Pass null for the channel, just like the cookie service does.
  nsCookieAccess access;
  nsresult rv = permMgr->CanAccess(codebaseURI, nsnull, &access);
  NS_ENSURE_SUCCESS(rv, NS_OK);

  if (access != nsICookiePermission::ACCESS_DEFAULT) {
    *aCookieEnabled = access != nsICookiePermission::ACCESS_DENY;
  }

  return NS_OK;
}

NS_IMETHODIMP
Navigator::GetOnLine(bool* aOnline)
{
  NS_PRECONDITION(aOnline, "Null out param");

  *aOnline = !NS_IsOffline();
  return NS_OK;
}

NS_IMETHODIMP
Navigator::GetBuildID(nsAString& aBuildID)
{
  if (!nsContentUtils::IsCallerTrustedForRead()) {
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

  nsCAutoString buildID;
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
  Telemetry::AutoTimer<Telemetry::CHECK_JAVA_ENABLED> telemetryTimer;
  // Return true if we have a handler for "application/x-java-vm",
  // otherwise return false.
  *aReturn = false;

  if (!mMimeTypes) {
    mMimeTypes = new nsMimeTypeArray(this);
  }

  RefreshMIMEArray();

  PRUint32 count;
  mMimeTypes->GetLength(&count);
  for (PRUint32 i = 0; i < count; i++) {
    nsresult rv;
    nsIDOMMimeType* type = mMimeTypes->GetItemAt(i, &rv);

    if (NS_FAILED(rv) || !type) {
      continue;
    }

    nsAutoString mimeString;
    if (NS_FAILED(type->GetType(mimeString))) {
      continue;
    }

    if (mimeString.EqualsLiteral("application/x-java-vm")) {
      *aReturn = true;
      break;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
Navigator::TaintEnabled(bool *aReturn)
{
  *aReturn = false;
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
  VibrateWindowListener(nsIDOMWindow *aWindow, nsIDOMDocument *aDocument)
  {
    mWindow = do_GetWeakReference(aWindow);
    mDocument = do_GetWeakReference(aDocument);

    nsCOMPtr<nsIDOMEventTarget> target = do_QueryInterface(aDocument);
    NS_NAMED_LITERAL_STRING(visibilitychange, "mozvisibilitychange");
    target->AddSystemEventListener(visibilitychange,
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

nsRefPtr<VibrateWindowListener> gVibrateWindowListener;

NS_IMETHODIMP
VibrateWindowListener::HandleEvent(nsIDOMEvent* aEvent)
{
  nsCOMPtr<nsIDOMEventTarget> target;
  aEvent->GetTarget(getter_AddRefs(target));
  nsCOMPtr<nsIDOMDocument> doc = do_QueryInterface(target);

  bool hidden = true;
  if (doc) {
    doc->GetMozHidden(&hidden);
  }

  if (hidden) {
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
  nsCOMPtr<nsIDOMEventTarget> target = do_QueryReferent(mDocument);
  if (!target) {
    return;
  }
  NS_NAMED_LITERAL_STRING(visibilitychange, "mozvisibilitychange");
  target->RemoveSystemEventListener(visibilitychange, this,
                                    true /* use capture */);
}

/**
 * Converts a jsval into a vibration duration, checking that the duration is in
 * bounds (non-negative and not larger than sMaxVibrateMS).
 *
 * Returns true on success, false on failure.
 */
bool
GetVibrationDurationFromJsval(const jsval& aJSVal, JSContext* cx,
                              PRInt32 *aOut)
{
  return JS_ValueToInt32(cx, aJSVal, aOut) &&
         *aOut >= 0 && static_cast<PRUint32>(*aOut) <= sMaxVibrateMS;
}

} // anonymous namespace

NS_IMETHODIMP
Navigator::MozVibrate(const jsval& aPattern, JSContext* cx)
{
  nsCOMPtr<nsPIDOMWindow> win = do_QueryReferent(mWindow);
  NS_ENSURE_TRUE(win, NS_OK);

  nsIDOMDocument* domDoc = win->GetExtantDocument();
  NS_ENSURE_TRUE(domDoc, NS_ERROR_FAILURE);

  bool hidden = true;
  domDoc->GetMozHidden(&hidden);
  if (hidden) {
    // Hidden documents cannot start or stop a vibration.
    return NS_OK;
  }

  nsAutoTArray<PRUint32, 8> pattern;

  // null or undefined pattern is an error.
  if (JSVAL_IS_NULL(aPattern) || JSVAL_IS_VOID(aPattern)) {
    return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
  }

  if (JSVAL_IS_PRIMITIVE(aPattern)) {
    PRInt32 p;
    if (GetVibrationDurationFromJsval(aPattern, cx, &p)) {
      pattern.AppendElement(p);
    }
    else {
      return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
    }
  }
  else {
    JSObject *obj = JSVAL_TO_OBJECT(aPattern);
    PRUint32 length;
    if (!JS_GetArrayLength(cx, obj, &length) || length > sMaxVibrateListLen) {
      return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
    }
    pattern.SetLength(length);

    for (PRUint32 i = 0; i < length; ++i) {
      jsval v;
      PRInt32 pv;
      if (JS_GetElement(cx, obj, i, &v) &&
          GetVibrationDurationFromJsval(v, cx, &pv)) {
        pattern[i] = pv;
      }
      else {
        return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
      }
    }
  }

  // The spec says we check sVibratorEnabled after we've done the sanity
  // checking on the pattern.
  if (!sVibratorEnabled) {
    return NS_OK;
  }

  // Add a listener to cancel the vibration if the document becomes hidden,
  // and remove the old mozvisibility listener, if there was one.

  if (!gVibrateWindowListener) {
    // If gVibrateWindowListener is null, this is the first time we've vibrated,
    // and we need to register a listener to clear gVibrateWindowListener on
    // shutdown.
    ClearOnShutdown(&gVibrateWindowListener);
  }
  else {
    gVibrateWindowListener->RemoveListener();
  }
  gVibrateWindowListener = new VibrateWindowListener(win, domDoc);

  nsCOMPtr<nsIDOMWindow> domWindow =
    do_QueryInterface(static_cast<nsIDOMWindow*>(win));
  hal::Vibrate(pattern, domWindow);
  return NS_OK;
}

//*****************************************************************************
//    Navigator::nsIDOMClientInformation
//*****************************************************************************

NS_IMETHODIMP
Navigator::RegisterContentHandler(const nsAString& aMIMEType,
                                  const nsAString& aURI,
                                  const nsAString& aTitle)
{
  nsCOMPtr<nsPIDOMWindow> win(do_QueryReferent(mWindow));

  if (!win || !win->GetOuterWindow() || !win->GetDocShell()) {
    return NS_OK;
  }

  nsCOMPtr<nsIWebContentHandlerRegistrar> registrar =
    do_GetService(NS_WEBCONTENTHANDLERREGISTRAR_CONTRACTID);
  if (!registrar) {
    return NS_OK;
  }

  return registrar->RegisterContentHandler(aMIMEType, aURI, aTitle,
                                           win->GetOuterWindow());
}

NS_IMETHODIMP
Navigator::RegisterProtocolHandler(const nsAString& aProtocol,
                                   const nsAString& aURI,
                                   const nsAString& aTitle)
{
  nsCOMPtr<nsPIDOMWindow> win(do_QueryReferent(mWindow));

  if (!win || !win->GetOuterWindow() || !win->GetDocShell()) {
    return NS_OK;
  }

  nsCOMPtr<nsIWebContentHandlerRegistrar> registrar =
    do_GetService(NS_WEBCONTENTHANDLERREGISTRAR_CONTRACTID);
  if (!registrar) {
    return NS_OK;
  }

  return registrar->RegisterProtocolHandler(aProtocol, aURI, aTitle,
                                            win->GetOuterWindow());
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
  nsCOMPtr<nsIJSContextStack> stack = do_GetService(sJSStackContractID);
  NS_ENSURE_TRUE(stack, NS_ERROR_FAILURE);

  JSContext* cx = nsnull;
  rv = stack->Peek(&cx);
  NS_ENSURE_TRUE(cx, NS_ERROR_FAILURE);

  rv = nsContentUtils::GetSecurityManager()->CheckSameOrigin(cx, uri);
  NS_ENSURE_SUCCESS(rv, rv);

  // These load flags cause an error to be thrown if there is no
  // valid cache entry, and skip the load if there is.
  // If the cache is busy, assume that it is not yet available rather
  // than waiting for it to become available.
  PRUint32 loadFlags = nsIChannel::INHIBIT_CACHING |
                       nsICachingChannel::LOAD_NO_NETWORK_IO |
                       nsICachingChannel::LOAD_ONLY_IF_MODIFIED |
                       nsICachingChannel::LOAD_BYPASS_LOCAL_CACHE_IF_BUSY;

  if (aWhenOffline) {
    loadFlags |= nsICachingChannel::LOAD_CHECK_OFFLINE_CACHE |
                 nsICachingChannel::LOAD_ONLY_FROM_CACHE |
                 nsIRequest::LOAD_FROM_CACHE;
  }

  nsCOMPtr<nsIChannel> channel;
  rv = NS_NewChannel(getter_AddRefs(channel), uri,
                     nsnull, nsnull, nsnull, loadFlags);
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
//    Navigator::nsIDOMNavigatorGeolocation
//*****************************************************************************

NS_IMETHODIMP Navigator::GetGeolocation(nsIDOMGeoGeolocation** _retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = nsnull;

  if (!Preferences::GetBool("geo.enabled", true)) {
    return NS_OK;
  }

  if (mGeolocation) {
    NS_ADDREF(*_retval = mGeolocation);
    return NS_OK;
  }

  nsCOMPtr<nsPIDOMWindow> win(do_QueryReferent(mWindow));

  if (!win || !win->GetOuterWindow() || !win->GetDocShell()) {
    return NS_ERROR_FAILURE;
  }

  mGeolocation = new nsGeolocation();
  if (!mGeolocation) {
    return NS_ERROR_FAILURE;
  }

  if (NS_FAILED(mGeolocation->Init(win->GetOuterWindow()))) {
    mGeolocation = nsnull;
    return NS_ERROR_FAILURE;
  }

  NS_ADDREF(*_retval = mGeolocation);
  return NS_OK;
}

//*****************************************************************************
//    Navigator::nsIDOMNavigatorDesktopNotification
//*****************************************************************************

NS_IMETHODIMP Navigator::GetMozNotification(nsIDOMDesktopNotificationCenter** aRetVal)
{
  NS_ENSURE_ARG_POINTER(aRetVal);
  *aRetVal = nsnull;

  if (mNotification) {
    NS_ADDREF(*aRetVal = mNotification);
    return NS_OK;
  }

  nsCOMPtr<nsPIDOMWindow> win(do_QueryReferent(mWindow));
  NS_ENSURE_TRUE(win && win->GetDocShell(), NS_ERROR_FAILURE);

  mNotification = new nsDesktopNotificationCenter(win);

  NS_ADDREF(*aRetVal = mNotification);
  return NS_OK;
}

//*****************************************************************************
//    Navigator::nsIDOMNavigatorBattery
//*****************************************************************************

NS_IMETHODIMP
Navigator::GetMozBattery(nsIDOMMozBatteryManager** aBattery)
{
  if (!mBatteryManager) {
    *aBattery = nsnull;

    nsCOMPtr<nsPIDOMWindow> win(do_QueryReferent(mWindow));
    NS_ENSURE_TRUE(win->GetDocShell(), NS_OK);

    mBatteryManager = new battery::BatteryManager();
    mBatteryManager->Init(win);
  }

  NS_ADDREF(*aBattery = mBatteryManager);

  return NS_OK;
}

NS_IMETHODIMP
Navigator::GetMozPower(nsIDOMMozPowerManager** aPower)
{
  *aPower = nsnull;

  if (!mPowerManager) {
    nsCOMPtr<nsPIDOMWindow> win = do_QueryReferent(mWindow);
    NS_ENSURE_TRUE(win, NS_OK);

    mPowerManager = new power::PowerManager();
    mPowerManager->Init(win);
  }

  nsCOMPtr<nsIDOMMozPowerManager> power(mPowerManager);
  power.forget(aPower);

  return NS_OK;
}

NS_IMETHODIMP
Navigator::RequestWakeLock(const nsAString &aTopic, nsIDOMMozWakeLock **aWakeLock)
{
  *aWakeLock = nsnull;

  nsCOMPtr<nsPIDOMWindow> win = do_QueryReferent(mWindow);
  NS_ENSURE_TRUE(win, NS_OK);

  nsCOMPtr<nsIPowerManagerService> pmService =
    do_GetService(POWERMANAGERSERVICE_CONTRACTID);
  NS_ENSURE_TRUE(pmService, NS_OK);

  return pmService->NewWakeLock(aTopic, win, aWakeLock);
}

//*****************************************************************************
//    Navigator::nsIDOMNavigatorSms
//*****************************************************************************

bool
Navigator::IsSmsAllowed() const
{
  static const bool defaultSmsPermission = false;

  // First of all, the general pref has to be turned on.
  if (!Preferences::GetBool("dom.sms.enabled", defaultSmsPermission)) {
    return false;
  }

  // In addition of having 'dom.sms.enabled' set to true, we require the
  // website to be whitelisted. This is a temporary 'security model'.
  // 'dom.sms.whitelist' has to contain comma-separated values of URI prepath.
  // For local files, "file://" must be listed.
  // For data-urls: "moz-nullprincipal:".
  // Chrome files also have to be whitelisted for the moment.
  nsCOMPtr<nsPIDOMWindow> win(do_QueryReferent(mWindow));

  if (!win || !win->GetDocShell()) {
    return defaultSmsPermission;
  }

  nsCOMPtr<nsIDocument> doc = do_QueryInterface(win->GetExtantDocument());
  if (!doc) {
    return defaultSmsPermission;
  }

  nsCOMPtr<nsIURI> uri;
  doc->NodePrincipal()->GetURI(getter_AddRefs(uri));

  if (!uri) {
    return defaultSmsPermission;
  }

  nsCAutoString uriPrePath;
  uri->GetPrePath(uriPrePath);

  const nsAdoptingString& whitelist =
    Preferences::GetString("dom.sms.whitelist");

  nsCharSeparatedTokenizer tokenizer(whitelist, ',',
                                     nsCharSeparatedTokenizerTemplate<>::SEPARATOR_OPTIONAL);

  while (tokenizer.hasMoreTokens()) {
    const nsSubstring& whitelistItem = tokenizer.nextToken();

    if (NS_ConvertUTF16toUTF8(whitelistItem).Equals(uriPrePath)) {
      return true;
    }
  }

  // The current page hasn't been whitelisted.
  return false;
}

bool
Navigator::IsSmsSupported() const
{
#ifdef MOZ_WEBSMS_BACKEND
  nsCOMPtr<nsISmsService> smsService = do_GetService(SMS_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE(smsService, false);

  bool result = false;
  smsService->HasSupport(&result);

  return result;
#else
  return false;
#endif
}

NS_IMETHODIMP
Navigator::GetMozSms(nsIDOMMozSmsManager** aSmsManager)
{
  *aSmsManager = nsnull;

  if (!mSmsManager) {
    if (!IsSmsSupported() || !IsSmsAllowed()) {
      return NS_OK;
    }

    nsCOMPtr<nsPIDOMWindow> window = do_QueryReferent(mWindow);
    NS_ENSURE_TRUE(window && window->GetDocShell(), NS_OK);

    mSmsManager = new sms::SmsManager();
    mSmsManager->Init(window);
  }

  NS_ADDREF(*aSmsManager = mSmsManager);

  return NS_OK;
}

#ifdef MOZ_B2G_RIL

//*****************************************************************************
//    nsNavigator::nsIDOMNavigatorTelephony
//*****************************************************************************

NS_IMETHODIMP
Navigator::GetMozTelephony(nsIDOMTelephony** aTelephony)
{
  nsCOMPtr<nsIDOMTelephony> telephony = mTelephony;

  if (!telephony) {
    nsCOMPtr<nsPIDOMWindow> window = do_QueryReferent(mWindow);
    NS_ENSURE_TRUE(window, NS_ERROR_FAILURE);

    nsresult rv = NS_NewTelephony(window, getter_AddRefs(mTelephony));
    NS_ENSURE_SUCCESS(rv, rv);

    // mTelephony may be null here!
    telephony = mTelephony;
  }

  telephony.forget(aTelephony);
  return NS_OK;
}

#endif // MOZ_B2G_RIL

//*****************************************************************************
//    Navigator::nsIDOMNavigatorNetwork
//*****************************************************************************

NS_IMETHODIMP
Navigator::GetMozConnection(nsIDOMMozConnection** aConnection)
{
  *aConnection = nsnull;

  if (!mConnection) {
    nsCOMPtr<nsPIDOMWindow> window = do_QueryReferent(mWindow);
    NS_ENSURE_TRUE(window && window->GetDocShell(), NS_OK);

    mConnection = new network::Connection();
    mConnection->Init(window);
  }

  NS_ADDREF(*aConnection = mConnection);
  return NS_OK;
}

#ifdef MOZ_B2G_BT
//*****************************************************************************
//    nsNavigator::nsIDOMNavigatorBluetooth
//*****************************************************************************

NS_IMETHODIMP
Navigator::GetMozBluetooth(nsIDOMBluetoothAdapter** aBluetooth)
{
  nsCOMPtr<nsIDOMBluetoothAdapter> bluetooth = mBluetooth;

  if (!bluetooth) {
    nsCOMPtr<nsPIDOMWindow> window = do_QueryReferent(mWindow);
    NS_ENSURE_TRUE(window, NS_ERROR_FAILURE);

    mBluetooth = new bluetooth::BluetoothAdapter();

    bluetooth = mBluetooth;
  }

  bluetooth.forget(aBluetooth);
  return NS_OK;
}
#endif //MOZ_B2G_BT

size_t
Navigator::SizeOfIncludingThis(nsMallocSizeOfFun aMallocSizeOf) const
{
  size_t n = aMallocSizeOf(this);

  // TODO: add SizeOfIncludingThis() to nsMimeTypeArray, bug 674113.
  // TODO: add SizeOfIncludingThis() to nsPluginArray, bug 674114.
  // TODO: add SizeOfIncludingThis() to nsGeolocation, bug 674115.
  // TODO: add SizeOfIncludingThis() to nsDesktopNotificationCenter, bug 674116.

  return n;
}

void
Navigator::SetWindow(nsPIDOMWindow *aInnerWindow)
{
  NS_ASSERTION(aInnerWindow->IsInnerWindow(),
               "Navigator must get an inner window!");
  mWindow = do_GetWeakReference(aInnerWindow);
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

  nsCAutoString ua;
  rv = service->GetUserAgent(ua);
  CopyASCIItoUTF16(ua, aUserAgent);

  return rv;
}

nsresult
NS_GetNavigatorPlatform(nsAString& aPlatform)
{
  if (!nsContentUtils::IsCallerTrustedForRead()) {
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
  nsCAutoString plat;
  rv = service->GetOscpu(plat);
  CopyASCIItoUTF16(plat, aPlatform);
#endif

  return rv;
}
nsresult
NS_GetNavigatorAppVersion(nsAString& aAppVersion)
{
  if (!nsContentUtils::IsCallerTrustedForRead()) {
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

  nsCAutoString str;
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

nsresult
NS_GetNavigatorAppName(nsAString& aAppName)
{
  if (!nsContentUtils::IsCallerTrustedForRead()) {
    const nsAdoptingString& override =
      mozilla::Preferences::GetString("general.appname.override");

    if (override) {
      aAppName = override;
      return NS_OK;
    }
  }

  aAppName.AssignLiteral("Netscape");
  return NS_OK;
}
