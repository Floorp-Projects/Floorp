/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function test() {
  /** Private Browsing Test for Bug 394759 **/

  waitForExplicitFinish();

  // Set interval to a large time so state won't be written while we setup env.
  gPrefService.setIntPref("browser.sessionstore.interval", 100000);

  // Set up the browser in a blank state. Popup windows in previous tests result
  // in different states on different platforms.
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
    executeSoon(continue_test);
  }, "sessionstore-state-write", false);

  // Remove the sessionstore.js file before setting the interval to 0
  let profilePath = Cc["@mozilla.org/file/directory_service;1"].
                    getService(Ci.nsIProperties).
                    get("ProfD", Ci.nsIFile);
  let sessionStoreJS = profilePath.clone();
  sessionStoreJS.append("sessionstore.js");
  if (sessionStoreJS.exists())
    sessionStoreJS.remove(false);
  info("sessionstore.js was correctly removed: " + (!sessionStoreJS.exists()));

  // Make sure that sessionstore.js can be forced to be created by setting
  // the interval pref to 0.
  gPrefService.setIntPref("browser.sessionstore.interval", 0);
}

function continue_test() {
  let pb = Cc["@mozilla.org/privatebrowsing;1"].
           getService(Ci.nsIPrivateBrowsingService);
  // Ensure Private Browsing mode is disabled.
  ok(!pb.privateBrowsingEnabled, "Private Browsing is disabled");

  let closedWindowCount = ss.getClosedWindowCount();
  is(closedWindowCount, 0, "Correctly set window count");

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

  function openWindowAndTest(aTestIndex, aRunNextTestInPBMode) {
    info("Opening new window");
    function onLoad(event) {
            win.removeEventListener("load", onLoad, false);
            info("New window has been loaded");
            win.gBrowser.addEventListener("load", function(aEvent) {
              win.gBrowser.removeEventListener("load", arguments.callee, true);
              info("New window browser has been loaded");
              executeSoon(function() {
                // Add a tab.
                win.gBrowser.addTab();

                executeSoon(function() {
                  // Mark the window with some unique data to be restored later on.
                  ss.setWindowValue(win, TESTS[aTestIndex].key, TESTS[aTestIndex].value);

                  win.close();

                  // Ensure that we incremented # of close windows.
                  is(ss.getClosedWindowCount(), closedWindowCount + 1,
                     "The closed window was added to the list");

                  // Ensure we added window to undo list.
                  let data = JSON.parse(ss.getClosedWindowData())[0];
                  ok(JSON.stringify(data).indexOf(TESTS[aTestIndex].value) > -1,
                     "The closed window data was stored correctly");

                  if (aRunNextTestInPBMode) {
                    // Enter private browsing mode.
                    pb.privateBrowsingEnabled = true;
                    ok(pb.privateBrowsingEnabled, "private browsing enabled");

                    // Ensure that we have 0 undo windows when entering PB.
                    is(ss.getClosedWindowCount(), 0,
                       "Recently Closed Windows are removed when entering Private Browsing");
                    is(ss.getClosedWindowData(), "[]",
                       "Recently Closed Windows data is cleared when entering Private Browsing");
                  }
                  else {
                    // Exit private browsing mode.
                    pb.privateBrowsingEnabled = false;
                    ok(!pb.privateBrowsingEnabled, "private browsing disabled");

                    // Ensure that we still have the closed windows from before.
                    is(ss.getClosedWindowCount(), closedWindowCount + 1,
                       "The correct number of recently closed windows were restored " +
                       "when exiting PB mode");

                    let data = JSON.parse(ss.getClosedWindowData())[0];
                    ok(JSON.stringify(data).indexOf(TESTS[aTestIndex - 1].value) > -1,
                       "The data associated with the recently closed window was " +
                       "restored when exiting PB mode");
                  }

                  if (aTestIndex == TESTS.length - 1) {
                    gPrefService.clearUserPref("browser.sessionstore.interval");
                    finish();
                  }
                  else {
                    // Run next test.
                    openWindowAndTest(aTestIndex + 1, !aRunNextTestInPBMode);
                  }
                });
              });
            }, true);
    }
    // Open a window.
    var win = openDialog(location, "", "chrome,all,dialog=no", TESTS[aTestIndex].url);
    win.addEventListener("load", onLoad, false);
  }

  openWindowAndTest(0, true);
}
