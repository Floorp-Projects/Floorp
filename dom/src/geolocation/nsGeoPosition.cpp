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
 * The Initial Developer of the Original Code is Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Doug Turner <dougt@meer.net>  (Original Author)
 *  Nino D'Aversa <ninodaversa@gmail.com>
 *  Mike Kristoffersen <moz@mikek.dk>
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

#include "nsGeoPosition.h"
#include "nsDOMClassInfo.h"

////////////////////////////////////////////////////
// nsGeoPositionAddress
////////////////////////////////////////////////////

nsGeoPositionAddress::nsGeoPositionAddress(const nsAString &aStreetNumber,
                                           const nsAString &aStreet,
                                           const nsAString &aPremises,
                                           const nsAString &aCity,
                                           const nsAString &aCounty,
                                           const nsAString &aRegion,
                                           const nsAString &aCountry,
                                           const nsAString &aCountryCode,
                                           const nsAString &aPostalCode)
    : mStreetNumber(aStreetNumber)
    , mStreet(aStreet)
    , mPremises(aPremises)
    , mCity(aCity)
    , mCounty(aCounty)
    , mRegion(aRegion)
    , mCountry(aCountry)
    , mCountryCode(aCountryCode)
    , mPostalCode(aPostalCode)
{
}

nsGeoPositionAddress::~nsGeoPositionAddress()
{
}

DOMCI_DATA(GeoPositionAddress, nsGeoPositionAddress)

NS_INTERFACE_MAP_BEGIN(nsGeoPositionAddress)
NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMGeoPositionAddress)
NS_INTERFACE_MAP_ENTRY(nsIDOMGeoPositionAddress)
NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(GeoPositionAddress)
NS_INTERFACE_MAP_END

NS_IMPL_THREADSAFE_ADDREF(nsGeoPositionAddress)
NS_IMPL_THREADSAFE_RELEASE(nsGeoPositionAddress)

NS_IMETHODIMP
nsGeoPositionAddress::GetStreetNumber(nsAString & aStreetNumber)
{
  aStreetNumber = mStreetNumber;
  return NS_OK;
}

NS_IMETHODIMP
nsGeoPositionAddress::GetStreet(nsAString & aStreet)
{
  aStreet = mStreet;
  return NS_OK;
}

NS_IMETHODIMP
nsGeoPositionAddress::GetPremises(nsAString & aPremises)
{
  aPremises = mPremises;
  return NS_OK;
}

NS_IMETHODIMP
nsGeoPositionAddress::GetCity(nsAString & aCity)
{
  aCity = mCity;
  return NS_OK;
}

NS_IMETHODIMP
nsGeoPositionAddress::GetCounty(nsAString & aCounty)
{
  aCounty = mCounty;
  return NS_OK;
}

NS_IMETHODIMP
nsGeoPositionAddress::GetRegion(nsAString & aRegion)
{
  aRegion = mRegion;
  return NS_OK;
}

NS_IMETHODIMP
nsGeoPositionAddress::GetCountry(nsAString & aCountry)
{
  aCountry = mCountry;
  return NS_OK;
}

NS_IMETHODIMP
nsGeoPositionAddress::GetCountryCode(nsAString & aCountryCode)
{
  aCountryCode = mCountryCode;
  return NS_OK;
}

NS_IMETHODIMP
nsGeoPositionAddress::GetPostalCode(nsAString & aPostalCode)
{
  aPostalCode = mPostalCode;
  return NS_OK;
}

////////////////////////////////////////////////////
// nsGeoPositionCoords
////////////////////////////////////////////////////
nsGeoPositionCoords::nsGeoPositionCoords(double aLat, double aLong,
                                         double aAlt, double aHError,
                                         double aVError, double aHeading,
                                         double aSpeed)
  : mLat(aLat)
  , mLong(aLong)
  , mAlt(aAlt)
  , mHError(aHError)
  , mVError(aVError)
  , mHeading(aHeading)
  , mSpeed(aSpeed)
{
}

nsGeoPositionCoords::~nsGeoPositionCoords()
{
}

DOMCI_DATA(GeoPositionCoords, nsGeoPositionCoords)

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

nsGeoPosition::nsGeoPosition(double aLat, double aLong,
                             double aAlt, double aHError,
                             double aVError, double aHeading,
                             double aSpeed, long long aTimestamp) :
    mTimestamp(aTimestamp)
{
    mCoords = new nsGeoPositionCoords(aLat, aLong,
                                      aAlt, aHError,
                                      aVError, aHeading,
                                      aSpeed);
    NS_ASSERTION(mCoords, "null mCoords in nsGeoPosition");
}

nsGeoPosition::nsGeoPosition(nsIDOMGeoPositionCoords *aCoords,
                             long long aTimestamp) :
    mCoords(aCoords),
    mTimestamp(aTimestamp)
{
}

nsGeoPosition::nsGeoPosition(nsIDOMGeoPositionCoords *aCoords,
                             nsIDOMGeoPositionAddress *aAddress,
                             DOMTimeStamp aTimestamp) :
  mTimestamp(aTimestamp),
  mCoords(aCoords),
  mAddress(aAddress)
{
}

nsGeoPosition::~nsGeoPosition()
{
}

DOMCI_DATA(GeoPosition, nsGeoPosition)

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
  NS_IF_ADDREF(*aAddress = mAddress);
  return NS_OK;
}

