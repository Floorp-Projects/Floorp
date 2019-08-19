/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsGeolocation.h"

#include "mozilla/ClearOnShutdown.h"
#include "mozilla/CycleCollectedJSContext.h"  // for nsAutoMicroTask
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/FeaturePolicyUtils.h"
#include "mozilla/dom/PermissionMessageUtils.h"
#include "mozilla/dom/PositionError.h"
#include "mozilla/dom/PositionErrorBinding.h"
#include "mozilla/Preferences.h"
#include "mozilla/Services.h"
#include "mozilla/StaticPrefs_geo.h"
#include "mozilla/Telemetry.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Unused.h"
#include "mozilla/WeakPtr.h"
#include "mozilla/EventStateManager.h"
#include "nsComponentManagerUtils.h"
#include "nsContentPermissionHelper.h"
#include "nsContentUtils.h"
#include "nsGlobalWindow.h"
#include "mozilla/dom/Document.h"
#include "nsINamed.h"
#include "nsIObserverService.h"
#include "nsIScriptError.h"
#include "nsPIDOMWindow.h"
#include "nsIXULRuntime.h"
#include "nsServiceManagerUtils.h"
#include "nsThreadUtils.h"
#include "nsXULAppAPI.h"

class nsIPrincipal;

#ifdef MOZ_WIDGET_ANDROID
#  include "AndroidLocationProvider.h"
#endif

#ifdef MOZ_GPSD
#  include "GpsdLocationProvider.h"
#endif

#ifdef MOZ_WIDGET_COCOA
#  include "CoreLocationLocationProvider.h"
#endif

#ifdef XP_WIN
#  include "WindowsLocationProvider.h"
#  include "mozilla/WindowsVersion.h"
#endif

// Some limit to the number of get or watch geolocation requests
// that a window can make.
#define MAX_GEO_REQUESTS_PER_WINDOW 1500

// This preference allows to override the "secure context" by
// default policy.
#define PREF_GEO_SECURITY_ALLOWINSECURE "geo.security.allowinsecure"

using mozilla::Unused;  // <snicker>
using namespace mozilla;
using namespace mozilla::dom;

class nsGeolocationRequest final
    : public ContentPermissionRequestBase,
      public nsIGeolocationUpdate,
      public SupportsWeakPtr<nsGeolocationRequest> {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIGEOLOCATIONUPDATE

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(nsGeolocationRequest,
                                           ContentPermissionRequestBase)

  nsGeolocationRequest(Geolocation* aLocator, GeoPositionCallback aCallback,
                       GeoPositionErrorCallback aErrorCallback,
                       UniquePtr<PositionOptions>&& aOptions,
                       uint8_t aProtocolType, nsIEventTarget* aMainThreadTarget,
                       bool aWatchPositionRequest = false,
                       int32_t aWatchId = 0);

  // nsIContentPermissionRequest
  MOZ_CAN_RUN_SCRIPT NS_IMETHOD Cancel(void) override;
  MOZ_CAN_RUN_SCRIPT NS_IMETHOD Allow(JS::HandleValue choices) override;

  MOZ_DECLARE_WEAKREFERENCE_TYPENAME(nsGeolocationRequest)

  void Shutdown();

  // MOZ_CAN_RUN_SCRIPT_BOUNDARY is OK here because we're always called from a
  // runnable.  Ideally nsIRunnable::Run and its overloads would just be
  // MOZ_CAN_RUN_SCRIPT and then we could be too...
  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  void SendLocation(nsIDOMGeoPosition* aLocation);
  bool WantsHighAccuracy() {
    return !mShutdown && mOptions && mOptions->mEnableHighAccuracy;
  }
  void SetTimeoutTimer();
  void StopTimeoutTimer();
  MOZ_CAN_RUN_SCRIPT
  void NotifyErrorAndShutdown(uint16_t);
  using ContentPermissionRequestBase::GetPrincipal;
  nsIPrincipal* GetPrincipal();

  bool IsWatch() { return mIsWatchPositionRequest; }
  int32_t WatchId() { return mWatchId; }

 private:
  virtual ~nsGeolocationRequest();

  class TimerCallbackHolder final : public nsITimerCallback, public nsINamed {
   public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSITIMERCALLBACK

    explicit TimerCallbackHolder(nsGeolocationRequest* aRequest)
        : mRequest(aRequest) {}

    NS_IMETHOD GetName(nsACString& aName) override {
      aName.AssignLiteral("nsGeolocationRequest::TimerCallbackHolder");
      return NS_OK;
    }

   private:
    ~TimerCallbackHolder() = default;
    WeakPtr<nsGeolocationRequest> mRequest;
  };

  // Only called from a timer, so MOZ_CAN_RUN_SCRIPT_BOUNDARY ok for now.
  MOZ_CAN_RUN_SCRIPT_BOUNDARY void Notify();

  bool mIsWatchPositionRequest;

  nsCOMPtr<nsITimer> mTimeoutTimer;
  GeoPositionCallback mCallback;
  GeoPositionErrorCallback mErrorCallback;
  UniquePtr<PositionOptions> mOptions;

  RefPtr<Geolocation> mLocator;

  int32_t mWatchId;
  bool mShutdown;
  nsCOMPtr<nsIContentPermissionRequester> mRequester;
  uint8_t mProtocolType;
  nsCOMPtr<nsIEventTarget> mMainThreadTarget;
};

static UniquePtr<PositionOptions> CreatePositionOptionsCopy(
    const PositionOptions& aOptions) {
  UniquePtr<PositionOptions> geoOptions = MakeUnique<PositionOptions>();

  geoOptions->mEnableHighAccuracy = aOptions.mEnableHighAccuracy;
  geoOptions->mMaximumAge = aOptions.mMaximumAge;
  geoOptions->mTimeout = aOptions.mTimeout;

  return geoOptions;
}

class RequestSendLocationEvent : public Runnable {
 public:
  RequestSendLocationEvent(nsIDOMGeoPosition* aPosition,
                           nsGeolocationRequest* aRequest)
      : mozilla::Runnable("RequestSendLocationEvent"),
        mPosition(aPosition),
        mRequest(aRequest) {}

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
// nsGeolocationRequest
////////////////////////////////////////////////////

static nsPIDOMWindowInner* ConvertWeakReferenceToWindow(
    nsIWeakReference* aWeakPtr) {
  nsCOMPtr<nsPIDOMWindowInner> window = do_QueryReferent(aWeakPtr);
  // This isn't usually safe, but here we're just extracting a raw pointer in
  // order to pass it to a base class constructor which will in turn convert it
  // into a strong pointer for us.
  nsPIDOMWindowInner* raw = window.get();
  return raw;
}

nsGeolocationRequest::nsGeolocationRequest(
    Geolocation* aLocator, GeoPositionCallback aCallback,
    GeoPositionErrorCallback aErrorCallback,
    UniquePtr<PositionOptions>&& aOptions, uint8_t aProtocolType,
    nsIEventTarget* aMainThreadTarget, bool aWatchPositionRequest,
    int32_t aWatchId)
    : ContentPermissionRequestBase(
          aLocator->GetPrincipal(),
          ConvertWeakReferenceToWindow(aLocator->GetOwner()),
          NS_LITERAL_CSTRING("geo"), NS_LITERAL_CSTRING("geolocation")),
      mIsWatchPositionRequest(aWatchPositionRequest),
      mCallback(std::move(aCallback)),
      mErrorCallback(std::move(aErrorCallback)),
      mOptions(std::move(aOptions)),
      mLocator(aLocator),
      mWatchId(aWatchId),
      mShutdown(false),
      mProtocolType(aProtocolType),
      mMainThreadTarget(aMainThreadTarget) {
  if (nsCOMPtr<nsPIDOMWindowInner> win =
          do_QueryReferent(mLocator->GetOwner())) {
  }
}

nsGeolocationRequest::~nsGeolocationRequest() { StopTimeoutTimer(); }

NS_IMPL_QUERY_INTERFACE_CYCLE_COLLECTION_INHERITED(nsGeolocationRequest,
                                                   ContentPermissionRequestBase,
                                                   nsIGeolocationUpdate)

NS_IMPL_ADDREF_INHERITED(nsGeolocationRequest, ContentPermissionRequestBase)
NS_IMPL_RELEASE_INHERITED(nsGeolocationRequest, ContentPermissionRequestBase)
NS_IMPL_CYCLE_COLLECTION_INHERITED(nsGeolocationRequest,
                                   ContentPermissionRequestBase, mCallback,
                                   mErrorCallback, mLocator)

void nsGeolocationRequest::Notify() {
  SetTimeoutTimer();
  NotifyErrorAndShutdown(PositionError_Binding::TIMEOUT);
}

void nsGeolocationRequest::NotifyErrorAndShutdown(uint16_t aErrorCode) {
  MOZ_ASSERT(!mShutdown, "timeout after shutdown");
  if (!mIsWatchPositionRequest) {
    Shutdown();
    mLocator->RemoveRequest(this);
  }

  NotifyError(aErrorCode);
}

NS_IMETHODIMP
nsGeolocationRequest::Cancel() {
  if (mRequester) {
    // Record the number of denied requests for regular web content.
    // This method is only called when the user explicitly denies the request,
    // and is not called when the page is simply unloaded, or similar.
    Telemetry::Accumulate(Telemetry::GEOLOCATION_REQUEST_GRANTED,
                          mProtocolType);
  }

  if (mLocator->ClearPendingRequest(this)) {
    return NS_OK;
  }

  NotifyError(PositionError_Binding::PERMISSION_DENIED);
  return NS_OK;
}

NS_IMETHODIMP
nsGeolocationRequest::Allow(JS::HandleValue aChoices) {
  MOZ_ASSERT(aChoices.isUndefined());

  if (mRequester) {
    // Record the number of granted requests for regular web content.
    Telemetry::Accumulate(Telemetry::GEOLOCATION_REQUEST_GRANTED,
                          mProtocolType + 10);

    // Record whether a location callback is fulfilled while the owner window
    // is not visible.
    bool isVisible = false;
    nsCOMPtr<nsPIDOMWindowInner> window = mLocator->GetParentObject();

    if (window) {
      nsCOMPtr<Document> doc = window->GetDoc();
      isVisible = doc && !doc->Hidden();
    }

    if (IsWatch()) {
      mozilla::Telemetry::Accumulate(
          mozilla::Telemetry::GEOLOCATION_WATCHPOSITION_VISIBLE, isVisible);
    } else {
      mozilla::Telemetry::Accumulate(
          mozilla::Telemetry::GEOLOCATION_GETCURRENTPOSITION_VISIBLE,
          isVisible);
    }
  }

  if (mLocator->ClearPendingRequest(this)) {
    return NS_OK;
  }

  RefPtr<nsGeolocationService> gs =
      nsGeolocationService::GetGeolocationService();

  bool canUseCache = false;
  CachedPositionAndAccuracy lastPosition = gs->GetCachedPosition();
  if (lastPosition.position) {
    DOMTimeStamp cachedPositionTime_ms;
    lastPosition.position->GetTimestamp(&cachedPositionTime_ms);
    // check to see if we can use a cached value
    // if the user has specified a maximumAge, return a cached value.
    if (mOptions && mOptions->mMaximumAge > 0) {
      uint32_t maximumAge_ms = mOptions->mMaximumAge;
      bool isCachedWithinRequestedAccuracy =
          WantsHighAccuracy() <= lastPosition.isHighAccuracy;
      bool isCachedWithinRequestedTime =
          DOMTimeStamp(PR_Now() / PR_USEC_PER_MSEC - maximumAge_ms) <=
          cachedPositionTime_ms;
      canUseCache =
          isCachedWithinRequestedAccuracy && isCachedWithinRequestedTime;
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
      NotifyError(PositionError_Binding::TIMEOUT);
      return NS_OK;
    }
  }

  // Kick off the geo device, if it isn't already running
  nsCOMPtr<nsIPrincipal> principal = GetPrincipal();
  nsresult rv = gs->StartDevice(principal);

  if (NS_FAILED(rv)) {
    // Location provider error
    NotifyError(PositionError_Binding::POSITION_UNAVAILABLE);
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

void nsGeolocationRequest::SetTimeoutTimer() {
  MOZ_ASSERT(!mShutdown, "set timeout after shutdown");

  StopTimeoutTimer();

  if (mOptions && mOptions->mTimeout != 0 && mOptions->mTimeout != 0x7fffffff) {
    RefPtr<TimerCallbackHolder> holder = new TimerCallbackHolder(this);
    NS_NewTimerWithCallback(getter_AddRefs(mTimeoutTimer), holder,
                            mOptions->mTimeout, nsITimer::TYPE_ONE_SHOT);
  }
}

void nsGeolocationRequest::StopTimeoutTimer() {
  if (mTimeoutTimer) {
    mTimeoutTimer->Cancel();
    mTimeoutTimer = nullptr;
  }
}

void nsGeolocationRequest::SendLocation(nsIDOMGeoPosition* aPosition) {
  if (mShutdown) {
    // Ignore SendLocationEvents issued before we were cleared.
    return;
  }

  if (mOptions && mOptions->mMaximumAge > 0) {
    DOMTimeStamp positionTime_ms;
    aPosition->GetTimestamp(&positionTime_ms);
    const uint32_t maximumAge_ms = mOptions->mMaximumAge;
    const bool isTooOld = DOMTimeStamp(PR_Now() / PR_USEC_PER_MSEC -
                                       maximumAge_ms) > positionTime_ms;
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
    NotifyError(PositionError_Binding::POSITION_UNAVAILABLE);
    return;
  }

  if (!mIsWatchPositionRequest) {
    // Cancel timer and position updates in case the position
    // callback spins the event loop
    Shutdown();
  }

  nsAutoMicroTask mt;
  if (mCallback.HasWebIDLCallback()) {
    RefPtr<PositionCallback> callback = mCallback.GetWebIDLCallback();

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

nsIPrincipal* nsGeolocationRequest::GetPrincipal() {
  if (!mLocator) {
    return nullptr;
  }
  return mLocator->GetPrincipal();
}

NS_IMETHODIMP
nsGeolocationRequest::Update(nsIDOMGeoPosition* aPosition) {
  nsCOMPtr<nsIRunnable> ev = new RequestSendLocationEvent(aPosition, this);
  mMainThreadTarget->Dispatch(ev.forget());
  return NS_OK;
}

NS_IMETHODIMP
nsGeolocationRequest::NotifyError(uint16_t aErrorCode) {
  MOZ_ASSERT(NS_IsMainThread());
  RefPtr<PositionError> positionError = new PositionError(mLocator, aErrorCode);
  positionError->NotifyCallback(mErrorCallback);
  return NS_OK;
}

void nsGeolocationRequest::Shutdown() {
  MOZ_ASSERT(!mShutdown, "request shutdown twice");
  mShutdown = true;

  StopTimeoutTimer();

  // If there are no other high accuracy requests, the geolocation service will
  // notify the provider to switch to the default accuracy.
  if (mOptions && mOptions->mEnableHighAccuracy) {
    RefPtr<nsGeolocationService> gs =
        nsGeolocationService::GetGeolocationService();
    if (gs) {
      gs->UpdateAccuracy();
    }
  }
}

////////////////////////////////////////////////////
// nsGeolocationRequest::TimerCallbackHolder
////////////////////////////////////////////////////

NS_IMPL_ISUPPORTS(nsGeolocationRequest::TimerCallbackHolder, nsITimerCallback,
                  nsINamed)

NS_IMETHODIMP
nsGeolocationRequest::TimerCallbackHolder::Notify(nsITimer*) {
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

nsresult nsGeolocationService::Init() {
  if (!StaticPrefs::geo_enabled()) {
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
#  ifdef MOZ_GPSD
  if (Preferences::GetBool("geo.provider.use_gpsd", false)) {
    mProvider = new GpsdLocationProvider();
  }
#  endif
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
nsGeolocationService::Observe(nsISupports* aSubject, const char* aTopic,
                              const char16_t* aData) {
  if (!strcmp("xpcom-shutdown", aTopic)) {
    nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
    if (obs) {
      obs->RemoveObserver(this, "xpcom-shutdown");
    }

    for (uint32_t i = 0; i < mGeolocators.Length(); i++) {
      mGeolocators[i]->Shutdown();
    }
    StopDevice();

    return NS_OK;
  }

  if (!strcmp("timer-callback", aTopic)) {
    // decide if we can close down the service.
    for (uint32_t i = 0; i < mGeolocators.Length(); i++)
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
nsGeolocationService::Update(nsIDOMGeoPosition* aSomewhere) {
  if (aSomewhere) {
    SetCachedPosition(aSomewhere);
  }

  for (uint32_t i = 0; i < mGeolocators.Length(); i++) {
    mGeolocators[i]->Update(aSomewhere);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsGeolocationService::NotifyError(uint16_t aErrorCode) {
  // nsTArray doesn't have a constructors that takes a different-type TArray.
  nsTArray<RefPtr<Geolocation>> geolocators;
  geolocators.AppendElements(mGeolocators);
  for (uint32_t i = 0; i < geolocators.Length(); i++) {
    // MOZ_KnownLive because the stack array above keeps it alive.
    MOZ_KnownLive(geolocators[i])->NotifyError(aErrorCode);
  }
  return NS_OK;
}

void nsGeolocationService::SetCachedPosition(nsIDOMGeoPosition* aPosition) {
  mLastPosition.position = aPosition;
  mLastPosition.isHighAccuracy = mHigherAccuracy;
}

CachedPositionAndAccuracy nsGeolocationService::GetCachedPosition() {
  return mLastPosition;
}

nsresult nsGeolocationService::StartDevice(nsIPrincipal* aPrincipal) {
  if (!StaticPrefs::geo_enabled()) {
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
    NotifyError(PositionError_Binding::POSITION_UNAVAILABLE);
    return rv;
  }

  obs->NotifyObservers(mProvider, "geolocation-device-events", u"starting");

  return NS_OK;
}

void nsGeolocationService::SetDisconnectTimer() {
  if (!mDisconnectTimer) {
    mDisconnectTimer = NS_NewTimer();
  } else {
    mDisconnectTimer->Cancel();
  }

  mDisconnectTimer->Init(this, StaticPrefs::geo_timeout(),
                         nsITimer::TYPE_ONE_SHOT);
}

bool nsGeolocationService::HighAccuracyRequested() {
  for (uint32_t i = 0; i < mGeolocators.Length(); i++) {
    if (mGeolocators[i]->HighAccuracyRequested()) {
      return true;
    }
  }

  return false;
}

void nsGeolocationService::UpdateAccuracy(bool aForceHigh) {
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

void nsGeolocationService::StopDevice() {
  if (mDisconnectTimer) {
    mDisconnectTimer->Cancel();
    mDisconnectTimer = nullptr;
  }

  if (XRE_IsContentProcess()) {
    ContentChild* cpc = ContentChild::GetSingleton();
    cpc->SendRemoveGeolocationListener();

    return;  // bail early
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
  obs->NotifyObservers(mProvider, "geolocation-device-events", u"shutdown");
}

StaticRefPtr<nsGeolocationService> nsGeolocationService::sService;

already_AddRefed<nsGeolocationService>
nsGeolocationService::GetGeolocationService() {
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

void nsGeolocationService::AddLocator(Geolocation* aLocator) {
  mGeolocators.AppendElement(aLocator);
}

void nsGeolocationService::RemoveLocator(Geolocation* aLocator) {
  mGeolocators.RemoveElement(aLocator);
}

////////////////////////////////////////////////////
// Geolocation
////////////////////////////////////////////////////

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(Geolocation)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY(nsIGeolocationUpdate)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(Geolocation)
NS_IMPL_CYCLE_COLLECTING_RELEASE(Geolocation)

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(Geolocation, mPendingCallbacks,
                                      mWatchingCallbacks, mPendingRequests)

Geolocation::Geolocation()
    : mProtocolType(ProtocolType::OTHER), mLastWatchId(0) {}

Geolocation::~Geolocation() {
  if (mService) {
    Shutdown();
  }
}

StaticRefPtr<Geolocation> Geolocation::sNonWindowSingleton;

already_AddRefed<Geolocation> Geolocation::NonWindowSingleton() {
  if (sNonWindowSingleton) {
    return do_AddRef(sNonWindowSingleton);
  }

  RefPtr<Geolocation> result = new Geolocation();
  DebugOnly<nsresult> rv = result->Init();
  MOZ_ASSERT(NS_SUCCEEDED(rv), "How can this fail?");

  ClearOnShutdown(&sNonWindowSingleton);
  sNonWindowSingleton = result;
  return result.forget();
}

nsresult Geolocation::Init(nsPIDOMWindowInner* aContentDom) {
  // Remember the window
  if (aContentDom) {
    mOwner = do_GetWeakReference(aContentDom);
    if (!mOwner) {
      return NS_ERROR_FAILURE;
    }

    // Grab the principal of the document
    nsCOMPtr<Document> doc = aContentDom->GetDoc();
    if (!doc) {
      return NS_ERROR_FAILURE;
    }

    mPrincipal = doc->NodePrincipal();

    nsCOMPtr<nsIURI> uri;
    nsresult rv = mPrincipal->GetURI(getter_AddRefs(uri));
    NS_ENSURE_SUCCESS(rv, rv);

    if (uri) {
      // Store the protocol to send via telemetry later.
      if (uri->SchemeIs("http")) {
        mProtocolType = ProtocolType::HTTP;
      } else if (uri->SchemeIs("https")) {
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

void Geolocation::Shutdown() {
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

nsPIDOMWindowInner* Geolocation::GetParentObject() const {
  nsCOMPtr<nsPIDOMWindowInner> window = do_QueryReferent(mOwner);
  return window.get();
}

bool Geolocation::HasActiveCallbacks() {
  return mPendingCallbacks.Length() || mWatchingCallbacks.Length();
}

bool Geolocation::HighAccuracyRequested() {
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

void Geolocation::RemoveRequest(nsGeolocationRequest* aRequest) {
  bool requestWasKnown = (mPendingCallbacks.RemoveElement(aRequest) !=
                          mWatchingCallbacks.RemoveElement(aRequest));

  Unused << requestWasKnown;
}

NS_IMETHODIMP
Geolocation::Update(nsIDOMGeoPosition* aSomewhere) {
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
      mozilla::Telemetry::Accumulate(
          mozilla::Telemetry::GEOLOCATION_ACCURACY_EXPONENTIAL, accuracy);
    }
  }

  for (uint32_t i = mPendingCallbacks.Length(); i > 0; i--) {
    mPendingCallbacks[i - 1]->Update(aSomewhere);
    RemoveRequest(mPendingCallbacks[i - 1]);
  }

  // notify everyone that is watching
  for (uint32_t i = 0; i < mWatchingCallbacks.Length(); i++) {
    mWatchingCallbacks[i]->Update(aSomewhere);
  }

  return NS_OK;
}

NS_IMETHODIMP
Geolocation::NotifyError(uint16_t aErrorCode) {
  if (!WindowOwnerStillExists()) {
    Shutdown();
    return NS_OK;
  }

  mozilla::Telemetry::Accumulate(mozilla::Telemetry::GEOLOCATION_ERROR, true);

  for (uint32_t i = mPendingCallbacks.Length(); i > 0; i--) {
    RefPtr<nsGeolocationRequest> request = mPendingCallbacks[i - 1];
    request->NotifyErrorAndShutdown(aErrorCode);
    // NotifyErrorAndShutdown() removes the request from the array
  }

  // notify everyone that is watching
  for (uint32_t i = 0; i < mWatchingCallbacks.Length(); i++) {
    RefPtr<nsGeolocationRequest> request = mWatchingCallbacks[i];
    request->NotifyErrorAndShutdown(aErrorCode);
  }

  return NS_OK;
}

bool Geolocation::IsAlreadyCleared(nsGeolocationRequest* aRequest) {
  for (uint32_t i = 0, length = mClearedWatchIDs.Length(); i < length; ++i) {
    if (mClearedWatchIDs[i] == aRequest->WatchId()) {
      return true;
    }
  }

  return false;
}

bool Geolocation::ShouldBlockInsecureRequests() const {
  if (Preferences::GetBool(PREF_GEO_SECURITY_ALLOWINSECURE, false)) {
    return false;
  }

  nsCOMPtr<nsPIDOMWindowInner> win = do_QueryReferent(mOwner);
  if (!win) {
    return false;
  }

  nsCOMPtr<Document> doc = win->GetDoc();
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

bool Geolocation::FeaturePolicyBlocked() const {
  nsCOMPtr<nsPIDOMWindowInner> win = do_QueryReferent(mOwner);
  if (!win) {
    return true;
  }

  nsCOMPtr<Document> doc = win->GetExtantDoc();
  if (!doc) {
    return false;
  }

  return FeaturePolicyUtils::IsFeatureAllowed(doc,
                                              NS_LITERAL_STRING("geolocation"));
}

bool Geolocation::ClearPendingRequest(nsGeolocationRequest* aRequest) {
  if (aRequest->IsWatch() && this->IsAlreadyCleared(aRequest)) {
    this->NotifyAllowedRequest(aRequest);
    this->ClearWatch(aRequest->WatchId());
    return true;
  }

  return false;
}

void Geolocation::GetCurrentPosition(PositionCallback& aCallback,
                                     PositionErrorCallback* aErrorCallback,
                                     const PositionOptions& aOptions,
                                     CallerType aCallerType, ErrorResult& aRv) {
  nsresult rv = GetCurrentPosition(
      GeoPositionCallback(&aCallback), GeoPositionErrorCallback(aErrorCallback),
      CreatePositionOptionsCopy(aOptions), aCallerType);

  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
  }
}

static nsIEventTarget* MainThreadTarget(Geolocation* geo) {
  nsCOMPtr<nsPIDOMWindowInner> window = do_QueryReferent(geo->GetOwner());
  if (!window) {
    return GetMainThreadEventTarget();
  }
  return nsGlobalWindowInner::Cast(window)->EventTargetFor(
      mozilla::TaskCategory::Other);
}

nsresult Geolocation::GetCurrentPosition(GeoPositionCallback callback,
                                         GeoPositionErrorCallback errorCallback,
                                         UniquePtr<PositionOptions>&& options,
                                         CallerType aCallerType) {
  if (mPendingCallbacks.Length() > MAX_GEO_REQUESTS_PER_WINDOW) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  // After this we hand over ownership of options to our nsGeolocationRequest.

  // Count the number of requests per protocol/scheme.
  Telemetry::Accumulate(Telemetry::GEOLOCATION_GETCURRENTPOSITION_SECURE_ORIGIN,
                        static_cast<uint8_t>(mProtocolType));

  nsIEventTarget* target = MainThreadTarget(this);
  RefPtr<nsGeolocationRequest> request = new nsGeolocationRequest(
      this, std::move(callback), std::move(errorCallback), std::move(options),
      static_cast<uint8_t>(mProtocolType), target);

  if (!StaticPrefs::geo_enabled() || ShouldBlockInsecureRequests() ||
      !FeaturePolicyBlocked()) {
    request->RequestDelayedTask(target,
                                nsGeolocationRequest::DelayedTaskType::Deny);
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

  request->RequestDelayedTask(target,
                              nsGeolocationRequest::DelayedTaskType::Allow);

  return NS_OK;
}

int32_t Geolocation::WatchPosition(PositionCallback& aCallback,
                                   PositionErrorCallback* aErrorCallback,
                                   const PositionOptions& aOptions,
                                   CallerType aCallerType, ErrorResult& aRv) {
  return WatchPosition(GeoPositionCallback(&aCallback),
                       GeoPositionErrorCallback(aErrorCallback),
                       CreatePositionOptionsCopy(aOptions), aCallerType, aRv);
}

int32_t Geolocation::WatchPosition(
    nsIDOMGeoPositionCallback* aCallback,
    nsIDOMGeoPositionErrorCallback* aErrorCallback,
    UniquePtr<PositionOptions>&& aOptions) {
  MOZ_ASSERT(aCallback);

  return WatchPosition(GeoPositionCallback(aCallback),
                       GeoPositionErrorCallback(aErrorCallback),
                       std::move(aOptions), CallerType::System, IgnoreErrors());
}

// On errors we return -1 because that's not a valid watch id and will
// get ignored in clearWatch.
int32_t Geolocation::WatchPosition(GeoPositionCallback aCallback,
                                   GeoPositionErrorCallback aErrorCallback,
                                   UniquePtr<PositionOptions>&& aOptions,
                                   CallerType aCallerType, ErrorResult& aRv) {
  if (mWatchingCallbacks.Length() > MAX_GEO_REQUESTS_PER_WINDOW) {
    aRv.Throw(NS_ERROR_NOT_AVAILABLE);
    return -1;
  }

  // Count the number of requests per protocol/scheme.
  Telemetry::Accumulate(Telemetry::GEOLOCATION_WATCHPOSITION_SECURE_ORIGIN,
                        static_cast<uint8_t>(mProtocolType));

  // The watch ID:
  int32_t watchId = mLastWatchId++;

  nsIEventTarget* target = MainThreadTarget(this);
  RefPtr<nsGeolocationRequest> request = new nsGeolocationRequest(
      this, std::move(aCallback), std::move(aErrorCallback),
      std::move(aOptions), static_cast<uint8_t>(mProtocolType), target, true,
      watchId);

  if (!StaticPrefs::geo_enabled() || ShouldBlockInsecureRequests() ||
      !FeaturePolicyBlocked()) {
    request->RequestDelayedTask(target,
                                nsGeolocationRequest::DelayedTaskType::Deny);
    return watchId;
  }

  if (!mOwner && aCallerType != CallerType::System) {
    aRv.Throw(NS_ERROR_FAILURE);
    return -1;
  }

  if (mOwner) {
    if (!RegisterRequestWithPrompt(request)) {
      aRv.Throw(NS_ERROR_NOT_AVAILABLE);
      return -1;
    }

    return watchId;
  }

  if (aCallerType != CallerType::System) {
    aRv.Throw(NS_ERROR_FAILURE);
    return -1;
  }

  request->Allow(JS::UndefinedHandleValue);
  return watchId;
}

void Geolocation::ClearWatch(int32_t aWatchId) {
  if (aWatchId < 0) {
    return;
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
}

bool Geolocation::WindowOwnerStillExists() {
  // an owner was never set when Geolocation
  // was created, which means that this object
  // is being used without a window.
  if (mOwner == nullptr) {
    return true;
  }

  nsCOMPtr<nsPIDOMWindowInner> window = do_QueryReferent(mOwner);

  if (window) {
    nsPIDOMWindowOuter* outer = window->GetOuterWindow();
    if (!outer || outer->GetCurrentInnerWindow() != window || outer->Closed()) {
      return false;
    }
  }

  return true;
}

void Geolocation::NotifyAllowedRequest(nsGeolocationRequest* aRequest) {
  if (aRequest->IsWatch()) {
    mWatchingCallbacks.AppendElement(aRequest);
  } else {
    mPendingCallbacks.AppendElement(aRequest);
  }
}

bool Geolocation::RegisterRequestWithPromptImpl(
    nsGeolocationRequest* aRequest) {
  nsIEventTarget* target = MainThreadTarget(this);
  ContentPermissionRequestBase::PromptResult pr = aRequest->CheckPromptPrefs();
  if (pr == ContentPermissionRequestBase::PromptResult::Granted) {
    aRequest->RequestDelayedTask(target,
                                 nsGeolocationRequest::DelayedTaskType::Allow);
    return true;
  }
  if (pr == ContentPermissionRequestBase::PromptResult::Denied) {
    aRequest->RequestDelayedTask(target,
                                 nsGeolocationRequest::DelayedTaskType::Deny);
    return true;
  }

  aRequest->RequestDelayedTask(target,
                               nsGeolocationRequest::DelayedTaskType::Request);
  return true;
}

bool Geolocation::RegisterRequestWithPrompt(nsGeolocationRequest* request) {
  if (XRE_IsParentProcess()) {
#ifdef MOZ_WIDGET_COCOA
    if (!isMacGeoSystemPermissionEnabled()) {
      return false;
    }
#endif
    return RegisterRequestWithPromptImpl(request);
  }

  RefPtr<Geolocation> self = this;
  RefPtr<nsGeolocationRequest> req = request;
  nsCOMPtr<nsISerialEventTarget> serialTarget =
      SystemGroup::EventTargetFor(TaskCategory::Other);
  ContentChild* cpc = ContentChild::GetSingleton();
  cpc->SendGetGeoSysPermission()->Then(
      serialTarget, __func__,
      [req, self](bool aSysPermIsGranted) {
        if (!aSysPermIsGranted) {
          nsIEventTarget* target = MainThreadTarget(self);
          req->RequestDelayedTask(target,
                                  nsGeolocationRequest::DelayedTaskType::Deny);
        } else {
          self->RegisterRequestWithPromptImpl(req);
        }
      },
      [req, self](mozilla::ipc::ResponseRejectReason aReason) {
        nsIEventTarget* target = MainThreadTarget(self);
        req->RequestDelayedTask(target,
                                nsGeolocationRequest::DelayedTaskType::Deny);
      });
  return true;
}

JSObject* Geolocation::WrapObject(JSContext* aCtx,
                                  JS::Handle<JSObject*> aGivenProto) {
  return Geolocation_Binding::Wrap(aCtx, this, aGivenProto);
}
