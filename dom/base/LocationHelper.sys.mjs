/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

function isPublic(ap) {
  let mask = "_nomap";
  let result = ap.ssid.indexOf(mask, ap.ssid.length - mask.length);
  return result == -1;
}

function sort(a, b) {
  return b.signal - a.signal;
}

function encode(ap) {
  return { macAddress: ap.mac, signalStrength: ap.signal };
}

/**
 * Shared utility functions for modules dealing with
 * Location Services.
 */

export class LocationHelper {
  static formatWifiAccessPoints(accessPoints) {
    return accessPoints.filter(isPublic).sort(sort).map(encode);
  }

  /**
   * Calculate the distance between 2 points using the Haversine formula.
   * https://en.wikipedia.org/wiki/Haversine_formula
   */
  static distance(p1, p2) {
    let rad = x => (x * Math.PI) / 180;
    // Radius of the earth.
    let R = 6371e3;
    let lat = rad(p2.lat - p1.lat);
    let lng = rad(p2.lng - p1.lng);

    let a =
      Math.sin(lat / 2) * Math.sin(lat / 2) +
      Math.cos(rad(p1.lat)) *
        Math.cos(rad(p2.lat)) *
        Math.sin(lng / 2) *
        Math.sin(lng / 2);

    let c = 2 * Math.atan2(Math.sqrt(a), Math.sqrt(1 - a));

    return R * c;
  }
}
