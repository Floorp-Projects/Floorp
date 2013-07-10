function test() {
  var prefs = Components.classes["@mozilla.org/preferences-service;1"].getService(Components.interfaces.nsIPrefBranch);
  let baseProvider = "http://mochi.test:8888/browser/dom/tests/browser/network_geolocation.sjs";
  prefs.setCharPref("geo.wifi.uri", baseProvider + "?desired_access_token=fff");

  prefs.setBoolPref("geo.prompt.testing", true);
  prefs.setBoolPref("geo.prompt.testing.allow", true);
  var origScanValue = true; // same default in NetworkGeolocationProvider.js.
  try {
    origScanValue = prefs.getBoolPref("geo.wifi.scan");
  } catch(ex) {}
  prefs.setBoolPref("geo.wifi.scan", false);

  const testPageURL = "http://mochi.test:8888/browser/" +
    "dom/tests/browser/browser_geolocation_privatebrowsing_page.html";
  waitForExplicitFinish();

  var windowsToClose = [];
  function testOnWindow(aIsPrivate, aCallback) {
    let win = OpenBrowserWindow({private: aIsPrivate});
    let gotLoad = false;
    let gotActivate = 
      (Cc["@mozilla.org/focus-manager;1"].getService(Ci.nsIFocusManager).activeWindow == win);
    if (!gotActivate) {
      win.addEventListener("activate", function onActivate() {
        info("got activate");
        win.removeEventListener("activate", onActivate, true);
        gotActivate = true;
        if (gotLoad) {
          windowsToClose.push(win);
          win.BrowserChromeTest.runWhenReady(function() { aCallback(win) });
        }
      }, true);
    } else {
      info("Was activated");
    }
    win.addEventListener("load", function onLoad() {
      info("Got load");
      win.removeEventListener("load", onLoad, true);
      gotLoad = true;
      if (gotActivate) {
        windowsToClose.push(win);
        setTimeout(function() { aCallback(win) }, 1000);
      }
    }, true);
  }

  testOnWindow(false, function(aNormalWindow) {
    aNormalWindow.gBrowser.selectedBrowser.addEventListener("georesult", function load(ev) {
      aNormalWindow.gBrowser.selectedBrowser.removeEventListener("georesult", load, false);
      is(ev.detail, 200, "unexpected access token");

      prefs.setCharPref("geo.wifi.uri", baseProvider + "?desired_access_token=ggg");

      testOnWindow(true, function(aPrivateWindow) {
        aPrivateWindow.gBrowser.selectedBrowser.addEventListener("georesult", function load2(ev) {
          aPrivateWindow.gBrowser.selectedBrowser.removeEventListener("georesult", load2, false);
          is(ev.detail, 200, "unexpected access token");

          prefs.setCharPref("geo.wifi.uri", baseProvider + "?expected_access_token=fff");

          testOnWindow(false, function(aAnotherNormalWindow) {
            aAnotherNormalWindow.gBrowser.selectedBrowser.addEventListener("georesult", function load3(ev) {
              aAnotherNormalWindow.gBrowser.selectedBrowser.removeEventListener("georesult", load3, false);
              is(ev.detail, 200, "unexpected access token");
              prefs.setBoolPref("geo.prompt.testing", false);
              prefs.setBoolPref("geo.prompt.testing.allow", false);
              prefs.setBoolPref("geo.wifi.scan", origScanValue);
              windowsToClose.forEach(function(win) {
                                       win.close();
                                     });
              finish();
            }, false, true);
            aAnotherNormalWindow.content.location = testPageURL;
          });
        }, false, true);
        aPrivateWindow.content.location = testPageURL;
      });
    }, false, true);
    aNormalWindow.content.location = testPageURL;
  });
}
