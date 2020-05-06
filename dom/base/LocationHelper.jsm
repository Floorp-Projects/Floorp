/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["LocationHelper"];

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

class LocationHelper {
  static formatWifiAccessPoints(accessPoints) {
    return accessPoints
      .filter(isPublic)
      .sort(sort)
      .map(encode);
  }
}
