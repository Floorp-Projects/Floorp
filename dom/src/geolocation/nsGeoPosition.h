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

#ifndef nsGeoPosition_h
#define nsGeoPosition_h

#include "nsAutoPtr.h"
#include "nsIClassInfo.h"
#include "nsDOMClassInfoID.h"
#include "nsIDOMGeoPositionAddress.h"
#include "nsIDOMGeoPositionCoords.h"
#include "nsIDOMGeoPosition.h"
#include "nsString.h"

////////////////////////////////////////////////////
// nsGeoPositionAddress
////////////////////////////////////////////////////

class nsGeoPositionAddress : public nsIDOMGeoPositionAddress
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMGEOPOSITIONADDRESS

  nsGeoPositionAddress( const nsAString &aStreetNumber,
                        const nsAString &aStreet,
                        const nsAString &aPremises,
                        const nsAString &aCity,
                        const nsAString &aCounty,
                        const nsAString &aRegion,
                        const nsAString &aCountry,
                        const nsAString &aCountryCode,
                        const nsAString &aPostalCode);

  private:
    ~nsGeoPositionAddress();
    const nsString mStreetNumber;
    const nsString mStreet;
    const nsString mPremises;
    const nsString mCity;
    const nsString mCounty;
    const nsString mRegion;
    const nsString mCountry;
    const nsString mCountryCode;
    const nsString mPostalCode;
};

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
                      double aSpeed);
private:
  ~nsGeoPositionCoords();
  const double mLat, mLong, mAlt, mHError, mVError, mHeading, mSpeed;
};


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
                double aSpeed, long long aTimestamp);
  

  nsGeoPosition(nsIDOMGeoPositionCoords *aCoords,
                long long aTimestamp);

  nsGeoPosition(nsIDOMGeoPositionCoords *aCoords,
                nsIDOMGeoPositionAddress *aAddress,
                DOMTimeStamp aTimestamp);

private:
  ~nsGeoPosition();
  long long mTimestamp;
  nsRefPtr<nsIDOMGeoPositionCoords> mCoords;
  nsRefPtr<nsIDOMGeoPositionAddress> mAddress;
};

#endif /* nsGeoPosition_h */

