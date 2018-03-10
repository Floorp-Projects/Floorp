"use strict";

var harness = SimpleTest.harnessParameters.testRoot == "chrome" ? "chrome" : "tests";
var BASE_URL = "http://mochi.test:8888/" + harness + "/dom/tests/mochitest/geolocation/network_geolocation.sjs";

function sleep(delay)
{
    var start = Date.now();
    while (Date.now() < start + delay);
}

function force_prompt(allow, callback) {
  SpecialPowers.pushPrefEnv({"set": [["geo.prompt.testing", true], ["geo.prompt.testing.allow", allow]]}, callback);
}

function start_sending_garbage(callback)
{
  SpecialPowers.pushPrefEnv({"set": [["geo.wifi.uri", BASE_URL + "?action=respond-garbage"]]}, function() {
    // we need to be sure that all location data has been purged/set.
    sleep(1000);
    callback.call();
  });
}

function stop_sending_garbage(callback)
{
  SpecialPowers.pushPrefEnv({"set": [["geo.wifi.uri", BASE_URL + ""]]}, function() {
    // we need to be sure that all location data has been purged/set.
    sleep(1000);
    callback.call();
  });
}

function stop_geolocationProvider(callback)
{
  SpecialPowers.pushPrefEnv({"set": [["geo.wifi.uri", BASE_URL + "?action=stop-responding"]]}, function() {
    // we need to be sure that all location data has been purged/set.
    sleep(1000);
    callback.call();
  });
}

function set_network_request_cache_enabled(enabled, callback)
{
  SpecialPowers.pushPrefEnv({"set": [["geo.wifi.debug.requestCache.enabled", enabled]]}, callback);
}

function worse_geolocationProvider(callback)
{
  SpecialPowers.pushPrefEnv({"set": [["geo.wifi.uri", BASE_URL + "?action=worse-accuracy"]]}, callback);
}

function resume_geolocationProvider(callback)
{
  SpecialPowers.pushPrefEnv({"set": [["geo.wifi.uri", BASE_URL + ""]]}, callback);
}

function delay_geolocationProvider(delay, callback)
{
  SpecialPowers.pushPrefEnv({"set": [["geo.wifi.uri", BASE_URL + "?delay=" + delay]]}, callback);
}

function send404_geolocationProvider(callback)
{
  set_network_request_cache_enabled(false, function() {
    SpecialPowers.pushPrefEnv({"set": [["geo.wifi.uri", BASE_URL + "?action=send404"]]}, callback);});
}

function check_geolocation(location)
{
  ok(location, "Check to see if this location is non-null");

  const timestamp = location.timestamp;
  dump(`timestamp=$timestamp}\n`);
  ok(IsNumber(timestamp), "check timestamp type");
  ok(timestamp > 0, "check timestamp range");

  // eventually, coords may be optional (eg, when civic addresses are supported)
  ok("coords" in location, "Check to see if this location has a coords");

  const {
    latitude, longitude, accuracy, altitude, altitudeAccuracy, speed, heading
  } = location.coords;

  dump(`latitude=${latitude}\n`);
  dump(`longitude=${longitude}\n`);
  dump(`accuracy=${accuracy}\n`);
  dump(`altitude=${altitude}\n`);
  dump(`altitudeAccuracy=${altitudeAccuracy}\n`);
  dump(`speed=${speed}\n`);
  dump(`heading=${heading}\n`);

  ok(IsNumber(latitude), "check latitude type");
  ok(IsNumber(longitude), "check longitude type");

  ok(Math.abs(latitude - 37.41857) < 0.001, "latitude matches hard-coded value");
  ok(Math.abs(longitude + 122.08769) < 0.001, "longitude matches hard-coded value");

  ok(IsNonNegativeNumber(accuracy), "check accuracy type and range");
  ok(IsNumber(altitude) || altitude === null, "check accuracy type");

  ok((IsNonNegativeNumber(altitudeAccuracy) && IsNumber(altitude)) ||
     (altitudeAccuracy === null), "check altitudeAccuracy type and range");

  ok(IsNonNegativeNumber(speed) || speed === null, "check speed type and range");

  ok((IsNonNegativeNumber(heading) && heading < 360 && speed > 0) ||
     heading === null, "check heading type and range");
}

function IsNumber(x)
{
  return typeof(x) === "number" && !Number.isNaN(x);
}

function IsNonNegativeNumber(x)
{
  return IsNumber(x) && x >= 0;
}
