/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsGeolocation.h"

#include "mozilla/ClearOnShutdown.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/PermissionMessageUtils.h"
#include "mozilla/Preferences.h"
#include "mozilla/Services.h"
#include "mozilla/Telemetry.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Unused.h"
#include "mozilla/WeakPtr.h"
#include "mozilla/EventStateManager.h"
#include "nsComponentManagerUtils.h"
#include "nsContentPermissionHelper.h"
#include "nsContentUtils.h"
#include "nsDOMClassInfoID.h"
#include "nsGlobalWindow.h"
#include "nsIDocument.h"
#include "nsINamed.h"
#include "nsIObserverService.h"
#include "nsIScriptError.h"
#include "nsPIDOMWindow.h"
#include "nsServiceManagerUtils.h"
#include "nsThreadUtils.h"
#include "nsXULAppAPI.h"

class nsIPrincipal;

#ifdef MOZ_WIDGET_ANDROID
#include "AndroidLocationProvider.h"
#endif

#ifdef MOZ_GPSD
#include "GpsdLocationProvider.h"
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

// This preference allows to override the "secure context" by
// default policy.
#define PREF_GEO_SECURITY_ALLOWINSECURE "geo.security.allowinsecure"

using mozilla::Unused;          // <snicker>
using namespace mozilla;
using namespace mozilla::dom;

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
                       GeoPositionCallback aCallback,
                       GeoPositionErrorCallback aErrorCallback,
                       UniquePtr<PositionOptions>&& aOptions,
                       uint8_t aProtocolType,
                       bool aWatchPositionRequest = false,
                       bool aIsHandlingUserInput = false,
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
                                  , public nsINamed
  {
  public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSITIMERCALLBACK

    explicit TimerCallbackHolder(nsGeolocationRequest* aRequest)
      : mRequest(aRequest)
    {}

    NS_IMETHOD GetName(nsACString& aName) override
    {
      aName.AssignLiteral("nsGeolocationRequest::TimerCallbackHolder");
      return NS_OK;
    }

  private:
    ~TimerCallbackHolder() = default;
    WeakPtr<nsGeolocationRequest> mRequest;
  };

  void Notify();

  bool mIsWatchPositionRequest;

  nsCOMPtr<nsITimer> mTimeoutTimer;
  GeoPositionCallback mCallback;
  GeoPositionErrorCallback mErrorCallback;
  UniquePtr<PositionOptions> mOptions;
  bool mIsHandlingUserInput;

  RefPtr<Geolocation> mLocator;

  int32_t mWatchId;
  bool mShutdown;
  nsCOMPtr<nsIContentPermissionRequester> mRequester;
  uint8_t mProtocolType;
};

static UniquePtr<PositionOptions>
CreatePositionOptionsCopy(const PositionOptions& aOptions)
{
  UniquePtr<PositionOptions> geoOptions = MakeUnique<PositionOptions>();

  geoOptions->mEnableHighAccuracy = aOptions.mEnableHighAccuracy;
  geoOptions->mMaximumAge = aOptions.mMaximumAge;
  geoOptions->mTimeout = aOptions.mTimeout;

  return geoOptions;
}

class RequestPromptEvent : public Runnable
{
public:
  RequestPromptEvent(nsGeolocationRequest* aRequest, nsWeakPtr aWindow)
    : mozilla::Runnable("RequestPromptEvent")
    , mRequest(aRequest)
    , mWindow(aWindow)
  {
  }

  NS_IMETHOD Run() override
  {
    nsCOMPtr<nsPIDOMWindowInner> window = do_QueryReferent(mWindow);
    nsContentPermissionUtils::AskPermission(mRequest, window);
    return NS_OK;
  }

private:
  RefPtr<nsGeolocationRequest> mRequest;
  nsWeakPtr mWindow;
};

class RequestAllowEvent : public Runnable
{
public:
  RequestAllowEvent(int allow, nsGeolocationRequest* request)
    : mozilla::Runnable("RequestAllowEvent")
    , mAllow(allow)
    , mRequest(request)
  {
  }

  NS_IMETHOD Run() override {
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

class RequestSendLocationEvent : public Runnable
{
public:
  RequestSendLocationEvent(nsIDOMGeoPosition* aPosition,
                           nsGeolocationRequest* aRequest)
    : mozilla::Runnable("RequestSendLocationEvent")
    , mPosition(aPosition)
    , mRequest(aRequest)
  {
  }

  NS_IMETHOD Run() override {
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

PositionError::~PositionError() = default;


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
                                           GeoPositionCallback aCallback,
                                           GeoPositionErrorCallback aErrorCallback,
                                           UniquePtr<PositionOptions>&& aOptions,
                                           uint8_t aProtocolType,
                                           bool aWatchPositionRequest,
                                           bool aIsHandlingUserInput,
                                           int32_t aWatchId)
  : mIsWatchPositionRequest(aWatchPositionRequest),
    mCallback(Move(aCallback)),
    mErrorCallback(Move(aErrorCallback)),
    mOptions(Move(aOptions)),
    mIsHandlingUserInput(aIsHandlingUserInput),
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
nsGeolocationRequest::GetIsHandlingUserInput(bool* aIsHandlingUserInput)
{
  *aIsHandlingUserInput = mIsHandlingUserInput;
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

    // Record whether a location callback is fulfilled while the owner window
    // is not visible.
    bool isVisible = false;
    nsCOMPtr<nsPIDOMWindowInner> window = mLocator->GetParentObject();

    if (window) {
      nsCOMPtr<nsIDocument> doc = window->GetDoc();
      isVisible = doc && !doc->Hidden();
    }

    if (IsWatch()) {
      mozilla::Telemetry::Accumulate(mozilla::Telemetry::GEOLOCATION_WATCHPOSITION_VISIBLE, isVisible);
    } else {
      mozilla::Telemetry::Accumulate(mozilla::Telemetry::GEOLOCATION_GETCURRENTPOSITION_VISIBLE, isVisible);
    }
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
  MOZ_ASSERT(!mShutdown, "set timeout after shutdown");

  StopTimeoutTimer();

  if (mOptions && mOptions->mTimeout != 0 && mOptions->mTimeout != 0x7fffffff) {
    RefPtr<TimerCallbackHolder> holder = new TimerCallbackHolder(this);
    NS_NewTimerWithCallback(getter_AddRefs(mTimeoutTimer),
                            holder, mOptions->mTimeout, nsITimer::TYPE_ONE_SHOT);
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

  RefPtr<mozilla::dom::Position> wrapped;

  if (aPosition) {
    nsCOMPtr<nsIDOMGeoPositionCoords> coords;
    aPosition->GetCoords(getter_AddRefs(coords));
    if (coords) {
      wrapped = new mozilla::dom::Position(ToSupports(mLocator), aPosition);
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

  if (mIsWatchPositionRequest && !mShutdown) {
    SetTimeoutTimer();
  }
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
  nsCOMPtr<nsIRunnable> ev = new RequestSendLocationEvent(aPosition, this);
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

NS_IMPL_ISUPPORTS(nsGeolocationRequest::TimerCallbackHolder,
                  nsITimerCallback,
                  nsINamed)

NS_IMETHODIMP
nsGeolocationRequest::TimerCallbackHolder::Notify(nsITimer*)
{
  if (mRequest && mRequest->mLocator) {
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
static int32_t sProviderTimeout = 6000; // Time, in milliseconds, to wait for the location provider to spin up.

nsresult nsGeolocationService::Init()
{
  Preferences::AddIntVarCache(&sProviderTimeout, "geo.timeout", sProviderTimeout);
  Preferences::AddBoolVarCache(&sGeoEnabled, "geo.enabled", sGeoEnabled);

  if (!sGeoEnabled) {
    return NS_ERROR_FAILURE;
  }

  if (XRE_IsContentProcess()) {
    return NS_OK;
  }

  // geolocation service can be enabled -> now register observer
  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  if (!obs) {
    return NS_ERROR_FAILURE;
  }

  obs->AddObserver(this, "xpcom-shutdown", false);

#ifdef MOZ_WIDGET_ANDROID
  mProvider = new AndroidLocationProvider();
#endif

#ifdef MOZ_WIDGET_GTK
#ifdef MOZ_GPSD
  if (Preferences::GetBool("geo.provider.use_gpsd", false)) {
    mProvider = new GpsdLocationProvider();
  }
#endif
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
    nsCOMPtr<nsIGeolocationProvider> geoTestProvider =
      do_GetService(NS_GEOLOCATION_PROVIDER_CONTRACTID);

    if (geoTestProvider) {
      mProvider = geoTestProvider;
    }
  }

  return NS_OK;
}

nsGeolocationService::~nsGeolocationService() = default;

NS_IMETHODIMP
nsGeolocationService::Observe(nsISupports* aSubject,
                              const char* aTopic,
                              const char16_t* aData)
{
  if (!strcmp("xpcom-shutdown", aTopic)) {
    nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
    if (obs) {
      obs->RemoveObserver(this, "xpcom-shutdown");
    }

    for (uint32_t i = 0; i< mGeolocators.Length(); i++) {
      mGeolocators[i]->Shutdown();
    }
    StopDevice();

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
  if (!sGeoEnabled) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  // We do not want to keep the geolocation devices online
  // indefinitely.
  // Close them down after a reasonable period of inactivivity.
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
                       u"starting");

  return NS_OK;
}

void
nsGeolocationService::SetDisconnectTimer()
{
  if (!mDisconnectTimer) {
    mDisconnectTimer = NS_NewTimer();
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

  mProvider->SetHighAccuracy(!mHigherAccuracy && highRequired);
  mHigherAccuracy = highRequired;
}

void
nsGeolocationService::StopDevice()
{
  if (mDisconnectTimer) {
    mDisconnectTimer->Cancel();
    mDisconnectTimer = nullptr;
  }

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
                       u"shutdown");
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

void
Geolocation::Shutdown()
{
  // Release all callbacks
  mPendingCallbacks.Clear();
  mWatchingCallbacks.Clear();

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
Geolocation::ShouldBlockInsecureRequests() const
{
  if (Preferences::GetBool(PREF_GEO_SECURITY_ALLOWINSECURE, false)) {
    return false;
  }

  nsCOMPtr<nsPIDOMWindowInner> win = do_QueryReferent(mOwner);
  if (!win) {
    return false;
  }

  nsCOMPtr<nsIDocument> doc = win->GetDoc();
  if (!doc) {
    return false;
  }

  if (!nsGlobalWindowInner::Cast(win)->IsSecureContext()) {
    nsContentUtils::ReportToConsole(nsIScriptError::errorFlag,
                                    NS_LITERAL_CSTRING("DOM"), doc,
                                    nsContentUtils::eDOM_PROPERTIES,
                                    "GeolocationInsecureRequestIsForbidden");
    return true;
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
                                CallerType aCallerType,
                                ErrorResult& aRv)
{
  nsresult rv = GetCurrentPosition(GeoPositionCallback(&aCallback),
                                   GeoPositionErrorCallback(aErrorCallback),
                                   Move(CreatePositionOptionsCopy(aOptions)),
                                   aCallerType);

  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
  }
}

nsresult
Geolocation::GetCurrentPosition(GeoPositionCallback callback,
                                GeoPositionErrorCallback errorCallback,
                                UniquePtr<PositionOptions>&& options,
                                CallerType aCallerType)
{
  if (mPendingCallbacks.Length() > MAX_GEO_REQUESTS_PER_WINDOW) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  // After this we hand over ownership of options to our nsGeolocationRequest.

  // Count the number of requests per protocol/scheme.
  Telemetry::Accumulate(Telemetry::GEOLOCATION_GETCURRENTPOSITION_SECURE_ORIGIN,
                        static_cast<uint8_t>(mProtocolType));

  RefPtr<nsGeolocationRequest> request =
    new nsGeolocationRequest(this, Move(callback), Move(errorCallback),
                             Move(options), static_cast<uint8_t>(mProtocolType),
                             false, EventStateManager::IsHandlingUserInput());

  if (!sGeoEnabled || ShouldBlockInsecureRequests() ||
      nsContentUtils::ResistFingerprinting(aCallerType)) {
    nsCOMPtr<nsIRunnable> ev = new RequestAllowEvent(false, request);
    NS_DispatchToMainThread(ev);
    return NS_OK;
  }

  if (!mOwner && aCallerType != CallerType::System) {
    return NS_ERROR_FAILURE;
  }

  if (mOwner) {
    if (!RegisterRequestWithPrompt(request)) {
      return NS_ERROR_NOT_AVAILABLE;
    }

    return NS_OK;
  }

  if (aCallerType != CallerType::System) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIRunnable> ev = new RequestAllowEvent(true, request);
  NS_DispatchToMainThread(ev);

  return NS_OK;
}

int32_t
Geolocation::WatchPosition(PositionCallback& aCallback,
                           PositionErrorCallback* aErrorCallback,
                           const PositionOptions& aOptions,
                           CallerType aCallerType,
                           ErrorResult& aRv)
{
  int32_t ret = 0;
  nsresult rv = WatchPosition(GeoPositionCallback(&aCallback),
                              GeoPositionErrorCallback(aErrorCallback),
                              Move(CreatePositionOptionsCopy(aOptions)),
                              aCallerType,
                              &ret);

  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
  }

  return ret;
}

NS_IMETHODIMP
Geolocation::WatchPosition(nsIDOMGeoPositionCallback *aCallback,
                           nsIDOMGeoPositionErrorCallback *aErrorCallback,
                           UniquePtr<PositionOptions>&& aOptions,
                           int32_t* aRv)
{
  NS_ENSURE_ARG_POINTER(aCallback);

  return WatchPosition(GeoPositionCallback(aCallback),
                       GeoPositionErrorCallback(aErrorCallback),
                       Move(aOptions), CallerType::System,
                       aRv);
}

nsresult
Geolocation::WatchPosition(GeoPositionCallback aCallback,
                           GeoPositionErrorCallback aErrorCallback,
                           UniquePtr<PositionOptions>&& aOptions,
                           CallerType aCallerType,
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
    new nsGeolocationRequest(this, Move(aCallback), Move(aErrorCallback),
                             Move(aOptions),
                             static_cast<uint8_t>(mProtocolType), true,
                             EventStateManager::IsHandlingUserInput(), *aRv);

  if (!sGeoEnabled || ShouldBlockInsecureRequests() ||
      nsContentUtils::ResistFingerprinting(aCallerType)) {
    nsCOMPtr<nsIRunnable> ev = new RequestAllowEvent(false, request);
    NS_DispatchToMainThread(ev);
    return NS_OK;
  }

  if (!mOwner && aCallerType != CallerType::System) {
    return NS_ERROR_FAILURE;
  }

  if (mOwner) {
    if (!RegisterRequestWithPrompt(request))
      return NS_ERROR_NOT_AVAILABLE;

    return NS_OK;
  }

  if (aCallerType != CallerType::System) {
    return NS_ERROR_FAILURE;
  }

  request->Allow(JS::UndefinedHandleValue);

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
