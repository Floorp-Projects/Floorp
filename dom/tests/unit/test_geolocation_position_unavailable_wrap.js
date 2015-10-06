var Cc = Components.classes;
var Ci = Components.interfaces;
var Cu = Components.utils;


function run_test() {
  var prefs = Cc["@mozilla.org/preferences-service;1"].getService(Ci.nsIPrefBranch);
  prefs.setBoolPref("geo.wifi.scan", false);

  prefs.setCharPref("geo.wifi.uri", "UrlNotUsedHere");
  prefs.setBoolPref("dom.testing.ignore_ipc_principal", true);
  run_test_in_child("./test_geolocation_position_unavailable.js");
}
