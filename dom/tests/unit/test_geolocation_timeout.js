const { HttpServer } = ChromeUtils.import("resource://testing-common/httpd.js");
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

var httpserver = null;
var geolocation = null;
var success = false;
var watchId = -1;

function geoHandler(metadata, response) {
  var georesponse = {
    status: "OK",
    location: {
      lat: 42,
      lng: 42,
    },
    accuracy: 42,
  };
  var position = JSON.stringify(georesponse);
  response.processAsync();
  response.setStatusLine("1.0", 200, "OK");
  response.setHeader("Cache-Control", "no-cache", false);
  response.setHeader("Content-Type", "aplication/x-javascript", false);
  do_timeout(5000, function() {
    response.write(position);
    response.finish();
  });
}

function successCallback() {
  Assert.ok(false);
  do_test_finished();
}

function errorCallback() {
  Assert.ok(true);
  do_test_finished();
}

function run_test() {
  do_test_pending();

  if (Services.appinfo.processType == Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT) {
    // XPCShell does not get a profile by default. The geolocation service
    // depends on the settings service which uses IndexedDB and IndexedDB
    // needs a place where it can store databases.
    do_get_profile();

    httpserver = new HttpServer();
    httpserver.registerPathHandler("/geo", geoHandler);
    httpserver.start(-1);
    Services.prefs.setBoolPref("geo.wifi.scan", false);
    Services.prefs.setCharPref(
      "geo.wifi.uri",
      "http://localhost:" + httpserver.identity.primaryPort + "/geo"
    );
    Services.prefs.setBoolPref("dom.testing.ignore_ipc_principal", true);
  }

  geolocation = Cc["@mozilla.org/geolocation;1"].getService(Ci.nsISupports);
  geolocation.getCurrentPosition(successCallback, errorCallback, {
    timeout: 2000,
  });
}
