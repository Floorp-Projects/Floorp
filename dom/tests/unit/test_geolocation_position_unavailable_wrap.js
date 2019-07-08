const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

function run_test() {
  Services.prefs.setBoolPref("geo.wifi.scan", false);

  Services.prefs.setCharPref("geo.wifi.uri", "UrlNotUsedHere");
  Services.prefs.setBoolPref("dom.testing.ignore_ipc_principal", true);
  run_test_in_child("./test_geolocation_position_unavailable.js");
}
