const { HttpServer } = ChromeUtils.import("resource://testing-common/httpd.js");
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

var httpserver = null;
var geolocation = null;

function geoHandler(metadata, response) {
  response.processAsync();
}

function successCallback() {
  // The call shouldn't be sucessful.
  Assert.ok(false);
  do_test_finished();
}

function errorCallback() {
  Assert.ok(true);
  do_test_finished();
}

function run_test() {
  do_test_pending();

  // XPCShell does not get a profile by default. The geolocation service
  // depends on the settings service which uses IndexedDB and IndexedDB
  // needs a place where it can store databases.
  do_get_profile();

  httpserver = new HttpServer();
  httpserver.registerPathHandler("/geo", geoHandler);
  httpserver.start(-1);
  Services.prefs.setCharPref(
    "geo.wifi.uri",
    "http://localhost:" + httpserver.identity.primaryPort + "/geo"
  );
  Services.prefs.setBoolPref("dom.testing.ignore_ipc_principal", true);
  Services.prefs.setBoolPref("geo.wifi.scan", false);

  // Setting timeout to a very low value to ensure time out will happen.
  Services.prefs.setIntPref("geo.wifi.xhr.timeout", 5);

  geolocation = Cc["@mozilla.org/geolocation;1"].getService(Ci.nsISupports);
  geolocation.getCurrentPosition(successCallback, errorCallback);
}
