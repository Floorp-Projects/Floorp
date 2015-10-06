var Cc = Components.classes;
var Ci = Components.interfaces;
var Cu = Components.utils;

Cu.import("resource://testing-common/httpd.js");

var httpserver = null;
var geolocation = null;

function geoHandler(metadata, response)
{
  response.processAsync();
}

function successCallback() {
  // The call shouldn't be sucessful.
  do_check_true(false);
  do_test_finished();
}

function errorCallback() {
  do_check_true(true);
  do_test_finished();
}

function run_test()
{
  do_test_pending();

  // XPCShell does not get a profile by default. The geolocation service
  // depends on the settings service which uses IndexedDB and IndexedDB
  // needs a place where it can store databases.
  do_get_profile();

  httpserver = new HttpServer();
  httpserver.registerPathHandler("/geo", geoHandler);
  httpserver.start(-1);
  var prefs = Cc["@mozilla.org/preferences-service;1"].getService(Ci.nsIPrefBranch);
  prefs.setCharPref("geo.wifi.uri", "http://localhost:" +
                      httpserver.identity.primaryPort + "/geo");
  prefs.setBoolPref("dom.testing.ignore_ipc_principal", true);
  prefs.setBoolPref("geo.wifi.scan", false);

  // Setting timeout to a very low value to ensure time out will happen.
  prefs.setIntPref("geo.wifi.xhr.timeout", 5);

  geolocation = Cc["@mozilla.org/geolocation;1"].getService(Ci.nsISupports);
  geolocation.getCurrentPosition(successCallback, errorCallback);
}
