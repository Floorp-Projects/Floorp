/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 10000;

let geolocation = window.navigator.geolocation;
ok(geolocation);

var target = Object();
var wpid;

/**
 * Grant special power to get the geolocation
 */
SpecialPowers.addPermission("geolocation", true, document);

/**
 * Helper that compares the geolocation against the web API.
 */
function verifyLocation(callback, expectedLocation) {
  geolocation.getCurrentPosition(function(position) {
    log("Expected location: " + expectedLocation.latitude + " " + expectedLocation.longitude);
    log("Current location: " + position.coords.latitude + " " + position.coords.longitude);
    is(expectedLocation.latitude, position.coords.latitude);
    is(expectedLocation.longitude, position.coords.longitude);
  });
  window.setTimeout(callback, 0);
}

/**
 * Test story begins here.
 */
function setup() {
  log("Providing initial setup: set geographic position watcher.");

  wpid = geolocation.watchPosition(function(position) {
    log("Position changes has found.");    
    log("Watch: Target location: " + target.latitude + " " + target.longitude);
    log("Watch: Current location: " + position.coords.latitude + " " + position.coords.longitude);
    is(target.latitude, position.coords.latitude, "Latitude isn't match!");
    is(target.longitude, position.coords.longitude, "Longitude isn't match!");
  });

  target.latitude  = 0;
  target.longitude = 0;

  cmd = "geo fix " + target.longitude + " " + target.latitude;

  runEmulatorCmd(cmd, function(result) {
    log("Geolocation setting status: " + result);
    verifyLocation(movePosition_1, target);
  });

}

function movePosition_1() {
  log("Geolocation changes. Move to Position 1.");

  target.latitude  = 25;
  target.longitude = 121.56499833333334;

  cmd = "geo fix " + target.longitude + " " + target.latitude;

  runEmulatorCmd(cmd, function(result) {
    log("Geolocation setting status: " + result);
    verifyLocation(movePosition_2, target);
  });
}

function movePosition_2() {
  log("Geolocation changes to a negative longitude. Move to Position 2.");

  target.latitude  = 37.393;
  target.longitude = -122.08199833333335;

  cmd = "geo fix " + target.longitude + " " + target.latitude;

  runEmulatorCmd(cmd, function(result) {
    log("Geolocation setting status: " + result);
    verifyLocation(movePosition_3, target);
  });
}

function movePosition_3() {
  log("Geolocation changes with WatchPosition. Move to Position 3.");

  target.latitude  = -22;
  target.longitude = -43;

  cmd = "geo fix " + target.longitude + " " + target.latitude;

  runEmulatorCmd(cmd, function(result) {
    log("Geolocation setting status: " + result);
    verifyLocation(cleanup, target);
  });
}

function cleanup() {
  geolocation.clearWatch(wpid);
  SpecialPowers.removePermission("geolocation", document);
  finish();
}

setup();
