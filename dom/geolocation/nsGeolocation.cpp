/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsXULAppAPI.h"

#include "mozilla/dom/ContentChild.h"
#include "mozilla/Telemetry.h"

#include "nsISettingsService.h"

#include "nsGeolocation.h"
#include "nsGeoGridFuzzer.h"
#include "nsGeolocationSettings.h"
#include "nsDOMClassInfoID.h"
#include "nsComponentManagerUtils.h"
#include "nsServiceManagerUtils.h"
#include "nsContentUtils.h"
#include "nsContentPermissionHelper.h"
#include "nsIDocument.h"
#include "nsIDOMEvent.h"
#include "nsIObserverService.h"
#include "nsPIDOMWindow.h"
#include "nsThreadUtils.h"
#include "mozilla/HalWakeLock.h"
#include "mozilla/Hal.h"
#include "mozilla/Services.h"
#include "mozilla/unused.h"
#include "mozilla/Preferences.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/dom/Event.h"
#include "mozilla/WeakPtr.h"
#include "mozilla/dom/PermissionMessageUtils.h"
#include "mozilla/dom/SettingChangeNotificationBinding.h"
#include "mozilla/dom/WakeLock.h"

#include "nsJSUtils.h"
#include "prdtoa.h"

class nsIPrincipal;

#ifdef MOZ_ENABLE_QT5GEOPOSITION
#include "QTMLocationProvider.h"
#endif

#ifdef MOZ_WIDGET_ANDROID
#include "AndroidLocationProvider.h"
#endif

#ifdef MOZ_WIDGET_GONK
#include "GonkGPSGeolocationProvider.h"
#endif

#ifdef MOZ_WIDGET_COCOA
#include "CoreLocationLocationProvider.h"
#endif

#ifdef XP_WIN
#include "WindowsLocationProvider.h"
#include "mozilla/WindowsVersion.h"
#endif

// Some limit to the number of get or watch geolocation requests
// that a window can make.
#define MAX_GEO_REQUESTS_PER_WINDOW  1500

// the geolocation enabled setting
#define GEO_SETTINGS_ENABLED          "geolocation.enabled"

using mozilla::Unused;          // <snicker>
using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::hal;

class nsGeolocationRequest final
 : public nsIContentPermissionRequest
 , public nsIGeolocationUpdate
 , public SupportsWeakPtr<nsGeolocationRequest>
{
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_NSICONTENTPERMISSIONREQUEST
  NS_DECL_NSIGEOLOCATIONUPDATE

  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(nsGeolocationRequest, nsIContentPermissionRequest)

  nsGeolocationRequest(Geolocation* aLocator,
                       const GeoPositionCallback& aCallback,
                       const GeoPositionErrorCallback& aErrorCallback,
                       PositionOptions* aOptions,
                       uint8_t aProtocolType,
                       bool aWatchPositionRequest = false,
                       int32_t aWatchId = 0);

  MOZ_DECLARE_WEAKREFERENCE_TYPENAME(nsGeolocationRequest)

  void Shutdown();

  void SendLocation(nsIDOMGeoPosition* aLocation);
  bool WantsHighAccuracy() {return !mShutdown && mOptions && mOptions->mEnableHighAccuracy;}
  void SetTimeoutTimer();
  void StopTimeoutTimer();
  void NotifyErrorAndShutdown(uint16_t);
  nsIPrincipal* GetPrincipal();

  bool IsWatch() { return mIsWatchPositionRequest; }
  int32_t WatchId() { return mWatchId; }
 private:
  virtual ~nsGeolocationRequest();

  class TimerCallbackHolder final : public nsITimerCallback
  {
  public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSITIMERCALLBACK

    explicit TimerCallbackHolder(nsGeolocationRequest* aRequest)
      : mRequest(aRequest)
    {}

  private:
    ~TimerCallbackHolder() {}
    WeakPtr<nsGeolocationRequest> mRequest;
  };

  void Notify();

  already_AddRefed<nsIDOMGeoPosition> AdjustedLocation(nsIDOMGeoPosition*);

  bool mIsWatchPositionRequest;

  nsCOMPtr<nsITimer> mTimeoutTimer;
  GeoPositionCallback mCallback;
  GeoPositionErrorCallback mErrorCallback;
  nsAutoPtr<PositionOptions> mOptions;

  RefPtr<Geolocation> mLocator;

  int32_t mWatchId;
  bool mShutdown;
  nsCOMPtr<nsIContentPermissionRequester> mRequester;
  uint8_t mProtocolType;
};

static PositionOptions*
CreatePositionOptionsCopy(const PositionOptions& aOptions)
{
  nsAutoPtr<PositionOptions> geoOptions(new PositionOptions());

  geoOptions->mEnableHighAccuracy = aOptions.mEnableHighAccuracy;
  geoOptions->mMaximumAge = aOptions.mMaximumAge;
  geoOptions->mTimeout = aOptions.mTimeout;

  return geoOptions.forget();
}

class GeolocationSettingsCallback : public nsISettingsServiceCallback
{
  virtual ~GeolocationSettingsCallback() {
    MOZ_COUNT_DTOR(GeolocationSettingsCallback);
  }

public:
  NS_DECL_ISUPPORTS

  GeolocationSettingsCallback() {
    MOZ_COUNT_CTOR(GeolocationSettingsCallback);
  }

  NS_IMETHOD Handle(const nsAString& aName, JS::Handle<JS::Value> aResult) override
  {
    MOZ_ASSERT(NS_IsMainThread());

    if (aName.EqualsASCII(GEO_SETTINGS_ENABLED)) {
      // The geolocation is enabled by default:
      bool value = true;
      if (aResult.isBoolean()) {
        value = aResult.toBoolean();
      }

      GPSLOG("%s set to %s",
             NS_ConvertUTF16toUTF8(aName).get(),
             (value ? "ENABLED" : "DISABLED"));
      MozSettingValue(value);

    } else {
      RefPtr<nsGeolocationSettings> gs = nsGeolocationSettings::GetGeolocationSettings();
      if (gs) {
        gs->HandleGeolocationSettingsChange(aName, aResult);
      }
    }

    return NS_OK;
  }

  NS_IMETHOD HandleError(const nsAString& aName) override
  {
    if (aName.EqualsASCII(GEO_SETTINGS_ENABLED)) {
      GPSLOG("Unable to get value for '" GEO_SETTINGS_ENABLED "'");

      // Default it's enabled:
      MozSettingValue(true);
    } else {
      RefPtr<nsGeolocationSettings> gs = nsGeolocationSettings::GetGeolocationSettings();
      if (gs) {
        gs->HandleGeolocationSettingsError(aName);
      }
    }

    return NS_OK;
  }

  void MozSettingValue(const bool aValue)
  {
    RefPtr<nsGeolocationService> gs = nsGeolocationService::GetGeolocationService();
    if (gs) {
      gs->HandleMozsettingValue(aValue);
    }
  }
};

NS_IMPL_ISUPPORTS(GeolocationSettingsCallback, nsISettingsServiceCallback)

class RequestPromptEvent : public nsRunnable
{
public:
  RequestPromptEvent(nsGeolocationRequest* aRequest, nsWeakPtr aWindow)
    : mRequest(aRequest)
    , mWindow(aWindow)
  {
  }

  NS_IMETHOD Run()
  {
    nsCOMPtr<nsPIDOMWindowInner> window = do_QueryReferent(mWindow);
    nsContentPermissionUtils::AskPermission(mRequest, window);
    return NS_OK;
  }

private:
  RefPtr<nsGeolocationRequest> mRequest;
  nsWeakPtr mWindow;
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
      mRequest->Allow(JS::UndefinedHandleValue);
    } else {
      mRequest->Cancel();
    }
    return NS_OK;
  }

private:
  bool mAllow;
  RefPtr<nsGeolocationRequest> mRequest;
};

class RequestSendLocationEvent : public nsRunnable
{
public:
  RequestSendLocationEvent(nsIDOMGeoPosition* aPosition,
                           nsGeolocationRequest* aRequest)
    : mPosition(aPosition),
      mRequest(aRequest)
  {
  }

  NS_IMETHOD Run() {
    mRequest->SendLocation(mPosition);
    return NS_OK;
  }

private:
  nsCOMPtr<nsIDOMGeoPosition> mPosition;
  RefPtr<nsGeolocationRequest> mRequest;
  RefPtr<Geolocation> mLocator;
};

////////////////////////////////////////////////////
// PositionError
////////////////////////////////////////////////////

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(PositionError)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMGeoPositionError)
  NS_INTERFACE_MAP_ENTRY(nsIDOMGeoPositionError)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(PositionError, mParent)
NS_IMPL_CYCLE_COLLECTING_ADDREF(PositionError)
NS_IMPL_CYCLE_COLLECTING_RELEASE(PositionError)

PositionError::PositionError(Geolocation* aParent, int16_t aCode)
  : mCode(aCode)
  , mParent(aParent)
{
}

PositionError::~PositionError(){}


NS_IMETHODIMP
PositionError::GetCode(int16_t *aCode)
{
  NS_ENSURE_ARG_POINTER(aCode);
  *aCode = Code();
  return NS_OK;
}

NS_IMETHODIMP
PositionError::GetMessage(nsAString& aMessage)
{
  switch (mCode)
  {
    case nsIDOMGeoPositionError::PERMISSION_DENIED:
      aMessage = NS_LITERAL_STRING("User denied geolocation prompt");
      break;
    case nsIDOMGeoPositionError::POSITION_UNAVAILABLE:
      aMessage = NS_LITERAL_STRING("Unknown error acquiring position");
      break;
    case nsIDOMGeoPositionError::TIMEOUT:
      aMessage = NS_LITERAL_STRING("Position acquisition timed out");
      break;
    default:
      break;
  }
  return NS_OK;
}

Geolocation*
PositionError::GetParentObject() const
{
  return mParent;
}

JSObject*
PositionError::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return PositionErrorBinding::Wrap(aCx, this, aGivenProto);
}

void
PositionError::NotifyCallback(const GeoPositionErrorCallback& aCallback)
{
  nsAutoMicroTask mt;
  if (aCallback.HasWebIDLCallback()) {
    PositionErrorCallback* callback = aCallback.GetWebIDLCallback();

    if (callback) {
      callback->Call(*this);
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
                                           PositionOptions* aOptions,
                                           uint8_t aProtocolType,
                                           bool aWatchPositionRequest,
                                           int32_t aWatchId)
  : mIsWatchPositionRequest(aWatchPositionRequest),
    mCallback(aCallback),
    mErrorCallback(aErrorCallback),
    mOptions(aOptions),
    mLocator(aLocator),
    mWatchId(aWatchId),
    mShutdown(false),
    mProtocolType(aProtocolType)
{
  if (nsCOMPtr<nsPIDOMWindowInner> win =
      do_QueryReferent(mLocator->GetOwner())) {
    mRequester = new nsContentPermissionRequester(win);
  }
}

nsGeolocationRequest::~nsGeolocationRequest()
{
  StopTimeoutTimer();
}

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsGeolocationRequest)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIContentPermissionRequest)
  NS_INTERFACE_MAP_ENTRY(nsIContentPermissionRequest)
  NS_INTERFACE_MAP_ENTRY(nsIGeolocationUpdate)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsGeolocationRequest)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsGeolocationRequest)
NS_IMPL_CYCLE_COLLECTION(nsGeolocationRequest, mCallback, mErrorCallback, mLocator)
void
nsGeolocationRequest::Notify()
{
  SetTimeoutTimer();
  NotifyErrorAndShutdown(nsIDOMGeoPositionError::TIMEOUT);
}
void
nsGeolocationRequest::NotifyErrorAndShutdown(uint16_t aErrorCode)
{
  MOZ_ASSERT(!mShutdown, "timeout after shutdown");
  if (!mIsWatchPositionRequest) {
    Shutdown();
    mLocator->RemoveRequest(this);
  }

  NotifyError(aErrorCode);
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
nsGeolocationRequest::GetTypes(nsIArray** aTypes)
{
  nsTArray<nsString> emptyOptions;
  return nsContentPermissionUtils::CreatePermissionArray(NS_LITERAL_CSTRING("geolocation"),
                                                         NS_LITERAL_CSTRING("unused"),
                                                         emptyOptions,
                                                         aTypes);
}

NS_IMETHODIMP
nsGeolocationRequest::GetWindow(mozIDOMWindow** aRequestingWindow)
{
  NS_ENSURE_ARG_POINTER(aRequestingWindow);

  nsCOMPtr<nsPIDOMWindowInner> window = do_QueryReferent(mLocator->GetOwner());
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
  if (mRequester) {
    // Record the number of denied requests for regular web content.
    // This method is only called when the user explicitly denies the request,
    // and is not called when the page is simply unloaded, or similar.
    Telemetry::Accumulate(Telemetry::GEOLOCATION_REQUEST_GRANTED, mProtocolType);
  }

  if (mLocator->ClearPendingRequest(this)) {
    return NS_OK;
  }

  NotifyError(nsIDOMGeoPositionError::PERMISSION_DENIED);
  return NS_OK;
}

NS_IMETHODIMP
nsGeolocationRequest::Allow(JS::HandleValue aChoices)
{
  MOZ_ASSERT(aChoices.isUndefined());

  if (mRequester) {
    // Record the number of granted requests for regular web content.
    Telemetry::Accumulate(Telemetry::GEOLOCATION_REQUEST_GRANTED, mProtocolType + 10);
  }

  if (mLocator->ClearPendingRequest(this)) {
    return NS_OK;
  }

  RefPtr<nsGeolocationService> gs = nsGeolocationService::GetGeolocationService();

  bool canUseCache = false;
  CachedPositionAndAccuracy lastPosition = gs->GetCachedPosition();
  if (lastPosition.position) {
    DOMTimeStamp cachedPositionTime_ms;
    lastPosition.position->GetTimestamp(&cachedPositionTime_ms);
    // check to see if we can use a cached value
    // if the user has specified a maximumAge, return a cached value.
    if (mOptions && mOptions->mMaximumAge > 0) {
      uint32_t maximumAge_ms = mOptions->mMaximumAge;
      bool isCachedWithinRequestedAccuracy = WantsHighAccuracy() <= lastPosition.isHighAccuracy;
      bool isCachedWithinRequestedTime =
        DOMTimeStamp(PR_Now() / PR_USEC_PER_MSEC - maximumAge_ms) <= cachedPositionTime_ms;
      canUseCache = isCachedWithinRequestedAccuracy && isCachedWithinRequestedTime;
    }
  }

  gs->UpdateAccuracy(WantsHighAccuracy());
  if (canUseCache) {
    // okay, we can return a cached position
    // getCurrentPosition requests serviced by the cache
    // will now be owned by the RequestSendLocationEvent
    Update(lastPosition.position);

    // After Update is called, getCurrentPosition finishes it's job.
    if (!mIsWatchPositionRequest) {
      return NS_OK;
    }

  } else {
    // if it is not a watch request and timeout is 0,
    // invoke the errorCallback (if present) with TIMEOUT code
    if (mOptions && mOptions->mTimeout == 0 && !mIsWatchPositionRequest) {
      NotifyError(nsIDOMGeoPositionError::TIMEOUT);
      return NS_OK;
    }

  }

  // Kick off the geo device, if it isn't already running
  nsresult rv = gs->StartDevice(GetPrincipal());

  if (NS_FAILED(rv)) {
    // Location provider error
    NotifyError(nsIDOMGeoPositionError::POSITION_UNAVAILABLE);
    return NS_OK;
  }

  if (mLocator->ContainsRequest(this)) {
    return NS_OK;
  }

  if (mIsWatchPositionRequest || !canUseCache) {
    // let the locator know we're pending
    // we will now be owned by the locator
    mLocator->NotifyAllowedRequest(this);
  }

  SetTimeoutTimer();

  return NS_OK;
}

NS_IMETHODIMP
nsGeolocationRequest::GetRequester(nsIContentPermissionRequester** aRequester)
{
  NS_ENSURE_ARG_POINTER(aRequester);

  nsCOMPtr<nsIContentPermissionRequester> requester = mRequester;
  requester.forget(aRequester);
  return NS_OK;
}

void
nsGeolocationRequest::SetTimeoutTimer()
{
  StopTimeoutTimer();

  int32_t timeout;
  if (mOptions && (timeout = mOptions->mTimeout) != 0) {

    if (timeout < 0) {
      timeout = 0;
    } else if (timeout < 10) {
      timeout = 10;
    }

    mTimeoutTimer = do_CreateInstance("@mozilla.org/timer;1");
    RefPtr<TimerCallbackHolder> holder = new TimerCallbackHolder(this);
    mTimeoutTimer->InitWithCallback(holder, timeout, nsITimer::TYPE_ONE_SHOT);
  }
}

void
nsGeolocationRequest::StopTimeoutTimer()
{
  if (mTimeoutTimer) {
    mTimeoutTimer->Cancel();
    mTimeoutTimer = nullptr;
  }
}

static already_AddRefed<nsIDOMGeoPosition>
SynthesizeLocation(DOMTimeStamp aTimestamp, double aLatitude, double aLongitude)
{
  // return a position at sea level, N heading, 0 speed, 0 error.
  RefPtr<nsGeoPosition> pos = new nsGeoPosition(aLatitude, aLongitude,
                                                  0.0, 0.0, 0.0, 0.0, 0.0,
                                                  aTimestamp);
  return pos.forget();
}


already_AddRefed<nsIDOMGeoPosition>
nsGeolocationRequest::AdjustedLocation(nsIDOMGeoPosition *aPosition)
{
  nsCOMPtr<nsIDOMGeoPosition> pos = aPosition;
  if (XRE_IsContentProcess()) {
    GPSLOG("child process just copying position");
    return pos.forget();
  }

  // get the settings cache
  RefPtr<nsGeolocationSettings> gs = nsGeolocationSettings::GetGeolocationSettings();
  if (!gs) {
    return pos.forget();
  }

  // make sure ALA is enabled
  if (!gs->IsAlaEnabled()) {
    GPSLOG("ALA is disabled, returning precise location");
    return pos.forget();
  }

  // look up the geolocation settings via the watch ID
  DOMTimeStamp ts(PR_Now() / PR_USEC_PER_MSEC);
  GeolocationSetting setting = gs->LookupGeolocationSetting(mWatchId);
  switch (setting.GetType()) {
    case GEO_ALA_TYPE_PRECISE:
      GPSLOG("returning precise location watch ID: %d", mWatchId);
      return pos.forget();
#ifdef MOZ_APPROX_LOCATION
    case GEO_ALA_TYPE_APPROX:
      GPSLOG("returning approximate location for watch ID: %d", mWatchId);
      return nsGeoGridFuzzer::FuzzLocation(setting, aPosition);
#endif
    case GEO_ALA_TYPE_FIXED:
      GPSLOG("returning fixed location for watch ID:: %d", mWatchId);
      // use "now" as time stamp
      return SynthesizeLocation(ts, setting.GetFixedLatitude(),
                                setting.GetFixedLongitude());
    case GEO_ALA_TYPE_NONE:
      GPSLOG("returning no location for watch ID: %d", mWatchId);
      // return nullptr so no location callback happens
      return nullptr;
  }

  return nullptr;
}


void
nsGeolocationRequest::SendLocation(nsIDOMGeoPosition* aPosition)
{
  if (mShutdown) {
    // Ignore SendLocationEvents issued before we were cleared.
    return;
  }

  if (mOptions && mOptions->mMaximumAge > 0) {
    DOMTimeStamp positionTime_ms;
    aPosition->GetTimestamp(&positionTime_ms);
    const uint32_t maximumAge_ms = mOptions->mMaximumAge;
    const bool isTooOld =
        DOMTimeStamp(PR_Now() / PR_USEC_PER_MSEC - maximumAge_ms) > positionTime_ms;
    if (isTooOld) {
      return;
    }
  }

  RefPtr<Position> wrapped;

  if (aPosition) {
    nsCOMPtr<nsIDOMGeoPositionCoords> coords;
    aPosition->GetCoords(getter_AddRefs(coords));
    if (coords) {
#ifdef MOZ_GPS_DEBUG
      double lat = 0.0, lon = 0.0;
      coords->GetLatitude(&lat);
      coords->GetLongitude(&lon);
      GPSLOG("returning coordinates: %f, %f", lat, lon);
#endif
      wrapped = new Position(ToSupports(mLocator), aPosition);
    }
  }

  if (!wrapped) {
    NotifyError(nsIDOMGeoPositionError::POSITION_UNAVAILABLE);
    return;
  }

  if (!mIsWatchPositionRequest) {
    // Cancel timer and position updates in case the position
    // callback spins the event loop
    Shutdown();
  }

  nsAutoMicroTask mt;
  if (mCallback.HasWebIDLCallback()) {
    PositionCallback* callback = mCallback.GetWebIDLCallback();

    MOZ_ASSERT(callback);
    callback->Call(*wrapped);
  } else {
    nsIDOMGeoPositionCallback* callback = mCallback.GetXPCOMCallback();
    MOZ_ASSERT(callback);
    callback->HandleEvent(aPosition);
  }
  SetTimeoutTimer();
  MOZ_ASSERT(mShutdown || mIsWatchPositionRequest,
             "non-shutdown getCurrentPosition request after callback!");
}
nsIPrincipal*
nsGeolocationRequest::GetPrincipal()
{
  if (!mLocator) {
    return nullptr;
  }
  return mLocator->GetPrincipal();
}

NS_IMETHODIMP
nsGeolocationRequest::Update(nsIDOMGeoPosition* aPosition)
{
  nsCOMPtr<nsIDOMGeoPosition> pos = AdjustedLocation(aPosition);
  nsCOMPtr<nsIRunnable> ev = new RequestSendLocationEvent(pos, this);
  NS_DispatchToMainThread(ev);
  return NS_OK;
}
NS_IMETHODIMP
nsGeolocationRequest::NotifyError(uint16_t aErrorCode)
{
  MOZ_ASSERT(NS_IsMainThread());
  RefPtr<PositionError> positionError = new PositionError(mLocator, aErrorCode);
  positionError->NotifyCallback(mErrorCallback);
  return NS_OK;
}

void
nsGeolocationRequest::Shutdown()
{
  MOZ_ASSERT(!mShutdown, "request shutdown twice");
  mShutdown = true;

  StopTimeoutTimer();

  // If there are no other high accuracy requests, the geolocation service will
  // notify the provider to switch to the default accuracy.
  if (mOptions && mOptions->mEnableHighAccuracy) {
    RefPtr<nsGeolocationService> gs = nsGeolocationService::GetGeolocationService();
    if (gs) {
      gs->UpdateAccuracy();
    }
  }
}


////////////////////////////////////////////////////
// nsGeolocationRequest::TimerCallbackHolder
////////////////////////////////////////////////////

NS_IMPL_ISUPPORTS(nsGeolocationRequest::TimerCallbackHolder, nsISupports, nsITimerCallback)

NS_IMETHODIMP
nsGeolocationRequest::TimerCallbackHolder::Notify(nsITimer*)
{
  if (mRequest) {
    RefPtr<nsGeolocationRequest> request(mRequest);
    request->Notify();
  }
  return NS_OK;
}


////////////////////////////////////////////////////
// nsGeolocationService
////////////////////////////////////////////////////
NS_INTERFACE_MAP_BEGIN(nsGeolocationService)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIGeolocationUpdate)
  NS_INTERFACE_MAP_ENTRY(nsIGeolocationUpdate)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(nsGeolocationService)
NS_IMPL_RELEASE(nsGeolocationService)


static bool sGeoEnabled = true;
static bool sGeoInitPending = true;
static int32_t sProviderTimeout = 6000; // Time, in milliseconds, to wait for the location provider to spin up.

nsresult nsGeolocationService::Init()
{
  Preferences::AddIntVarCache(&sProviderTimeout, "geo.timeout", sProviderTimeout);
  Preferences::AddBoolVarCache(&sGeoEnabled, "geo.enabled", sGeoEnabled);

  if (!sGeoEnabled) {
    return NS_ERROR_FAILURE;
  }

  if (XRE_IsContentProcess()) {
    sGeoInitPending = false;
    return NS_OK;
  }

  // check if the geolocation service is enable from settings
  nsCOMPtr<nsISettingsService> settings =
    do_GetService("@mozilla.org/settingsService;1");

  if (settings) {
    nsCOMPtr<nsISettingsServiceLock> settingsLock;
    nsresult rv = settings->CreateLock(nullptr, getter_AddRefs(settingsLock));
    NS_ENSURE_SUCCESS(rv, rv);

    RefPtr<GeolocationSettingsCallback> callback = new GeolocationSettingsCallback();
    rv = settingsLock->Get(GEO_SETTINGS_ENABLED, callback);
    NS_ENSURE_SUCCESS(rv, rv);

    // look up the geolocation settings
    callback = new GeolocationSettingsCallback();
    rv = settingsLock->Get(GEO_ALA_ENABLED, callback);
    NS_ENSURE_SUCCESS(rv, rv);
    callback = new GeolocationSettingsCallback();
    rv = settingsLock->Get(GEO_ALA_TYPE, callback);
    NS_ENSURE_SUCCESS(rv, rv);
#ifdef MOZ_APPROX_LOCATION
    callback = new GeolocationSettingsCallback();
    rv = settingsLock->Get(GEO_ALA_APPROX_DISTANCE, callback);
    NS_ENSURE_SUCCESS(rv, rv);
#endif
    callback = new GeolocationSettingsCallback();
    rv = settingsLock->Get(GEO_ALA_FIXED_COORDS, callback);
    NS_ENSURE_SUCCESS(rv, rv);
    callback = new GeolocationSettingsCallback();
    rv = settingsLock->Get(GEO_ALA_APP_SETTINGS, callback);
    NS_ENSURE_SUCCESS(rv, rv);
    callback = new GeolocationSettingsCallback();
    rv = settingsLock->Get(GEO_ALA_ALWAYS_PRECISE, callback);
    NS_ENSURE_SUCCESS(rv, rv);

  } else {
    // If we cannot obtain the settings service, we continue
    // assuming that the geolocation is enabled:
    sGeoInitPending = false;
  }

  // geolocation service can be enabled -> now register observer
  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  if (!obs) {
    return NS_ERROR_FAILURE;
  }

  obs->AddObserver(this, "xpcom-shutdown", false);
  obs->AddObserver(this, "mozsettings-changed", false);

#ifdef MOZ_ENABLE_QT5GEOPOSITION
  mProvider = new QTMLocationProvider();
#endif

#ifdef MOZ_WIDGET_ANDROID
  mProvider = new AndroidLocationProvider();
#endif

#ifdef MOZ_WIDGET_GONK
  // GonkGPSGeolocationProvider can be started at boot up time for initialization reasons.
  // do_getService gets hold of the already initialized component and starts
  // processing location requests immediately.
  // do_Createinstance will create multiple instances of the provider which is not right.
  // bug 993041
  mProvider = do_GetService(GONK_GPS_GEOLOCATION_PROVIDER_CONTRACTID);
#endif

#ifdef MOZ_WIDGET_COCOA
  if (Preferences::GetBool("geo.provider.use_corelocation", true)) {
    mProvider = new CoreLocationLocationProvider();
  }
#endif

#ifdef XP_WIN
  if (Preferences::GetBool("geo.provider.ms-windows-location", false) &&
      IsWin8OrLater()) {
    mProvider = new WindowsLocationProvider();
  }
#endif

  if (Preferences::GetBool("geo.provider.use_mls", false)) {
    mProvider = do_CreateInstance("@mozilla.org/geolocation/mls-provider;1");
  }

  // Override platform-specific providers with the default (network)
  // provider while testing. Our tests are currently not meant to exercise
  // the provider, and some tests rely on the network provider being used.
  // "geo.provider.testing" is always set for all plain and browser chrome
  // mochitests, and also for xpcshell tests.
  if (!mProvider || Preferences::GetBool("geo.provider.testing", false)) {
    nsCOMPtr<nsIGeolocationProvider> override =
      do_GetService(NS_GEOLOCATION_PROVIDER_CONTRACTID);

    if (override) {
      mProvider = override;
    }
  }

  return NS_OK;
}

nsGeolocationService::~nsGeolocationService()
{
}

void
nsGeolocationService::HandleMozsettingChanged(nsISupports* aSubject)
{
    // The string that we're interested in will be a JSON string that looks like:
    //  {"key":"gelocation.enabled","value":true}

    RootedDictionary<SettingChangeNotification> setting(nsContentUtils::RootingCxForThread());
    if (!WrappedJSToDictionary(aSubject, setting)) {
      return;
    }
    if (!setting.mKey.EqualsASCII(GEO_SETTINGS_ENABLED)) {
      return;
    }
    if (!setting.mValue.isBoolean()) {
      return;
    }

    GPSLOG("mozsetting changed: %s == %s",
          NS_ConvertUTF16toUTF8(setting.mKey).get(),
          (setting.mValue.toBoolean() ? "TRUE" : "FALSE"));

    HandleMozsettingValue(setting.mValue.toBoolean());
}

void
nsGeolocationService::HandleMozsettingValue(const bool aValue)
{
    if (!aValue) {
      // turn things off
      StopDevice();
      Update(nullptr);
      mLastPosition.position = nullptr;
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
                              const char16_t* aData)
{
  if (!strcmp("xpcom-shutdown", aTopic)) {
    nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
    if (obs) {
      obs->RemoveObserver(this, "xpcom-shutdown");
      obs->RemoveObserver(this, "mozsettings-changed");
    }

    for (uint32_t i = 0; i< mGeolocators.Length(); i++) {
      mGeolocators[i]->Shutdown();
    }
    StopDevice();

    return NS_OK;
  }

  if (!strcmp("mozsettings-changed", aTopic)) {
    HandleMozsettingChanged(aSubject);
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
  if (aSomewhere) {
    SetCachedPosition(aSomewhere);
  }
  for (uint32_t i = 0; i< mGeolocators.Length(); i++) {
    mGeolocators[i]->Update(aSomewhere);
  }
  return NS_OK;
}
NS_IMETHODIMP
nsGeolocationService::NotifyError(uint16_t aErrorCode)
{
  for (uint32_t i = 0; i < mGeolocators.Length(); i++) {
    mGeolocators[i]->NotifyError(aErrorCode);
  }
  return NS_OK;
}

void
nsGeolocationService::SetCachedPosition(nsIDOMGeoPosition* aPosition)
{
  mLastPosition.position = aPosition;
  mLastPosition.isHighAccuracy = mHigherAccuracy;
}

CachedPositionAndAccuracy
nsGeolocationService::GetCachedPosition()
{
  return mLastPosition;
}

nsresult
nsGeolocationService::StartDevice(nsIPrincipal *aPrincipal)
{
  if (!sGeoEnabled || sGeoInitPending) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  // we do not want to keep the geolocation devices online
  // indefinitely.  Close them down after a reasonable period of
  // inactivivity
  SetDisconnectTimer();

  if (XRE_IsContentProcess()) {
    ContentChild* cpc = ContentChild::GetSingleton();
    cpc->SendAddGeolocationListener(IPC::Principal(aPrincipal),
                                    HighAccuracyRequested());
    return NS_OK;
  }

  // Start them up!
  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  if (!obs) {
    return NS_ERROR_FAILURE;
  }

  if (!mProvider) {
    return NS_ERROR_FAILURE;
  }

  nsresult rv;

  if (NS_FAILED(rv = mProvider->Startup()) ||
      NS_FAILED(rv = mProvider->Watch(this))) {

    NotifyError(nsIDOMGeoPositionError::POSITION_UNAVAILABLE);
    return rv;
  }

  obs->NotifyObservers(mProvider,
                       "geolocation-device-events",
                       MOZ_UTF16("starting"));

  return NS_OK;
}

void
nsGeolocationService::StopDisconnectTimer()
{
  if (mDisconnectTimer) {
    mDisconnectTimer->Cancel();
    mDisconnectTimer = nullptr;
  }
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
nsGeolocationService::UpdateAccuracy(bool aForceHigh)
{
  bool highRequired = aForceHigh || HighAccuracyRequested();

  if (XRE_IsContentProcess()) {
    ContentChild* cpc = ContentChild::GetSingleton();
    if (cpc->IsAlive()) {
      cpc->SendSetGeolocationHigherAccuracy(highRequired);
    }
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
  StopDisconnectTimer();

  if (XRE_IsContentProcess()) {
    ContentChild* cpc = ContentChild::GetSingleton();
    cpc->SendRemoveGeolocationListener();
    return; // bail early
  }

  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
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
                       MOZ_UTF16("shutdown"));
}

StaticRefPtr<nsGeolocationService> nsGeolocationService::sService;

already_AddRefed<nsGeolocationService>
nsGeolocationService::GetGeolocationService()
{
  RefPtr<nsGeolocationService> result;
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
  NS_INTERFACE_MAP_ENTRY(nsIGeolocationUpdate)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(Geolocation)
NS_IMPL_CYCLE_COLLECTING_RELEASE(Geolocation)

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(Geolocation,
                                      mPendingCallbacks,
                                      mWatchingCallbacks,
                                      mPendingRequests)

Geolocation::Geolocation()
: mProtocolType(ProtocolType::OTHER)
, mLastWatchId(0)
{
}

Geolocation::~Geolocation()
{
  if (mService) {
    Shutdown();
  }
}

nsresult
Geolocation::Init(nsPIDOMWindowInner* aContentDom)
{
  // Remember the window
  if (aContentDom) {
    mOwner = do_GetWeakReference(aContentDom);
    if (!mOwner) {
      return NS_ERROR_FAILURE;
    }

    // Grab the principal of the document
    nsCOMPtr<nsIDocument> doc = aContentDom->GetDoc();
    if (!doc) {
      return NS_ERROR_FAILURE;
    }

    mPrincipal = doc->NodePrincipal();

    if (Preferences::GetBool("dom.wakelock.enabled") &&
        XRE_IsContentProcess()) {
      doc->AddSystemEventListener(NS_LITERAL_STRING("visibilitychange"),
                                  /* listener */ this,
                                  /* use capture */ true,
                                  /* wants untrusted */ false);
    }

    nsCOMPtr<nsIURI> uri;
    nsresult rv = mPrincipal->GetURI(getter_AddRefs(uri));
    NS_ENSURE_SUCCESS(rv, rv);

    if (uri) {
      bool isHttp;
      rv = uri->SchemeIs("http", &isHttp);
      NS_ENSURE_SUCCESS(rv, rv);

      bool isHttps;
      rv = uri->SchemeIs("https", &isHttps);
      NS_ENSURE_SUCCESS(rv, rv);

      // Store the protocol to send via telemetry later.
      if (isHttp) {
        mProtocolType = ProtocolType::HTTP;
      } else if (isHttps) {
        mProtocolType = ProtocolType::HTTPS;
      }
    }
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

bool
Geolocation::ContainsRequest(nsGeolocationRequest* aRequest)
{
  if (aRequest->IsWatch()) {
    if (mWatchingCallbacks.Contains(aRequest)) {
	return true;
    }
  } else {
    if (mPendingCallbacks.Contains(aRequest)) {
        return true;
    }
  }
  return false;
}


NS_IMETHODIMP
Geolocation::HandleEvent(nsIDOMEvent* aEvent)
{

  nsAutoString type;
  aEvent->GetType(type);
  if (!type.EqualsLiteral("visibilitychange")) {
    return NS_OK;
  }

  nsCOMPtr<nsIDocument> doc = do_QueryInterface(aEvent->InternalDOMEvent()->GetTarget());
  MOZ_ASSERT(doc);

  if (doc->Hidden()) {
    WakeLockInformation info;
    GetWakeLockInfo(NS_LITERAL_STRING("gps"), &info);

    MOZ_ASSERT(XRE_IsContentProcess());
    ContentChild* cpc = ContentChild::GetSingleton();
    if (!info.lockingProcesses().Contains(cpc->GetID())) {
      cpc->SendRemoveGeolocationListener();
      mService->StopDisconnectTimer();
    }
  } else {
    mService->SetDisconnectTimer();

    // We will unconditionally allow all the requests in the callbacks
    // because if a request is put into either of these two callbacks,
    // it means that it has been allowed before.
    // That's why when we resume them, we unconditionally allow them again.
    for (uint32_t i = 0, length = mWatchingCallbacks.Length(); i < length; ++i) {
      mWatchingCallbacks[i]->Allow(JS::UndefinedHandleValue);
    }
    for (uint32_t i = 0, length = mPendingCallbacks.Length(); i < length; ++i) {
      mPendingCallbacks[i]->Allow(JS::UndefinedHandleValue);
    }
  }

  return NS_OK;
}

void
Geolocation::Shutdown()
{
  // Release all callbacks
  mPendingCallbacks.Clear();
  mWatchingCallbacks.Clear();

  if (Preferences::GetBool("dom.wakelock.enabled") &&
      XRE_IsContentProcess()) {
    if (nsCOMPtr<nsPIDOMWindowInner> window = do_QueryReferent(mOwner)) {
      nsCOMPtr<nsIDocument> doc = window->GetExtantDoc();
      if (doc) {
        doc->RemoveSystemEventListener(NS_LITERAL_STRING("visibilitychange"),
                                       this,
                                       /* useCapture = */ true);
      }
    }
  }

  if (mService) {
    mService->RemoveLocator(this);
    mService->UpdateAccuracy();
  }

  mService = nullptr;
  mPrincipal = nullptr;
}

nsPIDOMWindowInner*
Geolocation::GetParentObject() const {
  nsCOMPtr<nsPIDOMWindowInner> window = do_QueryReferent(mOwner);
  return window.get();
}

bool
Geolocation::HasActiveCallbacks()
{
  return mPendingCallbacks.Length() || mWatchingCallbacks.Length();
}

bool
Geolocation::HighAccuracyRequested()
{
  for (uint32_t i = 0; i < mWatchingCallbacks.Length(); i++) {
    if (mWatchingCallbacks[i]->WantsHighAccuracy()) {
      return true;
    }
  }

  for (uint32_t i = 0; i < mPendingCallbacks.Length(); i++) {
    if (mPendingCallbacks[i]->WantsHighAccuracy()) {
      return true;
    }
  }

  return false;
}

void
Geolocation::RemoveRequest(nsGeolocationRequest* aRequest)
{
  bool requestWasKnown =
    (mPendingCallbacks.RemoveElement(aRequest) !=
     mWatchingCallbacks.RemoveElement(aRequest));

  Unused << requestWasKnown;
}

NS_IMETHODIMP
Geolocation::Update(nsIDOMGeoPosition *aSomewhere)
{
  if (!WindowOwnerStillExists()) {
    Shutdown();
    return NS_OK;
  }

  if (aSomewhere) {
    nsCOMPtr<nsIDOMGeoPositionCoords> coords;
    aSomewhere->GetCoords(getter_AddRefs(coords));
    if (coords) {
      double accuracy = -1;
      coords->GetAccuracy(&accuracy);
      mozilla::Telemetry::Accumulate(mozilla::Telemetry::GEOLOCATION_ACCURACY_EXPONENTIAL, accuracy);
    }
  }

  for (uint32_t i = mPendingCallbacks.Length(); i > 0; i--) {
    mPendingCallbacks[i-1]->Update(aSomewhere);
    RemoveRequest(mPendingCallbacks[i-1]);
  }

  // notify everyone that is watching
  for (uint32_t i = 0; i < mWatchingCallbacks.Length(); i++) {
    mWatchingCallbacks[i]->Update(aSomewhere);
  }
  return NS_OK;
}
NS_IMETHODIMP
Geolocation::NotifyError(uint16_t aErrorCode)
{
  if (!WindowOwnerStillExists()) {
    Shutdown();
    return NS_OK;
  }
  mozilla::Telemetry::Accumulate(mozilla::Telemetry::GEOLOCATION_ERROR, true);

  for (uint32_t i = mPendingCallbacks.Length(); i > 0; i--) {
    mPendingCallbacks[i-1]->NotifyErrorAndShutdown(aErrorCode);
    //NotifyErrorAndShutdown() removes the request from the array
  }

  // notify everyone that is watching
  for (uint32_t i = 0; i < mWatchingCallbacks.Length(); i++) {
    mWatchingCallbacks[i]->NotifyErrorAndShutdown(aErrorCode);
  }

  return NS_OK;
}

bool
Geolocation::IsAlreadyCleared(nsGeolocationRequest* aRequest)
{
  for (uint32_t i = 0, length = mClearedWatchIDs.Length(); i < length; ++i) {
    if (mClearedWatchIDs[i] == aRequest->WatchId()) {
      return true;
    }
  }
  return false;
}

bool
Geolocation::ClearPendingRequest(nsGeolocationRequest* aRequest)
{
  if (aRequest->IsWatch() && this->IsAlreadyCleared(aRequest)) {
    this->NotifyAllowedRequest(aRequest);
    this->ClearWatch(aRequest->WatchId());
    return true;
  }
  return false;
}

void
Geolocation::GetCurrentPosition(PositionCallback& aCallback,
                                PositionErrorCallback* aErrorCallback,
                                const PositionOptions& aOptions,
                                ErrorResult& aRv)
{
  GeoPositionCallback successCallback(&aCallback);
  GeoPositionErrorCallback errorCallback(aErrorCallback);

  nsresult rv = GetCurrentPosition(successCallback, errorCallback,
                                   CreatePositionOptionsCopy(aOptions));

  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
  }

  return;
}

NS_IMETHODIMP
Geolocation::GetCurrentPosition(nsIDOMGeoPositionCallback* aCallback,
                                nsIDOMGeoPositionErrorCallback* aErrorCallback,
                                PositionOptions* aOptions)
{
  NS_ENSURE_ARG_POINTER(aCallback);

  GeoPositionCallback successCallback(aCallback);
  GeoPositionErrorCallback errorCallback(aErrorCallback);

  return GetCurrentPosition(successCallback, errorCallback, aOptions);
}

nsresult
Geolocation::GetCurrentPosition(GeoPositionCallback& callback,
                                GeoPositionErrorCallback& errorCallback,
                                PositionOptions *options)
{
  if (mPendingCallbacks.Length() > MAX_GEO_REQUESTS_PER_WINDOW) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  // Count the number of requests per protocol/scheme.
  Telemetry::Accumulate(Telemetry::GEOLOCATION_GETCURRENTPOSITION_SECURE_ORIGIN,
                        static_cast<uint8_t>(mProtocolType));

  RefPtr<nsGeolocationRequest> request =
    new nsGeolocationRequest(this, callback, errorCallback, options,
                             static_cast<uint8_t>(mProtocolType), false);

  if (!sGeoEnabled) {
    nsCOMPtr<nsIRunnable> ev = new RequestAllowEvent(false, request);
    NS_DispatchToMainThread(ev);
    return NS_OK;
  }

  if (!mOwner && !nsContentUtils::LegacyIsCallerChromeOrNativeCode()) {
    return NS_ERROR_FAILURE;
  }

  if (sGeoInitPending) {
    mPendingRequests.AppendElement(request);
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

  if (!nsContentUtils::LegacyIsCallerChromeOrNativeCode()) {
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
  int32_t ret = 0;
  GeoPositionCallback successCallback(&aCallback);
  GeoPositionErrorCallback errorCallback(aErrorCallback);

  nsresult rv = WatchPosition(successCallback, errorCallback,
                              CreatePositionOptionsCopy(aOptions), &ret);

  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
  }

  return ret;
}

NS_IMETHODIMP
Geolocation::WatchPosition(nsIDOMGeoPositionCallback *aCallback,
                           nsIDOMGeoPositionErrorCallback *aErrorCallback,
                           PositionOptions *aOptions,
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
                           PositionOptions* aOptions,
                           int32_t* aRv)
{
  if (mWatchingCallbacks.Length() > MAX_GEO_REQUESTS_PER_WINDOW) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  // Count the number of requests per protocol/scheme.
  Telemetry::Accumulate(Telemetry::GEOLOCATION_WATCHPOSITION_SECURE_ORIGIN,
                        static_cast<uint8_t>(mProtocolType));

  // The watch ID:
  *aRv = mLastWatchId++;

  RefPtr<nsGeolocationRequest> request =
    new nsGeolocationRequest(this, aCallback, aErrorCallback, aOptions,
                             static_cast<uint8_t>(mProtocolType), true, *aRv);

  if (!sGeoEnabled) {
    GPSLOG("request allow event");
    nsCOMPtr<nsIRunnable> ev = new RequestAllowEvent(false, request);
    NS_DispatchToMainThread(ev);
    return NS_OK;
  }

  if (!mOwner && !nsContentUtils::LegacyIsCallerChromeOrNativeCode()) {
    return NS_ERROR_FAILURE;
  }

  if (sGeoInitPending) {
    mPendingRequests.AppendElement(request);
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

  if (!nsContentUtils::LegacyIsCallerChromeOrNativeCode()) {
    return NS_ERROR_FAILURE;
  }

  aRequest->Allow(JS::UndefinedHandleValue);

  return NS_OK;
}

NS_IMETHODIMP
Geolocation::ClearWatch(int32_t aWatchId)
{
  if (aWatchId < 0) {
    return NS_OK;
  }

  if (!mClearedWatchIDs.Contains(aWatchId)) {
    mClearedWatchIDs.AppendElement(aWatchId);
  }

  for (uint32_t i = 0, length = mWatchingCallbacks.Length(); i < length; ++i) {
    if (mWatchingCallbacks[i]->WatchId() == aWatchId) {
      mWatchingCallbacks[i]->Shutdown();
      RemoveRequest(mWatchingCallbacks[i]);
      mClearedWatchIDs.RemoveElement(aWatchId);
      break;
    }
  }

  // make sure we also search through the pending requests lists for
  // watches to clear...
  for (uint32_t i = 0, length = mPendingRequests.Length(); i < length; ++i) {
    if (mPendingRequests[i]->IsWatch() &&
        (mPendingRequests[i]->WatchId() == aWatchId)) {
      mPendingRequests[i]->Shutdown();
      mPendingRequests.RemoveElementAt(i);
      break;
    }
  }

  return NS_OK;
}

void
Geolocation::ServiceReady()
{
  for (uint32_t length = mPendingRequests.Length(); length > 0; --length) {
    if (mPendingRequests[0]->IsWatch()) {
      WatchPositionReady(mPendingRequests[0]);
    } else {
      GetCurrentPositionReady(mPendingRequests[0]);
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

  nsCOMPtr<nsPIDOMWindowInner> window = do_QueryReferent(mOwner);

  if (window) {
    nsPIDOMWindowOuter* outer = window->GetOuterWindow();
    if (!outer || outer->GetCurrentInnerWindow() != window ||
        outer->Closed()) {
      return false;
    }
  }

  return true;
}

void
Geolocation::NotifyAllowedRequest(nsGeolocationRequest* aRequest)
{
  if (aRequest->IsWatch()) {
    mWatchingCallbacks.AppendElement(aRequest);
  } else {
    mPendingCallbacks.AppendElement(aRequest);
  }
}

bool
Geolocation::RegisterRequestWithPrompt(nsGeolocationRequest* request)
{
  if (Preferences::GetBool("geo.prompt.testing", false)) {
    bool allow = Preferences::GetBool("geo.prompt.testing.allow", false);
    nsCOMPtr<nsIRunnable> ev = new RequestAllowEvent(allow, request);
    NS_DispatchToMainThread(ev);
    return true;
  }

  nsCOMPtr<nsIRunnable> ev  = new RequestPromptEvent(request, mOwner);
  NS_DispatchToMainThread(ev);
  return true;
}

JSObject*
Geolocation::WrapObject(JSContext *aCtx, JS::Handle<JSObject*> aGivenProto)
{
  return GeolocationBinding::Wrap(aCtx, this, aGivenProto);
}
