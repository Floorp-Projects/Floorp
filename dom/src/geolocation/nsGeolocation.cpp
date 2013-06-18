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
#include "nsCxPusher.h"
#include "nsIURI.h"
#include "nsIPermissionManager.h"
#include "nsIObserverService.h"
#include "nsThreadUtils.h"
#include "mozilla/Services.h"
#include "mozilla/unused.h"
#include "mozilla/Preferences.h"
#include "mozilla/Attributes.h"
#include "mozilla/ClearOnShutdown.h"

#include <math.h>
#include <algorithm>
#include <limits>

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

static mozilla::idl::GeoPositionOptions*
GeoPositionOptionsFromPositionOptions(const PositionOptions& aOptions)
{
  nsAutoPtr<mozilla::idl::GeoPositionOptions> geoOptions(
    new mozilla::idl::GeoPositionOptions());

  geoOptions->enableHighAccuracy = aOptions.mEnableHighAccuracy;
  geoOptions->maximumAge = aOptions.mMaximumAge;
  geoOptions->timeout = aOptions.mTimeout;

  return geoOptions.forget();
}

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

  NS_IMETHOD Handle(const nsAString& aName, const JS::Value& aResult)
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
                           Geolocation* aLocator)
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
  nsCOMPtr<nsIDOMGeoPosition> mPosition;
  nsRefPtr<nsGeolocationRequest> mRequest;
  nsRefPtr<Geolocation> mLocator;
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
// PositionError
////////////////////////////////////////////////////

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(PositionError)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMGeoPositionError)
  NS_INTERFACE_MAP_ENTRY(nsIDOMGeoPositionError)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_1(PositionError, mParent)
NS_IMPL_CYCLE_COLLECTING_ADDREF(PositionError)
NS_IMPL_CYCLE_COLLECTING_RELEASE(PositionError)

PositionError::PositionError(Geolocation* aParent, int16_t aCode)
  : mCode(aCode)
  , mParent(aParent)
{
  SetIsDOMBinding();
}

PositionError::~PositionError(){}


NS_IMETHODIMP
PositionError::GetCode(int16_t *aCode)
{
  NS_ENSURE_ARG_POINTER(aCode);
  *aCode = Code();
  return NS_OK;
}

Geolocation*
PositionError::GetParentObject() const
{
  return mParent;
}

JSObject*
PositionError::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return PositionErrorBinding::Wrap(aCx, aScope, this);
}

void
PositionError::NotifyCallback(const GeoPositionErrorCallback& aCallback)
{
  // Ensure that the proper context is on the stack (bug 452762)
  nsCxPusher pusher;
  pusher.PushNull();

  nsAutoMicroTask mt;
  if (aCallback.HasWebIDLCallback()) {
    PositionErrorCallback* callback = aCallback.GetWebIDLCallback();

    if (callback) {
      ErrorResult err;
      callback->Call(*this, err);
    }
  } else {
    nsIDOMGeoPositionErrorCallback* callback = aCallback.GetXPCOMCallback();
    if (callback) {
      callback->HandleEvent(this);
    }
  }
}
////////////////////////////////////////////////////
// nsGeolocationRequest
////////////////////////////////////////////////////

nsGeolocationRequest::nsGeolocationRequest(Geolocation* aLocator,
                                           const GeoPositionCallback& aCallback,
                                           const GeoPositionErrorCallback& aErrorCallback,
                                           mozilla::idl::GeoPositionOptions* aOptions,
                                           bool aWatchPositionRequest,
                                           int32_t aWatchId)
  : mAllowed(false),
    mCleared(false),
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
  MOZ_ASSERT(NS_IsMainThread());

  nsRefPtr<PositionError> positionError = new PositionError(mLocator, errorCode);
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
  }
  gs->SetHigherAccuracy(mOptions && mOptions->enableHighAccuracy);

  if (lastPosition && maximumAge > 0 &&
      ( PRTime(PR_Now() / PR_USEC_PER_MSEC) - maximumAge <=
        PRTime(cachedPositionTime) )) {
    // okay, we can return a cached position
    mAllowed = true;

    nsCOMPtr<nsIRunnable> ev =
      new RequestSendLocationEvent(
        lastPosition, this, mIsWatchPositionRequest ? nullptr : mLocator);

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

  nsRefPtr<Position> wrapped, cachedWrapper = mLocator->GetCachedPosition();
  if (cachedWrapper && aPosition == cachedWrapper->GetWrappedGeoPosition()) {
    wrapped = cachedWrapper;
  } else if (aPosition) {
    nsCOMPtr<nsIDOMGeoPositionCoords> coords;
    aPosition->GetCoords(getter_AddRefs(coords));
    if (coords) {
      wrapped = new Position(mLocator, aPosition);
    }
  }

  if (!wrapped) {
    NotifyError(nsIDOMGeoPositionError::POSITION_UNAVAILABLE);
    return;
  }

  mLocator->SetCachedPosition(wrapped);

  // Ensure that the proper context is on the stack (bug 452762)
  nsCxPusher pusher;
  pusher.PushNull();
  nsAutoMicroTask mt;
  if (mCallback.HasWebIDLCallback()) {
    ErrorResult err;
    PositionCallback* callback = mCallback.GetWebIDLCallback();

    MOZ_ASSERT(callback);
    callback->Call(*wrapped, err);
  } else {
    nsIDOMGeoPositionCallback* callback = mCallback.GetXPCOMCallback();

    MOZ_ASSERT(callback);
    callback->HandleEvent(aPosition);
  }

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
nsGeolocationRequest::Update(nsIDOMGeoPosition* aPosition)
{
  if (!mAllowed) {
    return false;
  }

  nsCOMPtr<nsIRunnable> ev = new RequestSendLocationEvent(aPosition,
                                                          this,
                                                          mIsWatchPositionRequest ? nullptr :  mLocator);
  NS_DispatchToMainThread(ev);
  return true;
}

void
nsGeolocationRequest::Shutdown()
{
  if (mTimeoutTimer) {
    mTimeoutTimer->Cancel();
    mTimeoutTimer = nullptr;
  }
  mCleared = true;

  // This should happen last, to ensure that this request isn't taken into consideration
  // when deciding whether existing requests still require high accuracy.
  if (mOptions && mOptions->enableHighAccuracy) {
    nsRefPtr<nsGeolocationService> gs = nsGeolocationService::GetGeolocationService();
    if (gs) {
      gs->SetHigherAccuracy(false);
    }
  }
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

#ifdef MOZ_MAEMO_LIBLOCATION
  mProvider = new MaemoLocationProvider();
#endif

#ifdef MOZ_ENABLE_QTMOBILITY
  mProvider = new QTMLocationProvider();
#endif

#ifdef MOZ_WIDGET_ANDROID
  mProvider = new AndroidLocationProvider();
#endif

#ifdef MOZ_WIDGET_GONK
  mProvider = do_GetService(GONK_GPS_GEOLOCATION_PROVIDER_CONTRACTID);
#endif

  nsCOMPtr<nsIGeolocationProvider> providerOveride = do_GetService(NS_GEOLOCATION_PROVIDER_CONTRACTID); 
  if (providerOveride) {
    mProvider = providerOveride;
  }

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

    AutoSafeJSContext cx;

    nsDependentString dataStr(aData);
    JS::Rooted<JS::Value> val(cx);
    if (!JS_ParseJSON(cx, dataStr.get(), dataStr.Length(), &val) || !val.isObject()) {
      return;
    }

    JS::Rooted<JSObject*> obj(cx, &val.toObject());
    JS::Rooted<JS::Value> key(cx);
    if (!JS_GetProperty(cx, obj, "key", key.address()) || !key.isString()) {
      return;
    }

    JSBool match;
    if (!JS_StringEqualsAscii(cx, key.toString(), GEO_SETINGS_ENABLED, &match) || (match != JS_TRUE)) {
      return;
    }

    JS::Rooted<JS::Value> value(cx);
    if (!JS_GetProperty(cx, obj, "value", value.address()) || !value.isBoolean()) {
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
  SetCachedPosition(aSomewhere);

  for (uint32_t i = 0; i< mGeolocators.Length(); i++) {
    mGeolocators[i]->Update(aSomewhere);
  }
  return NS_OK;
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
    cpc->SendAddGeolocationListener(IPC::Principal(aPrincipal),
                                    HighAccuracyRequested());
    return NS_OK;
  }

  // Start them up!
  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (!obs) {
    return NS_ERROR_FAILURE;
  }

  if (!mProvider) {
    return NS_ERROR_FAILURE;
  }

  mProvider->Startup();
  mProvider->Watch(this, aRequestPrivate);
  obs->NotifyObservers(mProvider,
                       "geolocation-device-events",
                       NS_LITERAL_STRING("starting").get());

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

bool
nsGeolocationService::HighAccuracyRequested()
{
  for (uint32_t i = 0; i < mGeolocators.Length(); i++) {
    if (mGeolocators[i]->HighAccuracyRequested()) {
      return true;
    }
  }
  return false;
}

void
nsGeolocationService::SetHigherAccuracy(bool aEnable)
{
  bool highRequired = aEnable || HighAccuracyRequested();

  if (XRE_GetProcessType() == GeckoProcessType_Content) {
    ContentChild* cpc = ContentChild::GetSingleton();
    cpc->SendSetGeolocationHigherAccuracy(highRequired);
    return;
  }

  if (!mHigherAccuracy && highRequired) {
      mProvider->SetHighAccuracy(true);
  }

  if (mHigherAccuracy && !highRequired) {
      mProvider->SetHighAccuracy(false);
  }

  mHigherAccuracy = highRequired;
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

  if (!mProvider) {
    return;
  }

  mHigherAccuracy = false;

  mProvider->Shutdown();
  obs->NotifyObservers(mProvider,
                       "geolocation-device-events",
                       NS_LITERAL_STRING("shutdown").get());
}

StaticRefPtr<nsGeolocationService> nsGeolocationService::sService;

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
nsGeolocationService::AddLocator(Geolocation* aLocator)
{
  mGeolocators.AppendElement(aLocator);
}

void
nsGeolocationService::RemoveLocator(Geolocation* aLocator)
{
  mGeolocators.RemoveElement(aLocator);
}

////////////////////////////////////////////////////
// Geolocation
////////////////////////////////////////////////////

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(Geolocation)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMGeoGeolocation)
  NS_INTERFACE_MAP_ENTRY(nsIDOMGeoGeolocation)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(Geolocation)
NS_IMPL_CYCLE_COLLECTING_RELEASE(Geolocation)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(Geolocation)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mCachedPosition)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
  tmp->mPendingRequests.Clear();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mPendingCallbacks)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mWatchingCallbacks)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(Geolocation)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mCachedPosition)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
  for (uint32_t i = 0; i < tmp->mPendingRequests.Length(); ++i)
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPendingRequests[i].request)

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPendingCallbacks)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mWatchingCallbacks)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END
NS_IMPL_CYCLE_COLLECTION_TRACE_WRAPPERCACHE(Geolocation)

Geolocation::Geolocation()
: mLastWatchId(0)
{
  SetIsDOMBinding();
}

Geolocation::~Geolocation()
{
  if (mService) {
    Shutdown();
  }
}

nsresult
Geolocation::Init(nsIDOMWindow* aContentDom)
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
Geolocation::Shutdown()
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

nsIDOMWindow*
Geolocation::GetParentObject() const {
  nsCOMPtr<nsPIDOMWindow> window = do_QueryReferent(mOwner);
  return window.get();
}

bool
Geolocation::HasActiveCallbacks()
{
  for (uint32_t i = 0; i < mWatchingCallbacks.Length(); i++) {
    if (mWatchingCallbacks[i]->IsActive()) {
      return true;
    }
  }

  return mPendingCallbacks.Length() != 0;
}

bool
Geolocation::HighAccuracyRequested()
{
  for (uint32_t i = 0; i < mWatchingCallbacks.Length(); i++) {
    if (mWatchingCallbacks[i]->IsActive() &&
        mWatchingCallbacks[i]->WantsHighAccuracy()) {
      return true;
    }
  }

  for (uint32_t i = 0; i < mPendingCallbacks.Length(); i++) {
    if (mPendingCallbacks[i]->IsActive() &&
        mPendingCallbacks[i]->WantsHighAccuracy()) {
      return true;
    }
  }

  return false;
}

void
Geolocation::RemoveRequest(nsGeolocationRequest* aRequest)
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
Geolocation::Update(nsIDOMGeoPosition *aSomewhere)
{
  if (!WindowOwnerStillExists()) {
    return Shutdown();
  }

  for (uint32_t i = mPendingCallbacks.Length(); i> 0; i--) {
    if (mPendingCallbacks[i-1]->Update(aSomewhere)) {
      mPendingCallbacks.RemoveElementAt(i-1);
    }
  }

  // notify everyone that is watching
  for (uint32_t i = 0; i< mWatchingCallbacks.Length(); i++) {
    mWatchingCallbacks[i]->Update(aSomewhere);
  }
}

void
Geolocation::SetCachedPosition(Position* aPosition)
{
  mCachedPosition = aPosition;
}

Position*
Geolocation::GetCachedPosition()
{
  return mCachedPosition;
}

void
Geolocation::GetCurrentPosition(PositionCallback& aCallback,
                                PositionErrorCallback* aErrorCallback,
                                const PositionOptions& aOptions,
                                ErrorResult& aRv)
{
  GeoPositionCallback successCallback(&aCallback);
  GeoPositionErrorCallback errorCallback(aErrorCallback);

  nsresult rv =
    GetCurrentPosition(successCallback, errorCallback,
                       GeoPositionOptionsFromPositionOptions(aOptions));

  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
  }

  return;
}

NS_IMETHODIMP
Geolocation::GetCurrentPosition(nsIDOMGeoPositionCallback* aCallback,
                                nsIDOMGeoPositionErrorCallback* aErrorCallback,
                                mozilla::idl::GeoPositionOptions* aOptions)
{
  NS_ENSURE_ARG_POINTER(aCallback);

  GeoPositionCallback successCallback(aCallback);
  GeoPositionErrorCallback errorCallback(aErrorCallback);

  return GetCurrentPosition(successCallback, errorCallback, aOptions);
}

nsresult
Geolocation::GetCurrentPosition(GeoPositionCallback& callback,
                                GeoPositionErrorCallback& errorCallback,
                                mozilla::idl::GeoPositionOptions *options)
{
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
Geolocation::GetCurrentPositionReady(nsGeolocationRequest* aRequest)
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

int32_t
Geolocation::WatchPosition(PositionCallback& aCallback,
                           PositionErrorCallback* aErrorCallback,
                           const PositionOptions& aOptions,
                           ErrorResult& aRv)
{
  int32_t ret;
  GeoPositionCallback successCallback(&aCallback);
  GeoPositionErrorCallback errorCallback(aErrorCallback);

  nsresult rv =
    WatchPosition(successCallback, errorCallback,
                  GeoPositionOptionsFromPositionOptions(aOptions), &ret);

  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
  }

  return ret;
}

NS_IMETHODIMP
Geolocation::WatchPosition(nsIDOMGeoPositionCallback *aCallback,
                           nsIDOMGeoPositionErrorCallback *aErrorCallback,
                           mozilla::idl::GeoPositionOptions *aOptions,
                           int32_t* aRv)
{
  NS_ENSURE_ARG_POINTER(aCallback);

  GeoPositionCallback successCallback(aCallback);
  GeoPositionErrorCallback errorCallback(aErrorCallback);

  return WatchPosition(successCallback, errorCallback, aOptions, aRv);
}

nsresult
Geolocation::WatchPosition(GeoPositionCallback& aCallback,
                           GeoPositionErrorCallback& aErrorCallback,
                           mozilla::idl::GeoPositionOptions* aOptions,
                           int32_t* aRv)
{
  if (mWatchingCallbacks.Length() > MAX_GEO_REQUESTS_PER_WINDOW) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  // The watch ID:
  *aRv = mLastWatchId++;

  nsRefPtr<nsGeolocationRequest> request = new nsGeolocationRequest(this,
                                                                    aCallback,
                                                                    aErrorCallback,
                                                                    aOptions,
                                                                    true,
                                                                    *aRv);

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
Geolocation::WatchPositionReady(nsGeolocationRequest* aRequest)
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
Geolocation::ClearWatch(int32_t aWatchId)
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
Geolocation::ServiceReady()
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
Geolocation::WindowOwnerStillExists()
{
  // an owner was never set when Geolocation
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
Geolocation::RegisterRequestWithPrompt(nsGeolocationRequest* request)
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

JSObject*
Geolocation::WrapObject(JSContext *aCtx, JS::Handle<JSObject*> aScope)
{
  return mozilla::dom::GeolocationBinding::Wrap(aCtx, aScope, this);
}
