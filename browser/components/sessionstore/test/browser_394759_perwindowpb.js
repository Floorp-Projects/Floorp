/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/** Private Browsing Test for Bug 394759 **/
function test() {
  waitForExplicitFinish();

  let windowsToClose = [];
  let closedWindowCount = 0;
  // Prevent VM timers issues, cache now and increment it manually.
  let now = Date.now();
  const TESTS = [
    { url: "about:config",
      key: "bug 394759 Non-PB",
      value: "uniq" + (++now) },
    { url: "about:mozilla",
      key: "bug 394759 PB",
      value: "uniq" + (++now) },
  ];

  registerCleanupFunction(function() {
    Services.prefs.clearUserPref("browser.sessionstore.interval");
    windowsToClose.forEach(function(win) {
      win.close();
    });
  });

  function testOpenCloseWindow(aIsPrivate, aTest, aCallback) {
    whenNewWindowLoaded({ private: aIsPrivate }, function(win) {
      whenBrowserLoaded(win.gBrowser.selectedBrowser, function() {
        executeSoon(function() {
          // Mark the window with some unique data to be restored later on.
          ss.setWindowValue(win, aTest.key, aTest.value);
          // Close.
          win.close();
          aCallback();
        });
      });
      win.gBrowser.selectedBrowser.loadURI(aTest.url);
    });
  }

  function testOnWindow(aIsPrivate, aValue, aCallback) {
    whenNewWindowLoaded({ private: aIsPrivate }, function(win) {
      windowsToClose.push(win);
      executeSoon(function() checkClosedWindows(aIsPrivate, aValue, aCallback));
    });
  }

  function checkClosedWindows(aIsPrivate, aValue, aCallback) {
    let data = JSON.parse(ss.getClosedWindowData())[0];
    is(ss.getClosedWindowCount(), 1, "Check the closed window count");
    ok(JSON.stringify(data).indexOf(aValue) > -1,
       "Check the closed window data was stored correctly");
    aCallback();
  }

  function setupBlankState(aCallback) {
    // Set interval to a large time so state won't be written while we setup
    // environment.
    Services.prefs.setIntPref("browser.sessionstore.interval", 100000);

    // Set up the browser in a blank state. Popup windows in previous tests
    // result in different states on different platforms.
    let blankState = JSON.stringify({
      windows: [{
        tabs: [{ entries: [{ url: "about:blank" }] }],
        _closedTabs: []
      }],
      _closedWindows: []
    });
    ss.setBrowserState(blankState);

    // Wait for the sessionstore.js file to be written before going on.
    // Note: we don't wait for the complete event, since if asyncCopy fails we
    // would timeout.
    Services.obs.addObserver(function (aSubject, aTopic, aData) {
      Services.obs.removeObserver(arguments.callee, aTopic);
      info("sessionstore.js is being written");

      closedWindowCount = ss.getClosedWindowCount();
      is(closedWindowCount, 0, "Correctly set window count");

      executeSoon(aCallback);
    }, "sessionstore-state-write", false);

    // Remove the sessionstore.js file before setting the interval to 0
    let profilePath = Services.dirsvc.get("ProfD", Ci.nsIFile);
    let sessionStoreJS = profilePath.clone();
    sessionStoreJS.append("sessionstore.js");
    if (sessionStoreJS.exists())
      sessionStoreJS.remove(false);
    info("sessionstore.js was correctly removed: " + (!sessionStoreJS.exists()));

    // Make sure that sessionstore.js can be forced to be created by setting
    // the interval pref to 0.
    Services.prefs.setIntPref("browser.sessionstore.interval", 0);
  }

  setupBlankState(function() {
    testOpenCloseWindow(false, TESTS[0], function() {
      testOpenCloseWindow(true, TESTS[1], function() {
        testOnWindow(false, TESTS[0].value, function() {
          testOnWindow(true, TESTS[0].value, finish);
        });
      });
    });
  });
}

