/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GeolocationUtil.h"

double CalculateDeltaInMeter(double aLat, double aLon, double aLastLat, double aLastLon)
{
  // Use spherical law of cosines to calculate difference
  // Not quite as correct as the Haversine but simpler and cheaper
  const double radsInDeg = M_PI / 180.0;
  const double rNewLat = aLat * radsInDeg;
  const double rNewLon = aLon * radsInDeg;
  const double rOldLat = aLastLat * radsInDeg;
  const double rOldLon = aLastLon * radsInDeg;
  // WGS84 equatorial radius of earth = 6378137m
  double cosDelta = (sin(rNewLat) * sin(rOldLat)) +
                    (cos(rNewLat) * cos(rOldLat) * cos(rOldLon - rNewLon));
  if (cosDelta > 1.0) {
    cosDelta = 1.0;
  } else if (cosDelta < -1.0) {
    cosDelta = -1.0;
  }
  return acos(cosDelta) * 6378137;
}

