/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsGeoLocation_h
#define nsGeoLocation_h

#include "mozilla/dom/PContentPermissionRequestChild.h"
// Microsoft's API Name hackery sucks
#undef CreateEvent

#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "nsCOMArray.h"
#include "nsTArray.h"
#include "nsITimer.h"
#include "nsIObserver.h"
#include "nsIURI.h"
#include "nsWrapperCache.h"

#include "nsWeakPtr.h"
#include "nsCycleCollectionParticipant.h"

#include "nsGeoPosition.h"
#include "nsIDOMGeoGeolocation.h"
#include "nsIDOMGeoPosition.h"
#include "nsIDOMGeoPositionError.h"
#include "nsIDOMGeoPositionCallback.h"
#include "nsIDOMGeoPositionErrorCallback.h"
#include "nsIDOMNavigatorGeolocation.h"
#include "mozilla/dom/GeolocationBinding.h"
#include "mozilla/dom/PositionErrorBinding.h"
#include "mozilla/dom/CallbackObject.h"

#include "nsPIDOMWindow.h"

#include "nsIGeolocationProvider.h"
#include "nsIContentPermissionPrompt.h"
#include "DictionaryHelpers.h"
#include "PCOMContentPermissionRequestChild.h"
#include "mozilla/Attributes.h"

class nsGeolocationService;

namespace mozilla {
namespace dom {
class Geolocation;
typedef CallbackObjectHolder<PositionCallback, nsIDOMGeoPositionCallback> GeoPositionCallback;
typedef CallbackObjectHolder<PositionErrorCallback, nsIDOMGeoPositionErrorCallback> GeoPositionErrorCallback;
}
}

class nsGeolocationRequest
 : public nsIContentPermissionRequest
 , public nsITimerCallback
 , public PCOMContentPermissionRequestChild
{
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_NSICONTENTPERMISSIONREQUEST
  NS_DECL_NSITIMERCALLBACK

  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(nsGeolocationRequest, nsIContentPermissionRequest)

  nsGeolocationRequest(mozilla::dom::Geolocation* locator,
                       const mozilla::dom::GeoPositionCallback& callback,
                       const mozilla::dom::GeoPositionErrorCallback& errorCallback,
                       mozilla::idl::GeoPositionOptions* aOptions,
                       bool watchPositionRequest = false,
                       int32_t watchId = 0);
  void Shutdown();

  // Called by the geolocation device to notify that a location has changed.
  // isBetter: the accuracy is as good or better than the previous position. 
  bool Update(nsIDOMGeoPosition* aPosition, bool aIsBetter);

  void SendLocation(nsIDOMGeoPosition* location, bool aCachePosition);
  void MarkCleared();
  bool WantsHighAccuracy() {return mOptions && mOptions->enableHighAccuracy;}
  bool IsActive() {return !mCleared;}
  bool Allowed() {return mAllowed;}
  void SetTimeoutTimer();
  nsIPrincipal* GetPrincipal();

  ~nsGeolocationRequest();

  bool Recv__delete__(const bool& allow);
  void IPDLRelease() { Release(); }

  int32_t WatchId() { return mWatchId; }
 private:

  void NotifyError(int16_t errorCode);
  bool mAllowed;
  bool mCleared;
  bool mIsFirstUpdate;
  bool mIsWatchPositionRequest;

  nsCOMPtr<nsITimer> mTimeoutTimer;
  mozilla::dom::GeoPositionCallback mCallback;
  mozilla::dom::GeoPositionErrorCallback mErrorCallback;
  nsAutoPtr<mozilla::idl::GeoPositionOptions> mOptions;

  nsRefPtr<mozilla::dom::Geolocation> mLocator;

  int32_t mWatchId;
};

/**
 * Singleton that manages the geolocation provider
 */
class nsGeolocationService MOZ_FINAL : public nsIGeolocationUpdate, public nsIObserver
{
public:

  static already_AddRefed<nsGeolocationService> GetGeolocationService();
  static nsRefPtr<nsGeolocationService> sService;

  NS_DECL_ISUPPORTS
  NS_DECL_NSIGEOLOCATIONUPDATE
  NS_DECL_NSIOBSERVER

  nsGeolocationService() {
      mHigherAccuracy = false;
  }

  nsresult Init();

  void HandleMozsettingChanged(const PRUnichar* aData);
  void HandleMozsettingValue(const bool aValue);

  // Management of the Geolocation objects
  void AddLocator(mozilla::dom::Geolocation* locator);
  void RemoveLocator(mozilla::dom::Geolocation* locator);

  void SetCachedPosition(nsIDOMGeoPosition* aPosition);
  nsIDOMGeoPosition* GetCachedPosition();
  bool IsBetterPosition(nsIDOMGeoPosition *aSomewhere);

  // Find and startup a geolocation device (gps, nmea, etc.)
  nsresult StartDevice(nsIPrincipal* aPrincipal, bool aRequestPrivate);

  // Stop the started geolocation device (gps, nmea, etc.)
  void     StopDevice();

  // create, or reinitalize the callback timer
  void     SetDisconnectTimer();

  // request higher accuracy, if possible
  void     SetHigherAccuracy(bool aEnable);
  bool     HighAccuracyRequested();

private:

  ~nsGeolocationService();

  // Disconnect timer.  When this timer expires, it clears all pending callbacks
  // and closes down the provider, unless we are watching a point, and in that
  // case, we disable the disconnect timer.
  nsCOMPtr<nsITimer> mDisconnectTimer;

  // The object providing geo location information to us.
  nsCOMArray<nsIGeolocationProvider> mProviders;

  // mGeolocators are not owned here.  Their constructor
  // adds them to this list, and their destructor removes
  // them from this list.
  nsTArray<mozilla::dom::Geolocation*> mGeolocators;

  // This is the last geo position that we have seen.
  nsCOMPtr<nsIDOMGeoPosition> mLastPosition;

  // Current state of requests for higher accuracy
  bool mHigherAccuracy;
};

namespace mozilla {
namespace dom {

/**
 * Can return a geolocation info
 */
class Geolocation MOZ_FINAL : public nsIDOMGeoGeolocation,
                                public nsWrapperCache
{
public:

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(Geolocation)

  NS_DECL_NSIDOMGEOGEOLOCATION

  Geolocation();

  nsresult Init(nsIDOMWindow* contentDom=nullptr);

  nsIDOMWindow* GetParentObject() const;
  virtual JSObject* WrapObject(JSContext *aCtx,
                               JS::Handle<JSObject*> aScope) MOZ_OVERRIDE;

  int32_t WatchPosition(PositionCallback& aCallback, PositionErrorCallback* aErrorCallback, const PositionOptions& aOptions, ErrorResult& aRv);
  void GetCurrentPosition(PositionCallback& aCallback, PositionErrorCallback* aErrorCallback, const PositionOptions& aOptions, ErrorResult& aRv);

  // Called by the geolocation device to notify that a location has changed.
  void Update(nsIDOMGeoPosition* aPosition, bool aIsBetter);

  void SetCachedPosition(Position* aPosition);
  Position* GetCachedPosition();

  // Returns true if any of the callbacks are repeating
  bool HasActiveCallbacks();

  // Remove request from all callbacks arrays
  void RemoveRequest(nsGeolocationRequest* request);

  // Shutting down.
  void Shutdown();

  // Getter for the principal that this Geolocation was loaded from
  nsIPrincipal* GetPrincipal() { return mPrincipal; }

  // Getter for the window that this Geolocation is owned by
  nsIWeakReference* GetOwner() { return mOwner; }

  // Check to see if the widnow still exists
  bool WindowOwnerStillExists();

  // Check to see if any active request requires high accuracy
  bool HighAccuracyRequested();

  // Notification from the service:
  void ServiceReady();

private:

  ~Geolocation();

  nsresult GetCurrentPosition(GeoPositionCallback& aCallback, GeoPositionErrorCallback& aErrorCallback, mozilla::idl::GeoPositionOptions* aOptions);
  nsresult WatchPosition(GeoPositionCallback& aCallback, GeoPositionErrorCallback& aErrorCallback, mozilla::idl::GeoPositionOptions* aOptions, int32_t* aRv);

  bool RegisterRequestWithPrompt(nsGeolocationRequest* request);

  // Methods for the service when it's ready to process requests:
  nsresult GetCurrentPositionReady(nsGeolocationRequest* aRequest);
  nsresult WatchPositionReady(nsGeolocationRequest* aRequest);

  // Two callback arrays.  The first |mPendingCallbacks| holds objects for only
  // one callback and then they are released/removed from the array.  The second
  // |mWatchingCallbacks| holds objects until the object is explictly removed or
  // there is a page change.

  nsTArray<nsRefPtr<nsGeolocationRequest> > mPendingCallbacks;
  nsTArray<nsRefPtr<nsGeolocationRequest> > mWatchingCallbacks;

  // window that this was created for.  Weak reference.
  nsWeakPtr mOwner;

  // where the content was loaded from
  nsCOMPtr<nsIPrincipal> mPrincipal;

  // owning back pointer.
  nsRefPtr<nsGeolocationService> mService;

  // cached Position wrapper
  nsRefPtr<Position> mCachedPosition;

  // Watch ID
  uint32_t mLastWatchId;

  // Pending requests are used when the service is not ready:
  class PendingRequest
  {
  public:
    nsRefPtr<nsGeolocationRequest> request;
    enum {
      GetCurrentPosition,
      WatchPosition
    } type;
  };

  nsTArray<PendingRequest> mPendingRequests;
};

class PositionError MOZ_FINAL : public nsIDOMGeoPositionError,
                                public nsWrapperCache
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(PositionError)

  NS_DECL_NSIDOMGEOPOSITIONERROR

  PositionError(Geolocation* aParent, int16_t aCode);

  Geolocation* GetParentObject() const;

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aScope) MOZ_OVERRIDE;

  int16_t Code() const {
    return mCode;
  }

  void GetMessage(nsString& aRetVal) const {
    aRetVal.Truncate();
  }

  void NotifyCallback(const GeoPositionErrorCallback& callback);
private:
  ~PositionError();
  int16_t mCode;
  nsRefPtr<Geolocation> mParent;
};

}
}

#endif /* nsGeoLocation_h */
