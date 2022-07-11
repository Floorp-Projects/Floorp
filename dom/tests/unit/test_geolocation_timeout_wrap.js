const { HttpServer } = ChromeUtils.import("resource://testing-common/httpd.js");

var httpserver = null;

function run_test() {
  Services.prefs.setBoolPref("geo.provider.network.scan", false);

  httpserver = new HttpServer();
  httpserver.start(-1);
  Services.prefs.setCharPref(
    "geo.provider.network.url",
    "http://localhost:" + httpserver.identity.primaryPort + "/geo"
  );
  run_test_in_child("./test_geolocation_timeout.js");
}
