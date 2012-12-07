function test() {
  var prefs = Components.classes["@mozilla.org/preferences-service;1"].getService(Components.interfaces.nsIPrefBranch);
  let baseProvider = "http://mochi.test:8888/browser/dom/tests/browser/network_geolocation.sjs";
  prefs.setCharPref("geo.wifi.uri", baseProvider + "?desired_access_token=fff");

  let pb = Cc["@mozilla.org/privatebrowsing;1"].
           getService(Ci.nsIPrivateBrowsingService);

  prefs.setBoolPref("geo.prompt.testing", true);
  prefs.setBoolPref("geo.prompt.testing.allow", true);

  prefs.setBoolPref("browser.privatebrowsing.keep_current_session", true);

  const testPageURL = "http://mochi.test:8888/browser/" +
    "dom/tests/browser/browser_geolocation_privatebrowsing_page.html";
  waitForExplicitFinish();

  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("georesult", function load(ev) {
    gBrowser.selectedBrowser.removeEventListener("georesult", load, false);
    is(ev.detail, 200, "unexpected access token");
    gBrowser.removeCurrentTab();

    prefs.setCharPref("geo.wifi.uri", baseProvider + "?desired_access_token=ggg");

    pb.privateBrowsingEnabled = true;
    gBrowser.selectedTab = gBrowser.addTab();
    gBrowser.selectedBrowser.addEventListener("georesult", function load2(ev) {
      gBrowser.selectedBrowser.removeEventListener("georesult", load2, false);
      is(ev.detail, 200, "unexpected access token");
      gBrowser.removeCurrentTab();

      prefs.setCharPref("geo.wifi.uri", baseProvider + "?expected_access_token=fff");
      pb.privateBrowsingEnabled = false;
      gBrowser.selectedTab = gBrowser.addTab();
      gBrowser.selectedBrowser.addEventListener("georesult", function load3(ev) {
        gBrowser.selectedBrowser.removeEventListener("georesult", load3, false);
        is(ev.detail, 200, "unexpected access token");
        gBrowser.removeCurrentTab();
        prefs.setBoolPref("geo.prompt.testing", false);
        prefs.setBoolPref("geo.prompt.testing.allow", false);
        prefs.clearUserPref("browser.privatebrowsing.keep_current_session");
        finish();
      }, false, true);
      content.location = testPageURL;
    }, false, true);
    content.location = testPageURL;
  }, false, true);
  content.location = testPageURL;
}
