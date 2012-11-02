/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 10000;

let geolocation = window.navigator.geolocation;
ok(geolocation);

var sample = [];
var result = [];
var wpid;

/**
 * Grant special power to get the geolocation
 */
SpecialPowers.addPermission("geolocation", true, document);

/**
 * Disable wifi geolocation provider
 */
wifiUri = SpecialPowers.getCharPref("geo.wifi.uri");
SpecialPowers.setCharPref("geo.wifi.uri", "http://mochi.test:8888/tests/dom/tests/mochitest/geolocation/network_geolocation.sjs?action=stop-responding");

/**
 * Helper that compares the geolocation against the web API.
 */
function verifyLocation() {

  log("Sample:" + sample.join(','));
  log("Result:" + result.join(','));

  for (i in sample) {
    is(sample.pop(), result.pop());
  }

  window.setTimeout(cleanup, 0);
}

/**
 * Test story begins here.
 */
function setup() {
  log("Providing initial setup: set geographic position watcher.");


  wpid = geolocation.watchPosition(function(position) {
    log("Position changes: (" + position.coords.latitude + "/" + position.coords.longitude + ")");
    result.push(""+position.coords.latitude + "/" + position.coords.longitude);
  });

  lat = 0;
  lon = 0;

  cmd = "geo fix " + lon + " " + lat;
  sample.push(lat+"/"+lon);

  runEmulatorCmd(cmd, function(result) {
    window.setTimeout(movePosition_1, 0);
  });
}

function movePosition_1() {
  log("Geolocation changes. Move to Position 1.");

  lat = 25;
  lon = 121.56499833333334;

  cmd = "geo fix " + lon + " " + lat;
  sample.push(lat+"/"+lon);

  runEmulatorCmd(cmd, function(result) {
    window.setTimeout(movePosition_2, 0);
  });
}

function movePosition_2() {
  log("Geolocation changes to a negative longitude. Move to Position 2.");

  lat = 37.393;
  lon = -122.08199833333335;

  cmd = "geo fix " + lon + " " + lat;
  sample.push(lat+"/"+lon);

  runEmulatorCmd(cmd, function(result) {
    window.setTimeout(movePosition_3, 0);
  });
}

function movePosition_3() {
  log("Geolocation changes with WatchPosition. Move to Position 3.");

  lat = -22;
  lon = -43;

  cmd = "geo fix " + lon + " " + lat;
  sample.push(lat+"/"+lon);

  geolocation.getCurrentPosition(function(position) {    
    log("getCurrentPosition: Expected location: ("+lat+"/"+lon+"); Current location: (" + position.coords.latitude + "/" + position.coords.longitude + ")");
    is(lat, position.coords.latitude);
    is(lon, position.coords.longitude);
  });

  runEmulatorCmd(cmd, function(result) {
    window.setTimeout(verifyLocation, 0);
  });
}

function cleanup() {
  geolocation.clearWatch(wpid);
  SpecialPowers.removePermission("geolocation", document);
  SpecialPowers.setCharPref("geo.wifi.uri", wifiUri);
  finish();
}

setup();
