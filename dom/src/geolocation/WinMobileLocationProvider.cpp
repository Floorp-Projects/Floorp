/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Doug Turner <dougt@meer.net>  (Original Author)
 *  Nino D'Aversa <ninodaversa@gmail.com>
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

#include "WinMobileLocationProvider.h"
#include "nsGeolocation.h"
#include "nsIClassInfo.h"
#include "nsDOMClassInfoID.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsIServiceManager.h"

////////////////////////////////////////////////////
// nsGeoPositionCoords
////////////////////////////////////////////////////

/**
 * Simple object that holds a single point in space.
 */
class nsGeoPositionCoords : public nsIDOMGeoPositionCoords
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMGEOPOSITIONCOORDS
  
  nsGeoPositionCoords(double aLat, double aLong,
                      double aAlt, double aHError,
                      double aVError, double aHeading,
                      double aSpeed) :
    mLat(aLat), mLong(aLong),
    mAlt(aAlt), mHError(aHError),
    mVError(aVError), mHeading(aHeading),
    mSpeed(aSpeed) {};
private:
  ~nsGeoPositionCoords() {}
  double mLat, mLong, mAlt, mHError, mVError, mHeading, mSpeed;
};

NS_INTERFACE_MAP_BEGIN(nsGeoPositionCoords)
NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMGeoPositionCoords)
NS_INTERFACE_MAP_ENTRY(nsIDOMGeoPositionCoords)
NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(GeoPositionCoords)
NS_INTERFACE_MAP_END

NS_IMPL_THREADSAFE_ADDREF(nsGeoPositionCoords)
NS_IMPL_THREADSAFE_RELEASE(nsGeoPositionCoords)

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
                double aSpeed, long long aTimestamp): mTimestamp(aTimestamp)
  {
    mCoords = new nsGeoPositionCoords(aLat, aLong,
                                      aAlt, aHError,
                                      aVError, aHeading,
                                      aSpeed);
    NS_ASSERTION(mCoords, "null mCoords in nsGeoPosition");
  };
  
private:
  ~nsGeoPosition() {}
  long long mTimestamp;
  nsRefPtr<nsGeoPositionCoords> mCoords;
};

NS_INTERFACE_MAP_BEGIN(nsGeoPosition)
NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMGeoPosition)
NS_INTERFACE_MAP_ENTRY(nsIDOMGeoPosition)
NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(GeoPosition)
NS_INTERFACE_MAP_END

NS_IMPL_THREADSAFE_ADDREF(nsGeoPosition)
NS_IMPL_THREADSAFE_RELEASE(nsGeoPosition)

NS_IMETHODIMP
nsGeoPosition::GetTimestamp(DOMTimeStamp* aTimestamp)
{
  *aTimestamp = mTimestamp;
  return NS_OK;
}

NS_IMETHODIMP
nsGeoPosition::GetCoords(nsIDOMGeoPositionCoords * *aCoords)
{
  NS_IF_ADDREF(*aCoords = mCoords);
  return NS_OK;
}

NS_IMETHODIMP
nsGeoPosition::GetAddress(nsIDOMGeoPositionAddress** aAddress)
{
  *aAddress = nsnull;
  return NS_OK;
}

NS_IMPL_ISUPPORTS2(WinMobileLocationProvider, nsIGeolocationProvider, nsITimerCallback)

WinMobileLocationProvider::WinMobileLocationProvider() :
  mGPSDevice(nsnull), 
  mOpenDevice(nsnull),
  mCloseDevice(nsnull),
  mGetPosition(nsnull),
  mGetDeviceState(nsnull),
  mHasSeenLocation(PR_FALSE),
  mHasGPS(PR_TRUE) /* think positively */
{
}

WinMobileLocationProvider::~WinMobileLocationProvider()
{
}

NS_IMETHODIMP
WinMobileLocationProvider::Notify(nsITimer* aTimer)
{
  if (!mGPSDevice || !mGetPosition)
    return NS_ERROR_FAILURE;

  GPS_POSITION pos;
  memset(&pos, 0, sizeof(GPS_POSITION));
  pos.dwVersion = GPS_VERSION_1;
  pos.dwSize = sizeof(GPS_POSITION);
  
  //TODO: add ability to control maximum age with the PositionOptions
  //100ms is maximum age of GPS data
  mGetPosition(mGPSDevice, &pos, 100, 0);
  
  if ((!(pos.dwValidFields & GPS_VALID_LATITUDE) &&
       !(pos.dwValidFields & GPS_VALID_LONGITUDE) ) ||
      pos.dwFlags == GPS_DATA_FLAGS_HARDWARE_OFF ||
      pos.FixQuality == GPS_FIX_QUALITY_UNKNOWN)
    return NS_OK;

  nsRefPtr<nsGeoPosition> somewhere =
    new nsGeoPosition(pos.dblLatitude,
                      pos.dblLongitude,
                      (double)pos.flAltitudeWRTSeaLevel,
                      (double)pos.flHorizontalDilutionOfPrecision,
                      (double)pos.flVerticalDilutionOfPrecision,
                      (double)pos.flHeading,
                      (double)pos.flSpeed,
                      PR_Now());
  Update(somewhere);
  return NS_OK;
}

NS_IMETHODIMP WinMobileLocationProvider::Startup()
{
  if (mHasGPS && !mGPSInst) {
    mGPSInst = LoadLibraryW(L"gpsapi.dll");
    
    if(!mGPSInst) {
      mHasGPS = PR_FALSE;
      NS_ASSERTION(mGPSInst, "failed to load library gpsapi.dll");
      return NS_ERROR_FAILURE;
    }

    mOpenDevice  = (OpenDeviceProc) GetProcAddress(mGPSInst,"GPSOpenDevice");
    mCloseDevice = (CloseDeviceProc) GetProcAddress(mGPSInst,"GPSCloseDevice");
    mGetPosition = (GetPositionProc) GetProcAddress(mGPSInst,"GPSGetPosition");
    mGetDeviceState = (GetDeviceStateProc) GetProcAddress(mGPSInst,"GPSGetDeviceState");

    if (!mOpenDevice || !mCloseDevice || !mGetPosition || !mGetDeviceState)
      return NS_ERROR_FAILURE;

    mGPSDevice = mOpenDevice(NULL, NULL, NULL, 0);

    if (!mGPSDevice) {
      NS_ASSERTION(mGPSDevice, "GPS Device not found");
      return NS_ERROR_FAILURE;
    }
    nsresult rv;
    mUpdateTimer = do_CreateInstance("@mozilla.org/timer;1", &rv);

    if (NS_FAILED(rv))
      return NS_ERROR_FAILURE;

    nsCOMPtr<nsIPrefBranch> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
    PRInt32 update = 3000; //3 second default timer
    if (prefs)
      prefs->GetIntPref("geo.default.update", &update);

    mUpdateTimer->InitWithCallback(this, update, nsITimer::TYPE_REPEATING_SLACK);
  }
  return NS_OK;
}

NS_IMETHODIMP WinMobileLocationProvider::Watch(nsIGeolocationUpdate *callback)
{
  if (mCallback)
    return NS_OK;
  
  mCallback = callback;
  return NS_OK;
}

NS_IMETHODIMP WinMobileLocationProvider::Shutdown()
{
  if (mUpdateTimer)
    mUpdateTimer->Cancel();
  
  if (mGPSDevice)
    mCloseDevice(mGPSDevice);

  mGPSDevice = nsnull;
  
  mHasSeenLocation = PR_FALSE;
  
  mCallback = nsnull;
  
  FreeLibrary(mGPSInst);
  mGPSInst = nsnull;

  mOpenDevice  = nsnull;
  mCloseDevice = nsnull;
  mGetPosition = nsnull;
  mGetDeviceState = nsnull;
 
  return NS_OK;
}

void WinMobileLocationProvider::Update(nsIDOMGeoPosition* aPosition)
{
  mHasSeenLocation = PR_TRUE;
  if (mCallback)
    mCallback->Update(aPosition);
}
