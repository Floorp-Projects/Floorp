/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 **/

var gTests = [
  {
    name: "normal search (search service)",
    testText: "test search",
    searchURL: Services.search.defaultEngine.getSubmission("test search", null, "keyword").uri.spec
  },
  {
    name: "?-prefixed search (search service)",
    testText: "?   foo  ",
    searchURL: Services.search.defaultEngine.getSubmission("foo", null, "keyword").uri.spec
  }
];

function test() {
  waitForExplicitFinish();

  let windowObserver = {
    observe: function(aSubject, aTopic, aData) {
      if (aTopic == "domwindowopened") {
        ok(false, "Alert window opened");
        let win = aSubject.QueryInterface(Ci.nsIDOMEventTarget);
        win.addEventListener("load", function() {
          win.removeEventListener("load", arguments.callee, false);
          win.close();
        }, false);
        executeSoon(finish);
      }
    }
  };

  Services.ww.registerNotification(windowObserver);

  let tab = gBrowser.selectedTab = gBrowser.addTab();

  let listener = {
    onStateChange: function onLocationChange(webProgress, req, flags, status) {
      // Only care about document starts
      let docStart = Ci.nsIWebProgressListener.STATE_IS_DOCUMENT |
                     Ci.nsIWebProgressListener.STATE_START;
      if (!(flags & docStart))
        return;

      info("received document start");

      ok(req instanceof Ci.nsIChannel, "req is a channel");
      is(req.originalURI.spec, gCurrTest.searchURL, "search URL was loaded");
      info("Actual URI: " + req.URI.spec);

      executeSoon(nextTest);
    }
  }
  gBrowser.addProgressListener(listener);

  registerCleanupFunction(function () {
    Services.ww.unregisterNotification(windowObserver);

    gBrowser.removeProgressListener(listener);
    gBrowser.removeTab(tab);
  });

  nextTest();
}

var gCurrTest;
function nextTest() {
  // Clear the pref before every test (and after the last)
  try {
    Services.prefs.clearUserPref("keyword.URL");
  } catch(ex) {}

  if (gTests.length) {
    gCurrTest = gTests.shift();
    doTest();
  } else {
    finish();
  }
}

function doTest() {
  info("Running test: " + gCurrTest.name);

  if (gCurrTest.keywordURLPref)
    Services.prefs.setCharPref("keyword.URL", gCurrTest.keywordURLPref);

  // Simulate a user entering search terms
  gURLBar.value = gCurrTest.testText;
  gURLBar.focus();
  EventUtils.synthesizeKey("VK_RETURN", {});
}
