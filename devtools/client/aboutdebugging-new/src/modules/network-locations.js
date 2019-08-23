/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");

const NETWORK_LOCATIONS_PREF = "devtools.aboutdebugging.network-locations";

/**
 * This module provides a collection of helper methods to read and update network
 * locations monitored by about-debugging.
 */

function addNetworkLocationsObserver(listener) {
  Services.prefs.addObserver(NETWORK_LOCATIONS_PREF, listener);
}
exports.addNetworkLocationsObserver = addNetworkLocationsObserver;

function removeNetworkLocationsObserver(listener) {
  Services.prefs.removeObserver(NETWORK_LOCATIONS_PREF, listener);
}
exports.removeNetworkLocationsObserver = removeNetworkLocationsObserver;

/**
 * Read the current preference value for aboutdebugging network locations.
 * Will throw if the value cannot be parsed or is not an array.
 */
function _parsePreferenceAsArray() {
  const pref = Services.prefs.getStringPref(NETWORK_LOCATIONS_PREF, "[]");
  const parsedValue = JSON.parse(pref);
  if (!Array.isArray(parsedValue)) {
    throw new Error("Expected array value in " + NETWORK_LOCATIONS_PREF);
  }
  return parsedValue;
}

function getNetworkLocations() {
  try {
    return _parsePreferenceAsArray();
  } catch (e) {
    Services.prefs.clearUserPref(NETWORK_LOCATIONS_PREF);
    return [];
  }
}
exports.getNetworkLocations = getNetworkLocations;

function addNetworkLocation(location) {
  const locations = getNetworkLocations();
  const locationsSet = new Set(locations);
  locationsSet.add(location);

  Services.prefs.setStringPref(
    NETWORK_LOCATIONS_PREF,
    JSON.stringify([...locationsSet])
  );
}
exports.addNetworkLocation = addNetworkLocation;

function removeNetworkLocation(location) {
  const locations = getNetworkLocations();
  const locationsSet = new Set(locations);
  locationsSet.delete(location);

  Services.prefs.setStringPref(
    NETWORK_LOCATIONS_PREF,
    JSON.stringify([...locationsSet])
  );
}
exports.removeNetworkLocation = removeNetworkLocation;
