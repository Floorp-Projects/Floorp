/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsContentPermissionHelper.h"
#include "nsXULAppAPI.h"

#include "mozilla/dom/PBrowserChild.h"
#include "mozilla/dom/PBrowserParent.h"
#include "mozilla/dom/ContentChild.h"
#include "nsNetUtil.h"

#include "nsFrameManager.h"
#include "nsFrameLoader.h"
#include "nsIFrameLoader.h"

#include "nsIDocShellTreeOwner.h"
#include "nsIDocShellTreeItem.h"
#include "nsIWebProgressListener2.h"

#include "nsISettingsService.h"

#include "nsDOMEventTargetHelper.h"
#include "TabChild.h"

#include "nsGeolocation.h"
#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsIDOMWindow.h"
#include "nsDOMClassInfoID.h"
#include "nsComponentManagerUtils.h"
#include "nsICategoryManager.h"
#include "nsISupportsPrimitives.h"
#include "nsServiceManagerUtils.h"
#include "nsContentUtils.h"
#include "nsIURI.h"
#include "nsIPermissionManager.h"
#include "nsIObserverService.h"
#include "nsIJSContextStack.h"
#include "nsThreadUtils.h"
#include "mozilla/Services.h"
#include "mozilla/unused.h"
#include "mozilla/Preferences.h"
#include "mozilla/Attributes.h"
#include "mozilla/ClearOnShutdown.h"

#include <math.h>
#include <algorithm>

#ifdef MOZ_MAEMO_LIBLOCATION
#include "MaemoLocationProvider.h"
#endif

#ifdef MOZ_ENABLE_QTMOBILITY
#include "QTMLocationProvider.h"
#endif

#ifdef MOZ_WIDGET_ANDROID
#include "AndroidLocationProvider.h"
#endif

#ifdef MOZ_WIDGET_GONK
#include "GonkGPSGeolocationProvider.h"
#endif

#include "nsIDOMDocument.h"
#include "nsIDocument.h"

// Some limit to the number of get or watch geolocation requests
// that a window can make.
#define MAX_GEO_REQUESTS_PER_WINDOW  1500

// The settings key.
#define GEO_SETINGS_ENABLED          "geolocation.enabled"

using mozilla::unused;          // <snicker>
using namespace mozilla;
using namespace mozilla::dom;

class GeolocationSettingsCallback : public nsISettingsServiceCallback
{
public:
  NS_DECL_ISUPPORTS

  GeolocationSettingsCallback() {
    MOZ_COUNT_CTOR(GeolocationSettingsCallback);
  }

  virtual ~GeolocationSettingsCallback() {
    MOZ_COUNT_DTOR(GeolocationSettingsCallback);
  }

  NS_IMETHOD Handle(const nsAString& aName, const jsval& aResult)
  {
    MOZ_ASSERT(NS_IsMainThread());

    // The geolocation is enabled by default:
    bool value = true;
    if (aResult.isBoolean()) {
      value = aResult.toBoolean();
    }

    MozSettingValue(value);
    return NS_OK;
  }

  NS_IMETHOD HandleError(const nsAString& aName)
  {
    NS_WARNING("Unable to get value for '" GEO_SETINGS_ENABLED "'");

    // Default it's enabled:
    MozSettingValue(true);
    return NS_OK;
  }

  void MozSettingValue(const bool aValue)
  {
    nsRefPtr<nsGeolocationService> gs = nsGeolocationService::GetGeolocationService();
    if (gs) {
      gs->HandleMozsettingValue(aValue);
    }
  }
};

NS_IMPL_ISUPPORTS1(GeolocationSettingsCallback, nsISettingsServiceCallback)

class RequestPromptEvent : public nsRunnable
{
public:
  RequestPromptEvent(nsGeolocationRequest* request)
    : mRequest(request)
  {
  }

  NS_IMETHOD Run() {
    nsCOMPtr<nsIContentPermissionPrompt> prompt = do_CreateInstance(NS_CONTENT_PERMISSION_PROMPT_CONTRACTID);
    if (prompt) {
      prompt->Prompt(mRequest);
    }
    return NS_OK;
  }

private:
  nsRefPtr<nsGeolocationRequest> mRequest;
};

class RequestAllowEvent : public nsRunnable
{
public:
  RequestAllowEvent(int allow, nsGeolocationRequest* request)
    : mAllow(allow),
      mRequest(request)
  {
  }

  NS_IMETHOD Run() {
    if (mAllow) {
      mRequest->Allow();
    } else {
      mRequest->Cancel();
    }
    return NS_OK;
  }

private:
  bool mAllow;
  nsRefPtr<nsGeolocationRequest> mRequest;
};

class RequestSendLocationEvent : public nsRunnable
{
public:
  // a bit funky.  if locator is passed, that means this
  // event should remove the request from it.  If we ever
  // have to do more, then we can change this around.
  RequestSendLocationEvent(nsIDOMGeoPosition* aPosition,
                           nsGeolocationRequest* aRequest,
                           nsGeolocation* aLocator)
    : mPosition(aPosition),
      mRequest(aRequest),
      mLocator(aLocator)
  {
  }

  NS_IMETHOD Run() {
    mRequest->SendLocation(mPosition);
    if (mLocator) {
      mLocator->RemoveRequest(mRequest);
    }
    return NS_OK;
  }

private:
  nsCOMPtr<nsIDOMGeoPosition>    mPosition;
  nsRefPtr<nsGeolocationRequest> mRequest;

  nsRefPtr<nsGeolocation>        mLocator;
};

class RequestRestartTimerEvent : public nsRunnable
{
public:
  RequestRestartTimerEvent(nsGeolocationRequest* aRequest)
    : mRequest(aRequest)
  {
  }

  NS_IMETHOD Run() {
    mRequest->SetTimeoutTimer();
    return NS_OK;
  }

private:
  nsRefPtr<nsGeolocationRequest> mRequest;
};

////////////////////////////////////////////////////
// nsDOMGeoPositionError
////////////////////////////////////////////////////

class nsDOMGeoPositionError MOZ_FINAL : public nsIDOMGeoPositionError
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMGEOPOSITIONERROR

  nsDOMGeoPositionError(int16_t aCode);
  void NotifyCallback(nsIDOMGeoPositionErrorCallback* callback);

private:
  ~nsDOMGeoPositionError();
  int16_t mCode;
};

DOMCI_DATA(GeoPositionError, nsDOMGeoPositionError)

NS_INTERFACE_MAP_BEGIN(nsDOMGeoPositionError)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMGeoPositionError)
  NS_INTERFACE_MAP_ENTRY(nsIDOMGeoPositionError)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(GeoPositionError)
NS_INTERFACE_MAP_END

NS_IMPL_THREADSAFE_ADDREF(nsDOMGeoPositionError)
NS_IMPL_THREADSAFE_RELEASE(nsDOMGeoPositionError)

nsDOMGeoPositionError::nsDOMGeoPositionError(int16_t aCode)
  : mCode(aCode)
{
}

nsDOMGeoPositionError::~nsDOMGeoPositionError(){}


NS_IMETHODIMP
nsDOMGeoPositionError::GetCode(int16_t *aCode)
{
  NS_ENSURE_ARG_POINTER(aCode);
  *aCode = mCode;
  return NS_OK;
}

void
nsDOMGeoPositionError::NotifyCallback(nsIDOMGeoPositionErrorCallback* aCallback)
{
  if (!aCallback) {
    return;
  }

  // Ensure that the proper context is on the stack (bug 452762)
  nsCOMPtr<nsIJSContextStack> stack(do_GetService("@mozilla.org/js/xpc/ContextStack;1"));
  if (!stack || NS_FAILED(stack->Push(nullptr))) {
    return;
  }

  nsAutoMicroTask mt;
  aCallback->HandleEvent(this);

  // remove the stack
  JSContext* cx;
  stack->Pop(&cx);
}
////////////////////////////////////////////////////
// nsGeolocationRequest
////////////////////////////////////////////////////

nsGeolocationRequest::nsGeolocationRequest(nsGeolocation* aLocator,
                                           nsIDOMGeoPositionCallback* aCallback,
                                           nsIDOMGeoPositionErrorCallback* aErrorCallback,
                                           mozilla::dom::GeoPositionOptions* aOptions,
                                           bool aWatchPositionRequest,
                                           int32_t aWatchId)
  : mAllowed(false),
    mCleared(false),
    mIsFirstUpdate(true),
    mIsWatchPositionRequest(aWatchPositionRequest),
    mCallback(aCallback),
    mErrorCallback(aErrorCallback),
    mOptions(aOptions),
    mLocator(aLocator),
    mWatchId(aWatchId)
{
}

nsGeolocationRequest::~nsGeolocationRequest()
{
}


static mozilla::dom::GeoPositionOptions*
OptionsFromJSOptions(JSContext* aCx, const jsval& aOptions, nsresult* aRv)
{
  *aRv = NS_OK;
  nsAutoPtr<mozilla::dom::GeoPositionOptions> options(nullptr);
  if (aCx && !JSVAL_IS_VOID(aOptions) && !JSVAL_IS_NULL(aOptions)) {
    options = new mozilla::dom::GeoPositionOptions();
    nsresult rv = options->Init(aCx, &aOptions);
    if (NS_FAILED(rv)) {
      *aRv = rv;
      return nullptr;
    }
  }
  return options.forget();
}

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsGeolocationRequest)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIContentPermissionRequest)
  NS_INTERFACE_MAP_ENTRY(nsIContentPermissionRequest)
  NS_INTERFACE_MAP_ENTRY(nsITimerCallback)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsGeolocationRequest)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsGeolocationRequest)

NS_IMPL_CYCLE_COLLECTION_3(nsGeolocationRequest, mCallback, mErrorCallback, mLocator)


void
nsGeolocationRequest::NotifyError(int16_t errorCode)
{
  nsRefPtr<nsDOMGeoPositionError> positionError = new nsDOMGeoPositionError(errorCode);
  if (!positionError) {
    return;
  }

  positionError->NotifyCallback(mErrorCallback);
}


NS_IMETHODIMP
nsGeolocationRequest::Notify(nsITimer* aTimer)
{
  if (mCleared) {
    return NS_OK;
  }

  // If we haven't gotten an answer from the geolocation
  // provider yet, fire a TIMEOUT error and reset the timer.
  if (!mIsWatchPositionRequest) {
    mLocator->RemoveRequest(this);
  }

  NotifyError(nsIDOMGeoPositionError::TIMEOUT);

  if (mIsWatchPositionRequest) {
    SetTimeoutTimer();
  }

  return NS_OK;
}

NS_IMETHODIMP
nsGeolocationRequest::GetPrincipal(nsIPrincipal * *aRequestingPrincipal)
{
  NS_ENSURE_ARG_POINTER(aRequestingPrincipal);

  nsCOMPtr<nsIPrincipal> principal = mLocator->GetPrincipal();
  principal.forget(aRequestingPrincipal);

  return NS_OK;
}

NS_IMETHODIMP
nsGeolocationRequest::GetType(nsACString & aType)
{
  aType = "geolocation";
  return NS_OK;
}

NS_IMETHODIMP
nsGeolocationRequest::GetAccess(nsACString & aAccess)
{
  aAccess = "unused";
  return NS_OK;
}

NS_IMETHODIMP
nsGeolocationRequest::GetWindow(nsIDOMWindow * *aRequestingWindow)
{
  NS_ENSURE_ARG_POINTER(aRequestingWindow);

  nsCOMPtr<nsIDOMWindow> window = do_QueryReferent(mLocator->GetOwner());
  window.forget(aRequestingWindow);

  return NS_OK;
}

NS_IMETHODIMP
nsGeolocationRequest::GetElement(nsIDOMElement * *aRequestingElement)
{
  NS_ENSURE_ARG_POINTER(aRequestingElement);
  *aRequestingElement = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
nsGeolocationRequest::Cancel()
{
  // remove ourselves from the locators callback lists.
  mLocator->RemoveRequest(this);

  NotifyError(nsIDOMGeoPositionError::PERMISSION_DENIED);
  return NS_OK;
}

NS_IMETHODIMP
nsGeolocationRequest::Allow()
{
  nsCOMPtr<nsIDOMWindow> window;
  GetWindow(getter_AddRefs(window));
  nsCOMPtr<nsIWebNavigation> webNav = do_GetInterface(window);
  nsCOMPtr<nsILoadContext> loadContext = do_QueryInterface(webNav);
  bool isPrivate = loadContext && loadContext->UsePrivateBrowsing();

  // Kick off the geo device, if it isn't already running
  nsRefPtr<nsGeolocationService> gs = nsGeolocationService::GetGeolocationService();
  nsresult rv = gs->StartDevice(GetPrincipal(), isPrivate);

  if (NS_FAILED(rv)) {
    // Location provider error
    NotifyError(nsIDOMGeoPositionError::POSITION_UNAVAILABLE);
    return NS_OK;
  }

  nsCOMPtr<nsIDOMGeoPosition> lastPosition = gs->GetCachedPosition();
  DOMTimeStamp cachedPositionTime;
  if (lastPosition) {
    lastPosition->GetTimestamp(&cachedPositionTime);
  }

  // check to see if we can use a cached value
  //
  // either:
  // a) the user has specified a maximumAge which allows us to return a cached value,
  // -or-
  // b) the cached position time is some reasonable value to return to the user (<30s)

  uint32_t maximumAge = 30 * PR_MSEC_PER_SEC;
  if (mOptions) {
    if (mOptions->maximumAge >= 0) {
      maximumAge = mOptions->maximumAge;
    }
    if (mOptions->enableHighAccuracy) {
      gs->SetHigherAccuracy(true);
    }
  }

  if (lastPosition && maximumAge > 0 &&
      ( PRTime(PR_Now() / PR_USEC_PER_MSEC) - maximumAge <=
        PRTime(cachedPositionTime) )) {
    // okay, we can return a cached position
    mAllowed = true;

    nsCOMPtr<nsIRunnable> ev = new RequestSendLocationEvent(lastPosition,
							    this,
							    mIsWatchPositionRequest ? nullptr : mLocator);
    NS_DispatchToMainThread(ev);
  }

  SetTimeoutTimer();

  mAllowed = true;
  return NS_OK;
}

void
nsGeolocationRequest::SetTimeoutTimer()
{
  if (mTimeoutTimer) {
    mTimeoutTimer->Cancel();
    mTimeoutTimer = nullptr;
  }

  int32_t timeout;
  if (mOptions && (timeout = mOptions->timeout) != 0) {

    if (timeout < 0) {
      timeout = 0;
    } else if (timeout < 10) {
      timeout = 10;
    }

    mTimeoutTimer = do_CreateInstance("@mozilla.org/timer;1");
    mTimeoutTimer->InitWithCallback(this, timeout, nsITimer::TYPE_ONE_SHOT);
  }
}

void
nsGeolocationRequest::MarkCleared()
{
  if (mTimeoutTimer) {
    mTimeoutTimer->Cancel();
    mTimeoutTimer = nullptr;
  }
  mCleared = true;
}

void
nsGeolocationRequest::SendLocation(nsIDOMGeoPosition* aPosition)
{
  if (mCleared || !mAllowed) {
    return;
  }

  if (mTimeoutTimer) {
    mTimeoutTimer->Cancel();
    mTimeoutTimer = nullptr;
  }

  // we should not pass null back to the DOM.
  if (!aPosition) {
    NotifyError(nsIDOMGeoPositionError::POSITION_UNAVAILABLE);
    return;
  }

  // Ensure that the proper context is on the stack (bug 452762)
  nsCOMPtr<nsIJSContextStack> stack(do_GetService("@mozilla.org/js/xpc/ContextStack;1"));
  if (!stack || NS_FAILED(stack->Push(nullptr))) {
    return; // silently fail
  }

  nsAutoMicroTask mt;
  mCallback->HandleEvent(aPosition);

  // remove the stack
  JSContext* cx;
  stack->Pop(&cx);

  if (mIsWatchPositionRequest) {
    SetTimeoutTimer();
  }
}

nsIPrincipal*
nsGeolocationRequest::GetPrincipal()
{
  if (!mLocator) {
    return nullptr;
  }
  return mLocator->GetPrincipal();
}

bool
nsGeolocationRequest::Update(nsIDOMGeoPosition* aPosition, bool aIsBetter)
{
  if (!mAllowed) {
    return false;
  }
  // Only dispatch callbacks if this is the first position for this request, or
  // if the accuracy is as good or improving.
  //
  // This ensures that all listeners get at least one position callback, particularly
  // in the case when newly detected positions are all less accurate than the cached one.
  //
  // Fixes bug 596481
  nsCOMPtr<nsIRunnable> ev;
  if (mIsFirstUpdate || aIsBetter) {
    mIsFirstUpdate = false;
    ev  = new RequestSendLocationEvent(aPosition,
                                       this,
                                       mIsWatchPositionRequest ? nullptr : mLocator);
  } else {
    ev = new RequestRestartTimerEvent(this);
  }
  NS_DispatchToMainThread(ev);
  return true;
}

void
nsGeolocationRequest::Shutdown()
{
  if (mOptions && mOptions->enableHighAccuracy) {
    nsRefPtr<nsGeolocationService> gs = nsGeolocationService::GetGeolocationService();
    if (gs) {
      gs->SetHigherAccuracy(false);
    }
  }

  if (mTimeoutTimer) {
    mTimeoutTimer->Cancel();
    mTimeoutTimer = nullptr;
  }
  mCleared = true;
  mCallback = nullptr;
  mErrorCallback = nullptr;
}

bool nsGeolocationRequest::Recv__delete__(const bool& allow)
{
  if (allow) {
    (void) Allow();
  } else {
    (void) Cancel();
  }
  return true;
}
////////////////////////////////////////////////////
// nsGeolocationService
////////////////////////////////////////////////////
NS_INTERFACE_MAP_BEGIN(nsGeolocationService)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIGeolocationUpdate)
  NS_INTERFACE_MAP_ENTRY(nsIGeolocationUpdate)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
NS_INTERFACE_MAP_END

NS_IMPL_THREADSAFE_ADDREF(nsGeolocationService)
NS_IMPL_THREADSAFE_RELEASE(nsGeolocationService)


static bool sGeoEnabled = true;
static bool sGeoInitPending = true;
static bool sGeoIgnoreLocationFilter = false;
static int32_t sProviderTimeout = 6000; // Time, in milliseconds, to wait for the location provider to spin up.

nsresult nsGeolocationService::Init()
{
  Preferences::AddIntVarCache(&sProviderTimeout, "geo.timeout", sProviderTimeout);
  Preferences::AddBoolVarCache(&sGeoEnabled, "geo.enabled", sGeoEnabled);
  Preferences::AddBoolVarCache(&sGeoIgnoreLocationFilter, "geo.ignore.location_filter", sGeoIgnoreLocationFilter);

  if (!sGeoEnabled) {
    return NS_ERROR_FAILURE;
  }

  if (XRE_GetProcessType() == GeckoProcessType_Content) {
    sGeoInitPending = false;
    return NS_OK;
  }

  nsCOMPtr<nsIGeolocationProvider> provider = do_GetService(NS_GEOLOCATION_PROVIDER_CONTRACTID);
  if (provider) {
    mProviders.AppendObject(provider);
  }

  // look up any providers that were registered via the category manager
  nsCOMPtr<nsICategoryManager> catMan(do_GetService("@mozilla.org/categorymanager;1"));
  if (!catMan) {
    return NS_ERROR_FAILURE;
  }

  // check if the geolocation service is enable from settings
  nsCOMPtr<nsISettingsService> settings =
    do_GetService("@mozilla.org/settingsService;1");

  if (settings) {
    nsCOMPtr<nsISettingsServiceLock> settingsLock;
    nsresult rv = settings->CreateLock(getter_AddRefs(settingsLock));
    NS_ENSURE_SUCCESS(rv, rv);

    nsRefPtr<GeolocationSettingsCallback> callback = new GeolocationSettingsCallback();
    rv = settingsLock->Get(GEO_SETINGS_ENABLED, callback);
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    // If we cannot obtain the settings service, we continue
    // assuming that the geolocation is enabled:
    sGeoInitPending = false;
  }

  // geolocation service can be enabled -> now register observer
  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (!obs) {
    return NS_ERROR_FAILURE;
  }

  obs->AddObserver(this, "quit-application", false);
  obs->AddObserver(this, "mozsettings-changed", false);

  nsCOMPtr<nsISimpleEnumerator> geoproviders;
  catMan->EnumerateCategory("geolocation-provider", getter_AddRefs(geoproviders));
  if (geoproviders) {

    bool hasMore;
    while (NS_SUCCEEDED(geoproviders->HasMoreElements(&hasMore)) && hasMore) {
      nsCOMPtr<nsISupports> elem;
      geoproviders->GetNext(getter_AddRefs(elem));

      nsCOMPtr<nsISupportsCString> elemString = do_QueryInterface(elem);

      nsAutoCString name;
      elemString->GetData(name);

      nsXPIDLCString spec;
      catMan->GetCategoryEntry("geolocation-provider", name.get(), getter_Copies(spec));

      provider = do_GetService(spec);
      if (provider) {
        mProviders.AppendObject(provider);
      }
    }
  }

  // we should move these providers outside of this file! dft

#ifdef MOZ_MAEMO_LIBLOCATION
  provider = new MaemoLocationProvider();
  if (provider) {
    mProviders.AppendObject(provider);
  }
#endif

#ifdef MOZ_ENABLE_QTMOBILITY
  provider = new QTMLocationProvider();
  if (provider) {
    mProviders.AppendObject(provider);
  }
#endif

#ifdef MOZ_WIDGET_ANDROID
  provider = new AndroidLocationProvider();
  if (provider) {
    mProviders.AppendObject(provider);
  }
#endif

#ifdef MOZ_WIDGET_GONK
  provider = do_GetService(GONK_GPS_GEOLOCATION_PROVIDER_CONTRACTID);
  if (provider) {
    mProviders.AppendObject(provider);
  }
#endif

  return NS_OK;
}

nsGeolocationService::~nsGeolocationService()
{
}

void
nsGeolocationService::HandleMozsettingChanged(const PRUnichar* aData)
{
    // The string that we're interested in will be a JSON string that looks like:
    //  {"key":"gelocation.enabled","value":true}

    nsCOMPtr<nsIThreadJSContextStack> stack = do_GetService("@mozilla.org/js/xpc/ContextStack;1");
    if (!stack) {
      return;
    }

    JSContext *cx = stack->GetSafeJSContext();
    if (!cx) {
      return;
    }

    nsDependentString dataStr(aData);
    JS::Value val;
    if (!JS_ParseJSON(cx, dataStr.get(), dataStr.Length(), &val) || !val.isObject()) {
      return;
    }

    JSObject &obj(val.toObject());
    JS::Value key;
    if (!JS_GetProperty(cx, &obj, "key", &key) || !key.isString()) {
      return;
    }

    JSBool match;
    if (!JS_StringEqualsAscii(cx, key.toString(), GEO_SETINGS_ENABLED, &match) || (match != JS_TRUE)) {
      return;
    }

    JS::Value value;
    if (!JS_GetProperty(cx, &obj, "value", &value) || !value.isBoolean()) {
      return;
    }

    HandleMozsettingValue(value.toBoolean());
}

void
nsGeolocationService::HandleMozsettingValue(const bool aValue)
{
    if (!aValue) {
      // turn things off
      StopDevice();
      Update(nullptr);
      mLastPosition = nullptr;
      sGeoEnabled = false;
    } else {
      sGeoEnabled = true;
    }

    if (sGeoInitPending) {
      sGeoInitPending = false;
      for (uint32_t i = 0, length = mGeolocators.Length(); i < length; ++i) {
        mGeolocators[i]->ServiceReady();
      }
    }
}

NS_IMETHODIMP
nsGeolocationService::Observe(nsISupports* aSubject,
                              const char* aTopic,
                              const PRUnichar* aData)
{
  if (!strcmp("quit-application", aTopic)) {
    nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
    if (obs) {
      obs->RemoveObserver(this, "quit-application");
      obs->RemoveObserver(this, "mozsettings-changed");
    }

    for (uint32_t i = 0; i< mGeolocators.Length(); i++) {
      mGeolocators[i]->Shutdown();
    }
    StopDevice();

    return NS_OK;
  }

  if (!strcmp("mozsettings-changed", aTopic)) {
    HandleMozsettingChanged(aData);
    return NS_OK;
  }

  if (!strcmp("timer-callback", aTopic)) {
    // decide if we can close down the service.
    for (uint32_t i = 0; i< mGeolocators.Length(); i++)
      if (mGeolocators[i]->HasActiveCallbacks()) {
        SetDisconnectTimer();
        return NS_OK;
      }

    // okay to close up.
    StopDevice();
    Update(nullptr);
    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsGeolocationService::Update(nsIDOMGeoPosition *aSomewhere)
{
  // here we have to determine this aSomewhere is a "better"
  // position than any previously recv'ed.

  bool isBetter = IsBetterPosition(aSomewhere);

  if (isBetter) {
    SetCachedPosition(aSomewhere);
  }

  for (uint32_t i = 0; i< mGeolocators.Length(); i++) {
    mGeolocators[i]->Update(aSomewhere, isBetter);
  }
  return NS_OK;
}

PRBool
nsGeolocationService::IsBetterPosition(nsIDOMGeoPosition *aSomewhere)
{
  if (!aSomewhere) {
    return false;
  }

  if (mProviders.Count() == 1 || !mLastPosition) {
    return true;
  }

  nsCOMPtr<nsIDOMGeoPositionCoords> coords;
  mLastPosition->GetCoords(getter_AddRefs(coords));
  if (!coords) {
    return false;
  }

  double oldAccuracy;
  nsresult rv = coords->GetAccuracy(&oldAccuracy);
  NS_ENSURE_SUCCESS(rv, false);

  double oldLat, oldLon;
  rv = coords->GetLongitude(&oldLon);
  NS_ENSURE_SUCCESS(rv, false);

  rv = coords->GetLatitude(&oldLat);
  NS_ENSURE_SUCCESS(rv, false);

  aSomewhere->GetCoords(getter_AddRefs(coords));
  if (!coords) {
    return false;
  }

  double newAccuracy;
  rv = coords->GetAccuracy(&newAccuracy);
  NS_ENSURE_SUCCESS(rv, false);

  double newLat, newLon;
  rv = coords->GetLongitude(&newLon);
  NS_ENSURE_SUCCESS(rv, false);

  rv = coords->GetLatitude(&newLat);
  NS_ENSURE_SUCCESS(rv, false);

  // Latitude and longitude is reported in degrees.
  // However, it is easier to work in radian:
  // see: http://en.wikipedia.org/wiki/Radian
  double radsInDeg = M_PI / 180.0;

  newLat *= radsInDeg;
  newLon *= radsInDeg;
  oldLat *= radsInDeg;
  oldLon *= radsInDeg;

  // WGS84 equatorial radius of earth = 6378137m
  // http://en.wikipedia.org/wiki/WGS84
  double radius = 6378137;

  // We want to calculate the "Great Circle distance"
  // between the point (lat1, lon1) and (lat2, lon2).  We
  // will use the spherical law of cosines to the triangle
  // formed by our two points and the north pole.
  //
  // a = sin ( lat1 ) * sin ( lat2 )  + cos ( lat1 ) * cos (lat2) * cos (lon1 - lon2)
  // R = radius of circle
  // distance = arccos ( a ) * R 
  //
  // http://en.wikipedia.org/wiki/Great-circle_distance

  double delta = acos( (sin(newLat) * sin(oldLat)) +
                       (cos(newLat) * cos(oldLat) * cos(oldLon - newLon)) ) * radius; 

  // The threshold is when the distance between the two
  // positions exceeds the worse (larger value) of the two
  // accuracies.
  double max_accuracy = std::max(oldAccuracy, newAccuracy);
  if (delta > max_accuracy)
    return true;

  // check to see if the aSomewhere position is more accurate
  if (oldAccuracy >= newAccuracy)
    return true;

  return false;
}

void
nsGeolocationService::SetCachedPosition(nsIDOMGeoPosition* aPosition)
{
  mLastPosition = aPosition;
}

nsIDOMGeoPosition*
nsGeolocationService::GetCachedPosition()
{
  return mLastPosition;
}

nsresult
nsGeolocationService::StartDevice(nsIPrincipal *aPrincipal, bool aRequestPrivate)
{
  if (!sGeoEnabled || sGeoInitPending) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  // we do not want to keep the geolocation devices online
  // indefinitely.  Close them down after a reasonable period of
  // inactivivity
  SetDisconnectTimer();

  if (XRE_GetProcessType() == GeckoProcessType_Content) {
    ContentChild* cpc = ContentChild::GetSingleton();
    cpc->SendAddGeolocationListener(IPC::Principal(aPrincipal));
    return NS_OK;
  }

  // Start them up!
  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (!obs) {
    return NS_ERROR_FAILURE;
  }

  for (int32_t i = 0; i < mProviders.Count(); i++) {
    mProviders[i]->Startup();
    mProviders[i]->Watch(this, aRequestPrivate);
    obs->NotifyObservers(mProviders[i],
                         "geolocation-device-events",
                         NS_LITERAL_STRING("starting").get());
  }

  return NS_OK;
}

void
nsGeolocationService::SetDisconnectTimer()
{
  if (!mDisconnectTimer) {
    mDisconnectTimer = do_CreateInstance("@mozilla.org/timer;1");
  } else {
    mDisconnectTimer->Cancel();
  }

  mDisconnectTimer->Init(this,
                         sProviderTimeout,
                         nsITimer::TYPE_ONE_SHOT);
}

void
nsGeolocationService::SetHigherAccuracy(bool aEnable)
{
  if (XRE_GetProcessType() == GeckoProcessType_Content) {
    ContentChild* cpc = ContentChild::GetSingleton();
    cpc->SendSetGeolocationHigherAccuracy(aEnable);
    return;
  }

  if (!mHigherAccuracy && aEnable) {
    for (int32_t i = 0; i < mProviders.Count(); i++) {
      mProviders[i]->SetHighAccuracy(true);
    }
  }

  if (mHigherAccuracy && !aEnable) {
    for (int32_t i = 0; i < mProviders.Count(); i++) {
      mProviders[i]->SetHighAccuracy(false);
    }
  }

  mHigherAccuracy = aEnable;
}

void
nsGeolocationService::StopDevice()
{
  if(mDisconnectTimer) {
    mDisconnectTimer->Cancel();
    mDisconnectTimer = nullptr;
  }

  if (XRE_GetProcessType() == GeckoProcessType_Content) {
    ContentChild* cpc = ContentChild::GetSingleton();
    cpc->SendRemoveGeolocationListener();
    return; // bail early
  }

  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (!obs) {
    return;
  }

  for (int32_t i = 0; i < mProviders.Count(); i++) {
    mProviders[i]->Shutdown();
    obs->NotifyObservers(mProviders[i],
                         "geolocation-device-events",
                         NS_LITERAL_STRING("shutdown").get());
  }
}

nsRefPtr<nsGeolocationService> nsGeolocationService::sService;

already_AddRefed<nsGeolocationService>
nsGeolocationService::GetGeolocationService()
{
  nsRefPtr<nsGeolocationService> result;
  if (nsGeolocationService::sService) {
    result = nsGeolocationService::sService;
    return result.forget();
  }

  result = new nsGeolocationService();
  if (NS_FAILED(result->Init())) {
    return nullptr;
  }
  ClearOnShutdown(&nsGeolocationService::sService);
  nsGeolocationService::sService = result;
  return result.forget();
}

void
nsGeolocationService::AddLocator(nsGeolocation* aLocator)
{
  mGeolocators.AppendElement(aLocator);
}

void
nsGeolocationService::RemoveLocator(nsGeolocation* aLocator)
{
  mGeolocators.RemoveElement(aLocator);
}

////////////////////////////////////////////////////
// nsGeolocation
////////////////////////////////////////////////////

DOMCI_DATA(GeoGeolocation, nsGeolocation)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsGeolocation)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMGeoGeolocation)
  NS_INTERFACE_MAP_ENTRY(nsIDOMGeoGeolocation)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(GeoGeolocation)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsGeolocation)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsGeolocation)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsGeolocation)
  tmp->mPendingRequests.Clear();
  tmp->mPendingCallbacks.Clear();
  tmp->mWatchingCallbacks.Clear();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsGeolocation)
  uint32_t i;
  for (i = 0; i < tmp->mPendingRequests.Length(); ++i)
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPendingRequests[i].request)

  for (i = 0; i < tmp->mPendingCallbacks.Length(); ++i)
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPendingCallbacks[i])

  for (i = 0; i < tmp->mWatchingCallbacks.Length(); ++i)
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mWatchingCallbacks[i])
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

nsGeolocation::nsGeolocation()
: mLastWatchId(0)
{
}

nsGeolocation::~nsGeolocation()
{
  if (mService) {
    Shutdown();
  }
}

nsresult
nsGeolocation::Init(nsIDOMWindow* aContentDom)
{
  // Remember the window
  if (aContentDom) {
    nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(aContentDom);
    if (!window) {
      return NS_ERROR_FAILURE;
    }

    mOwner = do_GetWeakReference(window->GetCurrentInnerWindow());
    if (!mOwner) {
      return NS_ERROR_FAILURE;
    }

    // Grab the principal of the document
    nsCOMPtr<nsIDOMDocument> domdoc;
    aContentDom->GetDocument(getter_AddRefs(domdoc));
    nsCOMPtr<nsIDocument> doc = do_QueryInterface(domdoc);
    if (!doc) {
      return NS_ERROR_FAILURE;
    }

    mPrincipal = doc->NodePrincipal();
  }

  // If no aContentDom was passed into us, we are being used
  // by chrome/c++ and have no mOwner, no mPrincipal, and no need
  // to prompt.
  mService = nsGeolocationService::GetGeolocationService();
  if (mService) {
    mService->AddLocator(this);
  }
  return NS_OK;
}

void
nsGeolocation::Shutdown()
{
  // Shutdown and release all callbacks
  for (uint32_t i = 0; i< mPendingCallbacks.Length(); i++)
    mPendingCallbacks[i]->Shutdown();
  mPendingCallbacks.Clear();

  for (uint32_t i = 0; i< mWatchingCallbacks.Length(); i++)
    mWatchingCallbacks[i]->Shutdown();
  mWatchingCallbacks.Clear();

  if (mService) {
    mService->RemoveLocator(this);
  }

  mService = nullptr;
  mPrincipal = nullptr;
}

bool
nsGeolocation::HasActiveCallbacks()
{
  for (uint32_t i = 0; i < mWatchingCallbacks.Length(); i++) {
    if (mWatchingCallbacks[i]->IsActive()) {
      return true;
    }
  }

  return mPendingCallbacks.Length() != 0;
}

void
nsGeolocation::RemoveRequest(nsGeolocationRequest* aRequest)
{
  mPendingCallbacks.RemoveElement(aRequest);

  // if it is in the mWatchingCallbacks, we can't do much
  // since we passed back the position in the array to who
  // ever called WatchPosition() and we do not want to mess
  // around with the ordering of the array.  Instead, just
  // mark the request as "cleared".

  aRequest->MarkCleared();
}

void
nsGeolocation::Update(nsIDOMGeoPosition *aSomewhere, bool aIsBetter)
{
  if (!WindowOwnerStillExists()) {
    return Shutdown();
  }

  for (uint32_t i = mPendingCallbacks.Length(); i> 0; i--) {
    if (mPendingCallbacks[i-1]->Update(aSomewhere, aIsBetter)) {
      mPendingCallbacks.RemoveElementAt(i-1);
    }
  }

  // notify everyone that is watching
  for (uint32_t i = 0; i< mWatchingCallbacks.Length(); i++) {
    mWatchingCallbacks[i]->Update(aSomewhere, aIsBetter);
  }
}

NS_IMETHODIMP
nsGeolocation::GetCurrentPosition(nsIDOMGeoPositionCallback *callback,
                                  nsIDOMGeoPositionErrorCallback *errorCallback,
                                  const jsval& jsoptions,
                                  JSContext* cx)
{
  nsresult rv;
  nsAutoPtr<mozilla::dom::GeoPositionOptions> options(
      OptionsFromJSOptions(cx, jsoptions, &rv));
  NS_ENSURE_SUCCESS(rv, rv);
  return GetCurrentPosition(callback, errorCallback, options.forget());
}

nsresult
nsGeolocation::GetCurrentPosition(nsIDOMGeoPositionCallback *callback,
                                  nsIDOMGeoPositionErrorCallback *errorCallback,
                                  mozilla::dom::GeoPositionOptions *options)
{
  NS_ENSURE_ARG_POINTER(callback);

  if (mPendingCallbacks.Length() > MAX_GEO_REQUESTS_PER_WINDOW) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsRefPtr<nsGeolocationRequest> request = new nsGeolocationRequest(this,
                                                                    callback,
                                                                    errorCallback,
                                                                    options,
                                                                    false);

  if (!sGeoEnabled) {
    nsCOMPtr<nsIRunnable> ev = new RequestAllowEvent(false, request);
    NS_DispatchToMainThread(ev);
    return NS_OK;
  }

  if (!mOwner && !nsContentUtils::IsCallerChrome()) {
    return NS_ERROR_FAILURE;
  }

  mPendingCallbacks.AppendElement(request);

  if (sGeoInitPending) {
    PendingRequest req = { request, PendingRequest::GetCurrentPosition };
    mPendingRequests.AppendElement(req);
    return NS_OK;
  }

  return GetCurrentPositionReady(request);
}

nsresult
nsGeolocation::GetCurrentPositionReady(nsGeolocationRequest* aRequest)
{
  if (mOwner) {
    if (!RegisterRequestWithPrompt(aRequest)) {
      return NS_ERROR_NOT_AVAILABLE;
    }

    return NS_OK;
  }

  if (!nsContentUtils::IsCallerChrome()) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIRunnable> ev = new RequestAllowEvent(true, aRequest);
  NS_DispatchToMainThread(ev);

  return NS_OK;
}

NS_IMETHODIMP
nsGeolocation::WatchPosition(nsIDOMGeoPositionCallback *callback,
                             nsIDOMGeoPositionErrorCallback *errorCallback,
                             const jsval& jsoptions,
                             JSContext* cx,
                             int32_t *_retval)
{
  nsresult rv;
  nsAutoPtr<mozilla::dom::GeoPositionOptions> options(
      OptionsFromJSOptions(cx, jsoptions, &rv));
  NS_ENSURE_SUCCESS(rv, rv);
  return WatchPosition(callback, errorCallback, options.forget(), _retval);
}

nsresult
nsGeolocation::WatchPosition(nsIDOMGeoPositionCallback *callback,
                             nsIDOMGeoPositionErrorCallback *errorCallback,
                             mozilla::dom::GeoPositionOptions *options,
                             int32_t *_retval)
{
  NS_ENSURE_ARG_POINTER(callback);

  if (mWatchingCallbacks.Length() > MAX_GEO_REQUESTS_PER_WINDOW) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  // The watch ID:
  *_retval = mLastWatchId++;

  nsRefPtr<nsGeolocationRequest> request = new nsGeolocationRequest(this,
                                                                    callback,
                                                                    errorCallback,
                                                                    options,
                                                                    true,
                                                                    *_retval);

  if (!sGeoEnabled) {
    nsCOMPtr<nsIRunnable> ev = new RequestAllowEvent(false, request);

    // need to hand back an index/reference.
    mWatchingCallbacks.AppendElement(request);

    NS_DispatchToMainThread(ev);
    return NS_OK;
  }

  if (!mOwner && !nsContentUtils::IsCallerChrome()) {
    return NS_ERROR_FAILURE;
  }

  mWatchingCallbacks.AppendElement(request);

  if (sGeoInitPending) {
    PendingRequest req = { request, PendingRequest::WatchPosition };
    mPendingRequests.AppendElement(req);
    return NS_OK;
  }

  return WatchPositionReady(request);
}

nsresult
nsGeolocation::WatchPositionReady(nsGeolocationRequest* aRequest)
{
  if (mOwner) {
    if (!RegisterRequestWithPrompt(aRequest))
      return NS_ERROR_NOT_AVAILABLE;

    return NS_OK;
  }

  if (!nsContentUtils::IsCallerChrome()) {
    return NS_ERROR_FAILURE;
  }

  aRequest->Allow();

  return NS_OK;
}

NS_IMETHODIMP
nsGeolocation::ClearWatch(int32_t aWatchId)
{
  if (aWatchId < 0) {
    return NS_OK;
  }

  for (uint32_t i = 0, length = mWatchingCallbacks.Length(); i < length; ++i) {
    if (mWatchingCallbacks[i]->WatchId() == aWatchId) {
      mWatchingCallbacks[i]->MarkCleared();
      break;
    }
  }

  return NS_OK;
}

void
nsGeolocation::ServiceReady()
{
  for (uint32_t length = mPendingRequests.Length(); length > 0; --length) {
    switch (mPendingRequests[0].type) {
      case PendingRequest::GetCurrentPosition:
        GetCurrentPositionReady(mPendingRequests[0].request);
        break;

      case PendingRequest::WatchPosition:
        WatchPositionReady(mPendingRequests[0].request);
        break;
    }

    mPendingRequests.RemoveElementAt(0);
  }
}

bool
nsGeolocation::WindowOwnerStillExists()
{
  // an owner was never set when nsGeolocation
  // was created, which means that this object
  // is being used without a window.
  if (mOwner == nullptr) {
    return true;
  }

  nsCOMPtr<nsPIDOMWindow> window = do_QueryReferent(mOwner);

  if (window) {
    bool closed = false;
    window->GetClosed(&closed);
    if (closed) {
      return false;
    }

    nsPIDOMWindow* outer = window->GetOuterWindow();
    if (!outer || outer->GetCurrentInnerWindow() != window) {
      return false;
    }
  }

  return true;
}

bool
nsGeolocation::RegisterRequestWithPrompt(nsGeolocationRequest* request)
{
  if (Preferences::GetBool("geo.prompt.testing", false)) {
    bool allow = Preferences::GetBool("geo.prompt.testing.allow", false);
    nsCOMPtr<nsIRunnable> ev = new RequestAllowEvent(allow,
						     request);
    NS_DispatchToMainThread(ev);
    return true;
  }

  if (XRE_GetProcessType() == GeckoProcessType_Content) {
    nsCOMPtr<nsPIDOMWindow> window = do_QueryReferent(mOwner);
    if (!window) {
      return true;
    }

    // because owner implements nsITabChild, we can assume that it is
    // the one and only TabChild.
    TabChild* child = GetTabChildFrom(window->GetDocShell());
    if (!child) {
      return false;
    }

    // Retain a reference so the object isn't deleted without IPDL's knowledge.
    // Corresponding release occurs in DeallocPContentPermissionRequest.
    request->AddRef();
    child->SendPContentPermissionRequestConstructor(request,
                                                    NS_LITERAL_CSTRING("geolocation"),
                                                    NS_LITERAL_CSTRING("unused"),
                                                    IPC::Principal(mPrincipal));

    request->Sendprompt();
    return true;
  }

  nsCOMPtr<nsIRunnable> ev  = new RequestPromptEvent(request);
  NS_DispatchToMainThread(ev);
  return true;
}
