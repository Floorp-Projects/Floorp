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
 * The Original Code is Geolocation.
 *
 * The Initial Developer of the Original Code is Mozilla Corporation
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Doug Turner <dougt@meer.net>  (Original Author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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


#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "nsCOMArray.h"
#include "nsTArray.h"
#include "nsITimer.h"
#include "nsIObserver.h"
#include "nsIURI.h"

#include "nsIDOMGeolocation.h"
#include "nsIDOMGeolocation.h"
#include "nsIDOMGeolocationCallback.h"
#include "nsIDOMGeolocator.h"
#include "nsIDOMNavigatorGeolocator.h"

#include "nsPIDOMWindow.h"

#include "nsIGeolocationProvider.h"

class nsGeolocator;
class nsGeolocatorService;


class nsGeolocationRequest : public nsIGeolocationRequest
{
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIGEOLOCATIONREQUEST

  nsGeolocationRequest(nsGeolocator* locator, nsIDOMGeolocationCallback* callback);
  void Shutdown();

  void SendLocation(nsIDOMGeolocation* location);
  void MarkCleared();
  PRBool Allowed() {return mAllowed;}

  ~nsGeolocationRequest();

 private:
  PRBool mAllowed;
  PRBool mCleared;
  PRBool mFuzzLocation;

  nsCOMPtr<nsIDOMGeolocationCallback> mCallback;

  nsGeolocator* mLocator; // The locator exists alonger than this object.
};

/**
 * Simple object that holds a single point in space.
 */ 
class nsGeolocation : public nsIDOMGeolocation
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMGEOLOCATION

    nsGeolocation(double aLat, double aLong, double aAlt, double aHError, double aVError, long long aTimestamp)
    : mLat(aLat), mLong(aLong), mAlt(aAlt), mHError(aHError), mVError(aVError), mTimestamp(aTimestamp){};

private:
  ~nsGeolocation(){}
  double mLat, mLong, mAlt, mHError, mVError;
  long long mTimestamp;
};

/**
 * Singleton that manages the geolocation provider
 */
class nsGeolocatorService : public nsIGeolocationService, public nsIGeolocationUpdate, public nsIObserver
{
public:

  static nsGeolocatorService* GetGeolocationService();
  static nsGeolocatorService* GetInstance();  // Non-Addref'ing
  static nsGeolocatorService* gService;

  NS_DECL_ISUPPORTS
  NS_DECL_NSIGEOLOCATIONUPDATE
  NS_DECL_NSIOBSERVER
  NS_DECL_NSIGEOLOCATIONSERVICE

  nsGeolocatorService();

  // Management of the nsGeolocator objects
  void AddLocator(nsGeolocator* locator);
  void RemoveLocator(nsGeolocator* locator);

  // Returns the last geolocation we have seen since calling StartDevice()
  already_AddRefed<nsIDOMGeolocation> GetLastKnownPosition();
  
  // Returns the application defined UI prompt
  nsIGeolocationPrompt* GetPrompt() { return mPrompt; } // does not addref.

  // Returns true if the we have successfully found and started a
  // geolocation device
  PRBool   IsDeviceReady();

  // Find and startup a geolocation device (gps, nmea, etc.)
  nsresult StartDevice();

  // Stop the started geolocation device (gps, nmea, etc.)
  void     StopDevice();
  
  // create, or reinitalize the callback timer
  void     SetDisconnectTimer();

private:

  ~nsGeolocatorService();

  // Disconnect timer.  When this timer expires, it clears all pending callbacks
  // and closes down the provider, unless we are watching a point, and in that
  // case, we disable the disconnect timer.
  nsCOMPtr<nsITimer> mDisconnectTimer;

  // Time, in milliseconds, to wait for the location provider to spin up.
  PRInt32 mTimeout;

  // The object providing geo location information to us.
  nsCOMPtr<nsIGeolocationProvider> mProvider;

  // mGeolocators are not owned here.  Their constructor
  // addes them to this list, and their destructor removes
  // them from this list.
  nsTArray<nsGeolocator*> mGeolocators;

  // prompt callback, if any
  nsCOMPtr<nsIGeolocationPrompt> mPrompt;
};


/**
 * Can return a geolocation info
 */ 
class nsGeolocator : public nsIDOMGeolocator
{
public:

  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMGEOLOCATOR

  nsGeolocator(nsIDOMWindow* contentDom);

  // Called by the geolocation device to notify that a location has changed.
  void Update(nsIDOMGeolocation* aLocation);

  // Returns true if any of the callbacks are repeating
  PRBool   HasActiveCallbacks();

  // Remove request from all callbacks arrays
  void     RemoveRequest(nsGeolocationRequest* request);

  // Shutting down.
  void     Shutdown();

  // Setter and Getter of the URI that this nsGeolocator was loaded from
  nsIURI* GetURI() { return mURI; }

  // Setter and Getter of the window that this nsGeolocator is owned by
  nsIDOMWindow* GetOwner() { return mOwner; }

  // Check to see if the widnow still exists
  PRBool OwnerStillExists();

private:

  ~nsGeolocator();

  // Two callback arrays.  The first |mPendingCallbacks| holds objects for only
  // one callback and then they are released/removed from the array.  The second
  // |mWatchingCallbacks| holds objects until the object is explictly removed or
  // there is a page change.

  nsCOMArray<nsGeolocationRequest> mPendingCallbacks;
  nsCOMArray<nsGeolocationRequest> mWatchingCallbacks;

  PRBool mUpdateInProgress;

  // window that this was created for.  Weak reference.
  nsPIDOMWindow* mOwner;

  // where the content was loaded from
  nsCOMPtr<nsIURI> mURI;

  // owning back pointer.
  nsRefPtr<nsGeolocatorService> mService;
};
