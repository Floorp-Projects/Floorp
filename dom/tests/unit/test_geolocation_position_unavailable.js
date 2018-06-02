function successCallback() {
  Assert.ok(false);
  do_test_finished();
}

function errorCallback(err) {
  // PositionError has no interface object, so we can't get constants off that.
  Assert.equal(err.POSITION_UNAVAILABLE, err.code);
  Assert.equal(2, err.code);
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
