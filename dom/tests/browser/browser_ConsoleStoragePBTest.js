/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  try {
    var pb = Cc["@mozilla.org/privatebrowsing;1"].getService(Ci.nsIPrivateBrowsingService);
  } catch (ex) {
    ok(true, "nothing to do here, PB service doesn't exist");
    return;
  }

  waitForExplicitFinish();

  var CSS = {};
  Cu.import("resource://gre/modules/ConsoleAPIStorage.jsm", CSS);

  let innerID, beforeEvents, storageShouldOccur;

  var ConsoleObserver = {
    QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver]),

    observe: function CO_observe(aSubject, aTopic, aData)
    {
      if (aTopic != "console-api-log-event") {
        return;
      }

      let afterEvents = CSS.ConsoleAPIStorage.getEvents(innerID);

      is(beforeEvents.length == afterEvents.length - 1,
         storageShouldOccur,
         "storage should" + (storageShouldOccur ? "" : " not") + " occur");

      executeSoon(function() {
        Services.obs.removeObserver(ConsoleObserver, "console-api-log-event");
        pb.privateBrowsingEnabled = storageShouldOccur;
      });
    }
  };

  function checkStorageOccurs() {
    Services.obs.addObserver(ConsoleObserver, "console-api-log-event", false);

    let win = XPCNativeWrapper.unwrap(browser.contentWindow);
    innerID = getInnerWindowId(win);

    beforeEvents = CSS.ConsoleAPIStorage.getEvents(innerID);
    win.console.log("foo bar baz (private: " + !storageShouldOccur + ")");
  }

  function pbObserver(aSubject, aTopic, aData) {
    if (aData == "enter") {
      storageShouldOccur = false;
      checkStorageOccurs();
    } else if (aData == "exit") {
      executeSoon(finish);
    }
  }

  const TEST_URI = "http://example.com/browser/dom/tests/browser/test-console-api.html";
  var tab = gBrowser.selectedTab = gBrowser.addTab(TEST_URI);
  var browser = gBrowser.selectedBrowser;

  Services.obs.addObserver(pbObserver, "private-browsing", false);

  const PB_KEEP_SESSION_PREF = "browser.privatebrowsing.keep_current_session";
  Services.prefs.setBoolPref(PB_KEEP_SESSION_PREF, true);

  registerCleanupFunction(function () {
    gBrowser.removeTab(tab);

    Services.obs.removeObserver(pbObserver, "private-browsing");

    if (Services.prefs.prefHasUserValue(PB_KEEP_SESSION_PREF))
      Services.prefs.clearUserPref(PB_KEEP_SESSION_PREF);
  });

  browser.addEventListener("DOMContentLoaded", function onLoad(event) {
    if (browser.currentURI.spec != TEST_URI)
      return;

    browser.removeEventListener("DOMContentLoaded", onLoad, false);

    storageShouldOccur = true;
    checkStorageOccurs();
  }, false);
}

function getInnerWindowId(aWindow) {
  return aWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                .getInterface(Ci.nsIDOMWindowUtils)
                .currentInnerWindowID;
}
