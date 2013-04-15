const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const Cr = Components.results;

Cu.import("resource://testing-common/httpd.js");

var httpserver = null;
var geolocation = null;
var success = false;
var watchId = -1;

let gAccuracy = 42;
function geoHandler(metadata, response)
{
  var georesponse = {
      status: "OK",
      location: {
          lat: 0.45,
          lng: 0.45,
      },
      accuracy: gAccuracy,
  };
  var position = JSON.stringify(georesponse);
  response.setStatusLine("1.0", 200, "OK");
  response.setHeader("Cache-Control", "no-cache", false);
  response.setHeader("Content-Type", "aplication/x-javascript", false);
  response.write(position);
}

function errorCallback() {
  do_check_true(false);
  do_test_finished();
}

function run_test()
{
  do_test_pending();

  httpserver = new HttpServer();
  httpserver.registerPathHandler("/geo", geoHandler);
  httpserver.start(4444);

  if (Cc["@mozilla.org/xre/app-info;1"].getService(Ci.nsIXULRuntime)
        .processType == Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT) {
    var prefs = Cc["@mozilla.org/preferences-service;1"].getService(Ci.nsIPrefBranch);
    prefs.setBoolPref("geo.wifi.scan", false);
    prefs.setCharPref("geo.wifi.uri", "http://localhost:4444/geo");
    prefs.setBoolPref("geo.testing.ignore_ipc_principal", true);
  }

  let timesCalled = 0;
  geolocation = Cc["@mozilla.org/geolocation;1"].createInstance(Ci.nsISupports);
  geolocation.watchPosition(function(pos) {
    do_check_eq(++timesCalled, 1);
    do_check_eq(pos.coords.accuracy, gAccuracy);

    gAccuracy = 420;
    geolocation2 = Cc["@mozilla.org/geolocation;1"].createInstance(Ci.nsISupports);
    geolocation2.getCurrentPosition(function(pos) {
      do_check_eq(pos.coords.accuracy, gAccuracy);

      gAccuracy = 400;
      geolocation2.getCurrentPosition(function(pos) {
        do_check_eq(pos.coords.accuracy, 42);
        do_test_finished();
      }, errorCallback);
    }, errorCallback, {maximumAge: 0});
  }, errorCallback, {maximumAge: 0});
}
