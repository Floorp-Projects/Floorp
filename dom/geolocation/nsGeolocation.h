/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsGeoLocation_h
#define nsGeoLocation_h

// Microsoft's API Name hackery sucks
#undef CreateEvent

#include "mozilla/StaticPtr.h"
#include "nsCOMPtr.h"
#include "nsTArray.h"
#include "nsITimer.h"
#include "nsIObserver.h"
#include "nsWrapperCache.h"

#include "nsWeakPtr.h"
#include "nsCycleCollectionParticipant.h"

#include "nsGeoPosition.h"
#include "nsIDOMGeoPosition.h"
#include "nsIDOMGeoPositionCallback.h"
#include "nsIDOMGeoPositionErrorCallback.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/GeolocationBinding.h"
#include "mozilla/dom/CallbackObject.h"

#include "nsIGeolocationProvider.h"
#include "nsIContentPermissionPrompt.h"
#include "mozilla/Attributes.h"

class nsGeolocationService;
class nsGeolocationRequest;

namespace mozilla {
namespace dom {
class Geolocation;
typedef CallbackObjectHolder<PositionCallback, nsIDOMGeoPositionCallback> GeoPositionCallback;
typedef CallbackObjectHolder<PositionErrorCallback, nsIDOMGeoPositionErrorCallback> GeoPositionErrorCallback;
} // namespace dom
} // namespace mozilla

struct CachedPositionAndAccuracy {
  nsCOMPtr<nsIDOMGeoPosition> position;
  bool isHighAccuracy;
};

/**
 * Singleton that manages the geolocation provider
 */
class nsGeolocationService final : public nsIGeolocationUpdate,
                                   public nsIObserver
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

  // Management of the Geolocation objects
  void AddLocator(mozilla::dom::Geolocation* locator);
  void RemoveLocator(mozilla::dom::Geolocation* locator);

  void SetCachedPosition(nsIDOMGeoPosition* aPosition);
  CachedPositionAndAccuracy GetCachedPosition();

  // Find and startup a geolocation device (gps, nmea, etc.)
  nsresult StartDevice(nsIPrincipal* aPrincipal);

  // Stop the started geolocation device (gps, nmea, etc.)
  void     StopDevice();

  // create, or reinitalize the callback timer
  void     SetDisconnectTimer();

  // Update the accuracy and notify the provider if changed
  void     UpdateAccuracy(bool aForceHigh = false);
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
  CachedPositionAndAccuracy mLastPosition;

  // Current state of requests for higher accuracy
  bool mHigherAccuracy;
};

namespace mozilla {
namespace dom {

/**
 * Can return a geolocation info
 */
class Geolocation final : public nsIGeolocationUpdate,
                          public nsWrapperCache
{
public:

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(Geolocation)

  NS_DECL_NSIGEOLOCATIONUPDATE

  Geolocation();

  nsresult Init(nsPIDOMWindowInner* aContentDom = nullptr);

  nsPIDOMWindowInner* GetParentObject() const;
  virtual JSObject* WrapObject(JSContext *aCtx, JS::Handle<JSObject*> aGivenProto) override;

  int32_t WatchPosition(PositionCallback& aCallback,
                        PositionErrorCallback* aErrorCallback,
                        const PositionOptions& aOptions,
                        CallerType aCallerType,
                        ErrorResult& aRv);
  void GetCurrentPosition(PositionCallback& aCallback,
                          PositionErrorCallback* aErrorCallback,
                          const PositionOptions& aOptions,
                          CallerType aCallerType,
                          ErrorResult& aRv);
  void ClearWatch(int32_t aWatchId);

  // A WatchPosition for C++ use.  Returns -1 if we failed to actually watch.
  int32_t WatchPosition(nsIDOMGeoPositionCallback* aCallback,
                        nsIDOMGeoPositionErrorCallback* aErrorCallback,
                        UniquePtr<PositionOptions>&& aOptions);

  // Returns true if any of the callbacks are repeating
  bool HasActiveCallbacks();

  // Register an allowed request
  void NotifyAllowedRequest(nsGeolocationRequest* aRequest);

  // Remove request from all callbacks arrays
  void RemoveRequest(nsGeolocationRequest* request);

  // Check if there is already ClearWatch called for current
  // request & clear if yes
  bool ClearPendingRequest(nsGeolocationRequest* aRequest);

  // Shutting down.
  void Shutdown();

  // Getter for the principal that this Geolocation was loaded from
  nsIPrincipal* GetPrincipal() { return mPrincipal; }

  // Getter for the window that this Geolocation is owned by
  nsIWeakReference* GetOwner() { return mOwner; }

  // Check to see if the window still exists
  bool WindowOwnerStillExists();

  // Check to see if any active request requires high accuracy
  bool HighAccuracyRequested();

  // Get the singleton non-window Geolocation instance.  This never returns null.
  static already_AddRefed<Geolocation> NonWindowSingleton();

private:

  ~Geolocation();

  nsresult GetCurrentPosition(GeoPositionCallback aCallback,
                              GeoPositionErrorCallback aErrorCallback,
                              UniquePtr<PositionOptions>&& aOptions,
                              CallerType aCallerType);
  int32_t WatchPosition(GeoPositionCallback aCallback,
                        GeoPositionErrorCallback aErrorCallback,
                        UniquePtr<PositionOptions>&& aOptions,
                        CallerType aCallerType,
                        ErrorResult& aRv);

  bool RegisterRequestWithPrompt(nsGeolocationRequest* request);

  // Check if clearWatch is already called
  bool IsAlreadyCleared(nsGeolocationRequest* aRequest);

  // Returns whether the Geolocation object should block requests
  // within a context that is not secure.
  bool ShouldBlockInsecureRequests() const;

  // Two callback arrays.  The first |mPendingCallbacks| holds objects for only
  // one callback and then they are released/removed from the array.  The second
  // |mWatchingCallbacks| holds objects until the object is explictly removed or
  // there is a page change. All requests held by either array are active, that
  // is, they have been allowed and expect to be fulfilled.

  nsTArray<RefPtr<nsGeolocationRequest> > mPendingCallbacks;
  nsTArray<RefPtr<nsGeolocationRequest> > mWatchingCallbacks;

  // window that this was created for.  Weak reference.
  nsWeakPtr mOwner;

  // where the content was loaded from
  nsCOMPtr<nsIPrincipal> mPrincipal;

  // the protocols we want to measure
  enum class ProtocolType: uint8_t { OTHER, HTTP, HTTPS };

  // the protocol used to load the content
  ProtocolType mProtocolType;

  // owning back pointer.
  RefPtr<nsGeolocationService> mService;

  // Watch ID
  uint32_t mLastWatchId;

  // Pending requests are used when the service is not ready
  nsTArray<RefPtr<nsGeolocationRequest> > mPendingRequests;

  // Array containing already cleared watch IDs
  nsTArray<int32_t> mClearedWatchIDs;

  // Our cached non-window singleton.
  static mozilla::StaticRefPtr<Geolocation> sNonWindowSingleton;
};

} // namespace dom
} // namespace mozilla

#endif /* nsGeoLocation_h */
