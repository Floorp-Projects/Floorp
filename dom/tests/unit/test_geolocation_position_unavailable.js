var Cc = Components.classes;
var Ci = Components.interfaces;
var Cu = Components.utils;
var Cr = Components.results;


function successCallback() {
  do_check_true(false);
  do_test_finished();
}

function errorCallback(err) {
  do_check_eq(Ci.nsIDOMGeoPositionError.POSITION_UNAVAILABLE, err.code);
  do_test_finished();
}

function run_test()
{
  do_test_pending();

  if (Cc["@mozilla.org/xre/app-info;1"].getService(Ci.nsIXULRuntime)
        .processType == Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT) {
    // XPCShell does not get a profile by default. The geolocation service
    // depends on the settings service which uses IndexedDB and IndexedDB
    // needs a place where it can store databases.
    do_get_profile();

    var prefs = Cc["@mozilla.org/preferences-service;1"].getService(Ci.nsIPrefBranch);
    prefs.setBoolPref("geo.wifi.scan", false);
    prefs.setCharPref("geo.wifi.uri", "UrlNotUsedHere:");
    prefs.setBoolPref("dom.testing.ignore_ipc_principal", true);
  }

  geolocation = Cc["@mozilla.org/geolocation;1"].getService(Ci.nsISupports);
  geolocation.getCurrentPosition(successCallback, errorCallback);
}
