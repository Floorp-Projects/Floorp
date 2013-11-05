
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
  SpecialPowers.pushPrefEnv({"set": [["geo.wifi.uri", "http://mochi.test:8888/tests/dom/tests/mochitest/geolocation/network_geolocation.sjs?action=respond-garbage"]]}, function() {
    // we need to be sure that all location data has been purged/set.
    sleep(1000);
    callback.call();
  });
}

function stop_sending_garbage(callback)
{
  SpecialPowers.pushPrefEnv({"set": [["geo.wifi.uri", "http://mochi.test:8888/tests/dom/tests/mochitest/geolocation/network_geolocation.sjs"]]}, function() {
    // we need to be sure that all location data has been purged/set.
    sleep(1000);
    callback.call();
  });
}

function stop_geolocationProvider(callback)
{
  SpecialPowers.pushPrefEnv({"set": [["geo.wifi.uri", "http://mochi.test:8888/tests/dom/tests/mochitest/geolocation/network_geolocation.sjs?action=stop-responding"]]}, function() {
    // we need to be sure that all location data has been purged/set.
    sleep(1000);
    callback.call();
  });
}

function worse_geolocationProvider(callback)
{
  SpecialPowers.pushPrefEnv({"set": [["geo.wifi.uri", "http://mochi.test:8888/tests/dom/tests/mochitest/geolocation/network_geolocation.sjs?action=worse-accuracy"]]}, callback);
}

function resume_geolocationProvider(callback)
{
  SpecialPowers.pushPrefEnv({"set": [["geo.wifi.uri", "http://mochi.test:8888/tests/dom/tests/mochitest/geolocation/network_geolocation.sjs"]]}, callback);
}

function delay_geolocationProvider(delay, callback)
{
  SpecialPowers.pushPrefEnv({"set": [["geo.wifi.uri", "http://mochi.test:8888/tests/dom/tests/mochitest/geolocation/network_geolocation.sjs?delay=" + delay]]}, callback);
}

function send404_geolocationProvider(callback)
{
  SpecialPowers.pushPrefEnv({"set": [["geo.wifi.uri", "http://mochi.test:8888/tests/dom/tests/mochitest/geolocation/network_geolocation.sjs?action=send404"]]}, callback);
}

function check_geolocation(location) {

  ok(location, "Check to see if this location is non-null");

  ok("timestamp" in location, "Check to see if there is a timestamp");

  // eventually, coords may be optional (eg, when civic addresses are supported)
  ok("coords" in location, "Check to see if this location has a coords");

  var coords = location.coords;

  ok("latitude" in coords, "Check to see if there is a latitude");
  ok("longitude" in coords, "Check to see if there is a longitude");
  ok("accuracy" in coords, "Check to see if there is a accuracy");
  
  // optional ok("altitude" in coords, "Check to see if there is a altitude");
  // optional ok("altitudeAccuracy" in coords, "Check to see if there is a alt accuracy");
  // optional ok("heading" in coords, "Check to see if there is a heading");
  // optional ok("speed" in coords, "Check to see if there is a speed");

  ok (location.coords.latitude  == 37.41857, "lat matches known value");
  ok (location.coords.longitude == -122.08769, "lon matches known value");
  // optional  ok(location.coords.altitude == 42, "alt matches known value");
  // optional  ok(location.coords.altitudeAccuracy == 42, "alt acc matches known value");
}

function toggleGeolocationSetting(value, callback) {
  var mozSettings = window.navigator.mozSettings;
  var lock = mozSettings.createLock();

  var geoenabled = {"geolocation.enabled": value};

  req = lock.set(geoenabled);
  req.onsuccess = function () {
    ok(true, "set done");
    callback();
  }
}
