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

#include "MaemoLocationProvider.h"
#include "nsGeolocation.h"
#include "nsIClassInfo.h"
#include "nsDOMClassInfoID.h"

////////////////////////////////////////////////////
// nsGeoPositionCoords
////////////////////////////////////////////////////

class nsGeoPositionCoords : public nsIDOMGeoPositionCoords
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMGEOPOSITIONCOORDS
  
  nsGeoPositionCoords(double aLat, double aLong,
                      double aAlt, double aHError,
                      double aVError, double aHeading,
                      double aSpeed)
    : mLat(aLat),
      mLong(aLong),
      mAlt(aAlt),
      mHError(aHError),
      mVError(aVError),
      mHeading(aHeading),
      mSpeed(aSpeed)
  {
  }
  
  
  ~nsGeoPositionCoords(){}

private:
  double mLat, mLong, mAlt, mHError, mVError, mHeading, mSpeed;
};

NS_INTERFACE_MAP_BEGIN(nsGeoPositionCoords)
NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMGeoPositionCoords)
NS_INTERFACE_MAP_ENTRY(nsIDOMGeoPositionCoords)
NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(GeoPositionCoords)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(nsGeoPositionCoords)
NS_IMPL_RELEASE(nsGeoPositionCoords)

NS_IMETHODIMP
nsGeoPositionCoords::GetLatitude(double *aLatitude)
{
  *aLatitude = mLat;
  return NS_OK;
}

NS_IMETHODIMP
nsGeoPositionCoords::GetLongitude(double *aLongitude)
{
  *aLongitude = mLong;
  return NS_OK;
}

NS_IMETHODIMP
nsGeoPositionCoords::GetAltitude(double *aAltitude)
{
  *aAltitude = mAlt;
  return NS_OK;
}

NS_IMETHODIMP
nsGeoPositionCoords::GetAccuracy(double *aAccuracy)
{
  *aAccuracy = mHError;
  return NS_OK;
}

NS_IMETHODIMP
nsGeoPositionCoords::GetAltitudeAccuracy(double *aAltitudeAccuracy)
{
  *aAltitudeAccuracy = mVError;
  return NS_OK;
}

NS_IMETHODIMP
nsGeoPositionCoords::GetHeading(double *aHeading)
{
  *aHeading = mHeading;
  return NS_OK;
}

NS_IMETHODIMP
nsGeoPositionCoords::GetSpeed(double *aSpeed)
{
  *aSpeed = mSpeed;
  return NS_OK;
}

////////////////////////////////////////////////////
// nsGeoPosition
////////////////////////////////////////////////////

class nsGeoPosition : public nsIDOMGeoPosition
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMGEOPOSITION

    nsGeoPosition(double aLat, double aLong,
                  double aAlt, double aHError,
                  double aVError, double aHeading,
                  double aSpeed, long long aTimestamp)
      : mLat(aLat),
        mLong(aLong),
        mAlt(aAlt),
        mHError(aHError),
        mVError(aVError),
        mHeading(aHeading),
        mSpeed(aSpeed),
        mTimestamp(aTimestamp)
  {
  }

private:
  ~nsGeoPosition()
  {
  }
  double mLat, mLong, mAlt, mHError, mVError, mHeading, mSpeed;
  long long mTimestamp;
  nsRefPtr<nsGeoPositionCoords> mCoords;
};

NS_INTERFACE_MAP_BEGIN(nsGeoPosition)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMGeoPosition)
  NS_INTERFACE_MAP_ENTRY(nsIDOMGeoPosition)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(GeoPosition)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(nsGeoPosition)
NS_IMPL_RELEASE(nsGeoPosition)

NS_IMETHODIMP
nsGeoPosition::GetCoords(nsIDOMGeoPositionCoords * *aCoords)
{
  if (mCoords == nsnull)
    mCoords = new nsGeoPositionCoords(mLat,
                                      mLong,
                                      mAlt,
                                      mHError,
                                      mVError,
                                      mHeading,
                                      mSpeed);
  NS_IF_ADDREF(*aCoords = mCoords);
  return NS_OK;
}


NS_IMETHODIMP
nsGeoPosition::GetTimestamp(DOMTimeStamp* aTimestamp)
{
  *aTimestamp = mTimestamp;
  return NS_OK;
}


NS_IMPL_ISUPPORTS1(MaemoLocationProvider, nsIGeolocationProvider)

MaemoLocationProvider::MaemoLocationProvider()
: mGPSDevice(nsnull),
  mHasSeenLocation(PR_FALSE),
  mLastSeenTime(0)
{
}

MaemoLocationProvider::~MaemoLocationProvider()
{
}

static void location_changed(LocationGPSDevice *device, gpointer userdata)
{
  if (!userdata || !device || !device->fix) {
    return;
  }

  //  printf ("Latitude: %.5f\nLongitude: %.5f\nAltitude: %.2f\n",
  //	  device->fix->latitude, device->fix->longitude, device->fix->altitude);

  MaemoLocationProvider* provider = (MaemoLocationProvider*) userdata;
  nsRefPtr<nsGeoPosition> somewhere = new nsGeoPosition(device->fix->latitude,
                                                        device->fix->longitude,
                                                        device->fix->altitude,
                                                        0,
                                                        0,
                                                        0,
                                                        0,
                                                        PR_Now());
  provider->Update(somewhere);
}

NS_IMETHODIMP MaemoLocationProvider::Startup()
{
  if (!mGPSDevice)
  {
    mGPSDevice = (LocationGPSDevice*) g_object_new (LOCATION_TYPE_GPS_DEVICE, NULL);

    mCallbackChanged = g_signal_connect(mGPSDevice, 
                                        "changed",
                                        G_CALLBACK(location_changed),
                                        this);
  }

  return NS_OK;
}

NS_IMETHODIMP MaemoLocationProvider::IsReady(PRBool *_retval NS_OUTPARAM)
{
  *_retval = mHasSeenLocation;
  return NS_OK;
}

NS_IMETHODIMP MaemoLocationProvider::Watch(nsIGeolocationUpdate *callback)
{
  if (mCallback)
    return NS_OK;

  mCallback = callback;
  return NS_OK;
}

NS_IMETHODIMP MaemoLocationProvider::Shutdown()
{
  if (mGPSDevice)
  {
    g_signal_handler_disconnect(mGPSDevice, mCallbackChanged);

    g_object_unref(mGPSDevice);
    mGPSDevice = nsnull;

    mHasSeenLocation = PR_FALSE;
    
    mCallback = nsnull;
  }
  return NS_OK;
}

void MaemoLocationProvider::Update(nsIDOMGeoPosition* aPosition)
{
  if ((PR_Now() - mLastSeenTime) / PR_USEC_PER_SEC < 5)
    return;

  mHasSeenLocation = PR_TRUE;

  if (mCallback)
    mCallback->Update(aPosition);

  mLastSeenTime = PR_Now();
}
