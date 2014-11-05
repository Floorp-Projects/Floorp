/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#include <math.h>
#include "nsGeoGridFuzzer.h"
#include "nsGeoPosition.h"


#ifdef MOZ_APPROX_LOCATION

/* The following constants are taken from the World Geodetic System 1984 (WGS84)
 * reference model for the earth ellipsoid [1].  The values in the model are
 * an accepted standard for GPS and other navigational systems.
 *
 * [1] http://www.oosa.unvienna.org/pdf/icg/2012/template/WGS_84.pdf
 */
#define WGS84_a         (6378137.0)           // equitorial axis
#define WGS84_b         (6356752.314245179)   // polar axis (a * (1-f))
#define WGS84_f         (1.0/298.257223563)   // inverse flattening
#define WGS84_EPSILON   (5.72957795e-9)       // 1e-10 radians in degrees
#define sq(f)           ((f) * (f))
#define sign(f)         (((f) < 0) ? -1 : 1)

/* if you have an ellipsoid with semi-major axis A and semi-minor axis B, the
 * radius at angle phi along the semi-major axis can be calculated with this
 * formula.  by using the WGS84 values for A and B, we calculate the radius of
 * earth, given the angle of latitude, phi.*/
#define LON_RADIUS(phi) (sqrt((sq(sq(WGS84_a) * cos(phi)) + sq(sq(WGS84_b) * sin(phi))) / \
                              (sq(WGS84_a * cos(phi)) + sq(WGS84_b * sin(phi)))))
/* the radius of earth changes as a function of latitude, to simplify I am
 * assuming the fixed radius of the earth halfway between the poles and the
 * equator.  this is calculated from LON_RADIUS(M_PI/4), or the radius at
 * 45 degrees N.*/
#define LAT_RADIUS          (6367489.543863)

/* This function figures out the latitudinal grid square that the given
 * latitude coordinate falls into and then returns the latitudinal center of
 * that grid square.  It handles the proper wrapping at the poles +/- 90
 * (e.g. +95 wraps to +85 and -95 wraps to -85) */
static double GridAlgorithmLat(int32_t aDistMeters, double aLatDeg)
{
  /* switch to radians */
  double phi = (aLatDeg * M_PI) / 180;

  /* properly wrap the latitude */
  phi = atan(sin(phi) / fabs(cos(phi)));

  /* calculate grid size in radians */
  double gridSizeRad = aDistMeters / LAT_RADIUS;

  /* find the southern edge, in radians, of the grid cell, then add half of a
   * grid cell to find the center latitude in radians */
  double gridCenterPhi = gridSizeRad * floor(phi / gridSizeRad) + gridSizeRad / 2;

  /* properly wrap it and return it in degrees */
  return atan(sin(gridCenterPhi) / fabs(cos(gridCenterPhi))) * (180.0 / M_PI);
}

/* This function figures out the longitudinal grid square that the given longitude
 * coordinate falls into and then returns the longitudinal center of that grid
 * square.  It handles the proper wrapping at +/- 180 (e.g. +185 wraps to -175
 * and -185 wraps to +175) */
static double GridAlgorithmLon(int32_t aDistMeters, double aLatDeg, double aLonDeg)
{
  /* switch to radians */
  double phi = (aLatDeg * M_PI) / 180;
  double theta = (aLonDeg * M_PI) / 180;

  /* properly wrap the lat/lon */
  phi = atan(sin(phi) / fabs(cos(phi)));
  theta = atan2(sin(theta), cos(theta));

  /* calculate grid size in radians */
  double gridSizeRad = aDistMeters / LON_RADIUS(phi);

  /* find the western edge, in radians, of the grid cell, then add half of a
   * grid cell to find the center longitude in radians */
  double gridCenterTheta = gridSizeRad * floor(theta / gridSizeRad) + gridSizeRad / 2;

  /* properly wrap it and return it in degrees */
  return atan2(sin(gridCenterTheta), cos(gridCenterTheta)) * (180.0 / M_PI);
}

/* This function takes the grid size and the graticule coordinates of a
 * location and calculates which grid cell the coordinates fall within and
 * then returns the coordinates of the geographical center of the grid square.
 */
static void CalculateGridCoords(int32_t aDistKm, double&  aLatDeg, double& aLonDeg)
{
  // a grid size of 0 is the same as precise
  if (aDistKm == 0) {
    return;
  }
  aLonDeg = GridAlgorithmLon(aDistKm * 1000, aLatDeg, aLonDeg);
  aLatDeg = GridAlgorithmLat(aDistKm * 1000, aLatDeg);
}

already_AddRefed<nsIDOMGeoPosition>
nsGeoGridFuzzer::FuzzLocation(const GeolocationSetting & aSetting,
                              nsIDOMGeoPosition * aPosition)
{
  if (!aPosition) {
    return nullptr;
  }

  nsCOMPtr<nsIDOMGeoPositionCoords> coords;
  nsresult rv = aPosition->GetCoords(getter_AddRefs(coords));
  NS_ENSURE_SUCCESS(rv, nullptr);
  if (!coords) {
   return nullptr;
  }

  double lat = 0.0, lon = 0.0;
  coords->GetLatitude(&lat);
  coords->GetLongitude(&lon);

  // adjust lat/lon to be the center of the grid square
  CalculateGridCoords(aSetting.GetApproxDistance(), lat, lon);
  GPSLOG("approximate location with delta %d is %f, %f",
         aSetting.GetApproxDistance(), lat, lon);

  // reusing the timestamp
  DOMTimeStamp ts;
  rv = aPosition->GetTimestamp(&ts);
  NS_ENSURE_SUCCESS(rv, nullptr);

  // return a position at sea level, N heading, 0 speed, 0 error.
  nsRefPtr<nsGeoPosition> pos = new nsGeoPosition(lat, lon, 0.0, 0.0,
                                                  0.0, 0.0, 0.0, ts);
  return pos.forget();
}

#endif
