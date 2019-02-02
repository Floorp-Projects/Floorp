/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 **/

var gTests = [
  {
    name: "normal search (search service)",
    testText: "test search",
    searchURL: Services.search.defaultEngine.getSubmission("test search", null, "keyword").uri.spec,
  },
  {
    name: "?-prefixed search (search service)",
    testText: "?   foo  ",
    searchURL: Services.search.defaultEngine.getSubmission("foo", null, "keyword").uri.spec,
  },
];

function test() {
  waitForExplicitFinish();

  let windowObserver = {
    observe(aSubject, aTopic, aData) {
      if (aTopic == "domwindowopened") {
        ok(false, "Alert window opened");
        let win = aSubject;
        win.addEventListener("load", function() {
          win.close();
        }, {once: true});
        executeSoon(finish);
      }
    },
  };

  Services.ww.registerNotification(windowObserver);

  let tab = gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);

  // We use a web progress listener in the content process to cancel
  // the request. For everything else, we can do the work in the
  // parent, since it's easier.
  ContentTask.spawn(gBrowser.selectedBrowser, null, function* gen() {
    let listener = {
      onStateChange: function onLocationChange(webProgress, req, flags, status) {
        let docStart = Ci.nsIWebProgressListener.STATE_IS_DOCUMENT |
                       Ci.nsIWebProgressListener.STATE_START;
        if (!(flags & docStart)) {
          return;
        }

        req.cancel(Cr.NS_ERROR_FAILURE);
      },

      QueryInterface: ChromeUtils.generateQI(["nsIWebProgressListener", "nsIWebProgressListener2", "nsISupportsWeakReference"]),
    };

    let webProgress = docShell.QueryInterface(Ci.nsIInterfaceRequestor)
        .getInterface(Ci.nsIWebProgress);
    webProgress.addProgressListener(listener,
                                    Ci.nsIWebProgress.NOTIFY_STATE_DOCUMENT);

    addEventListener("unload", () => {
      webProgress.removeProgressListener(listener);
    });
  });

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
    },
  };

  gBrowser.addProgressListener(listener);

  registerCleanupFunction(function() {
    Services.ww.unregisterNotification(windowObserver);

    gBrowser.removeProgressListener(listener);
    gBrowser.removeTab(tab);
  });

  Services.search.init().then(nextTest);
}

var gCurrTest;
function nextTest() {
  if (gTests.length) {
    gCurrTest = gTests.shift();
    doTest();
  } else {
    finish();
  }
}

function doTest() {
  info("Running test: " + gCurrTest.name);

  // Simulate a user entering search terms
  gURLBar.value = gCurrTest.testText;
  gURLBar.focus();
  EventUtils.synthesizeKey("KEY_Enter");
}
