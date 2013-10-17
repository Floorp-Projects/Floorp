/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsGeoLocation_h
#define nsGeoLocation_h

// Microsoft's API Name hackery sucks
#undef CreateEvent

#include "mozilla/StaticPtr.h"
#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "nsTArray.h"
#include "nsITimer.h"
#include "nsIObserver.h"
#include "nsWrapperCache.h"

#include "nsWeakPtr.h"
#include "nsCycleCollectionParticipant.h"

#include "nsGeoPosition.h"
#include "nsIDOMGeoGeolocation.h"
#include "nsIDOMGeoPosition.h"
#include "nsIDOMGeoPositionError.h"
#include "nsIDOMGeoPositionCallback.h"
#include "nsIDOMGeoPositionErrorCallback.h"
#include "mozilla/dom/GeolocationBinding.h"
#include "mozilla/dom/PositionErrorBinding.h"
#include "mozilla/dom/CallbackObject.h"

#include "nsIGeolocationProvider.h"
#include "nsIContentPermissionPrompt.h"
#include "nsIDOMWindow.h"
#include "DictionaryHelpers.h"
#include "mozilla/Attributes.h"

class nsGeolocationService;
class nsGeolocationRequest;

namespace mozilla {
namespace dom {
class Geolocation;
typedef CallbackObjectHolder<PositionCallback, nsIDOMGeoPositionCallback> GeoPositionCallback;
typedef CallbackObjectHolder<PositionErrorCallback, nsIDOMGeoPositionErrorCallback> GeoPositionErrorCallback;
}
}

/**
 * Singleton that manages the geolocation provider
 */
class nsGeolocationService MOZ_FINAL : public nsIGeolocationUpdate, public nsIObserver
{
public:

  static already_AddRefed<nsGeolocationService> GetGeolocationService();
  static mozilla::StaticRefPtr<nsGeolocationService> sService;

  NS_DECL_THREADSAFE_ISUPPORTS
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

  // Find and startup a geolocation device (gps, nmea, etc.)
  nsresult StartDevice(nsIPrincipal* aPrincipal);

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
  nsCOMPtr<nsIGeolocationProvider> mProvider;

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
                              public nsIGeolocationUpdate,
                              public nsWrapperCache
{
public:

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_AMBIGUOUS(Geolocation, nsIDOMGeoGeolocation)

  NS_DECL_NSIGEOLOCATIONUPDATE
  NS_DECL_NSIDOMGEOGEOLOCATION

  Geolocation();

  nsresult Init(nsIDOMWindow* contentDom=nullptr);

  nsIDOMWindow* GetParentObject() const;
  virtual JSObject* WrapObject(JSContext *aCtx,
                               JS::Handle<JSObject*> aScope) MOZ_OVERRIDE;

  int32_t WatchPosition(PositionCallback& aCallback, PositionErrorCallback* aErrorCallback, const PositionOptions& aOptions, ErrorResult& aRv);
  void GetCurrentPosition(PositionCallback& aCallback, PositionErrorCallback* aErrorCallback, const PositionOptions& aOptions, ErrorResult& aRv);

  void SetCachedPosition(Position* aPosition);
  Position* GetCachedPosition();

  // Returns true if any of the callbacks are repeating
  bool HasActiveCallbacks();

  // Register an allowed request
  void NotifyAllowedRequest(nsGeolocationRequest* aRequest);

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

  nsresult GetCurrentPosition(GeoPositionCallback& aCallback, GeoPositionErrorCallback& aErrorCallback, PositionOptions* aOptions);
  nsresult WatchPosition(GeoPositionCallback& aCallback, GeoPositionErrorCallback& aErrorCallback, PositionOptions* aOptions, int32_t* aRv);

  bool RegisterRequestWithPrompt(nsGeolocationRequest* request);

  // Methods for the service when it's ready to process requests:
  nsresult GetCurrentPositionReady(nsGeolocationRequest* aRequest);
  nsresult WatchPositionReady(nsGeolocationRequest* aRequest);

  // Two callback arrays.  The first |mPendingCallbacks| holds objects for only
  // one callback and then they are released/removed from the array.  The second
  // |mWatchingCallbacks| holds objects until the object is explictly removed or
  // there is a page change. All requests held by either array are active, that
  // is, they have been allowed and expect to be fulfilled.

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

  // Pending requests are used when the service is not ready
  nsTArray<nsRefPtr<nsGeolocationRequest> > mPendingRequests;
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

  void NotifyCallback(const GeoPositionErrorCallback& callback);
private:
  ~PositionError();
  int16_t mCode;
  nsRefPtr<Geolocation> mParent;
};

}

inline nsISupports*
ToSupports(dom::Geolocation* aGeolocation)
{
  return ToSupports(static_cast<nsIDOMGeoGeolocation*>(aGeolocation));
}
}

#endif /* nsGeoLocation_h */
