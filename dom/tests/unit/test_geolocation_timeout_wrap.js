const { HttpServer } = ChromeUtils.import("resource://testing-common/httpd.js");
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

var httpserver = null;

function run_test() {
  Services.prefs.setBoolPref("geo.wifi.scan", false);

  httpserver = new HttpServer();
  httpserver.start(-1);
  Services.prefs.setCharPref(
    "geo.wifi.uri",
    "http://localhost:" + httpserver.identity.primaryPort + "/geo"
  );
  Services.prefs.setBoolPref("dom.testing.ignore_ipc_principal", true);
  run_test_in_child("./test_geolocation_timeout.js");
}
