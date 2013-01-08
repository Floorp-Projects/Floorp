const Cc = Components.classes;
const Ci = Components.interfaces;

function run_test() {
  var prefs = Cc["@mozilla.org/preferences-service;1"].getService(Ci.nsIPrefBranch);
  prefs.setBoolPref("geo.wifi.scan", false);
  prefs.setCharPref("geo.wifi.uri", "http://localhost:4444/geo");  
  prefs.setBoolPref("geo.testing.ignore_ipc_principal", true);
  run_test_in_child("./test_geolocation_timeout.js");
}
