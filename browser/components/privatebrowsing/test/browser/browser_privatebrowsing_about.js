/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Opens a new private window and loads "about:privatebrowsing" there.
 */
function* openAboutPrivateBrowsing() {
  let win = yield BrowserTestUtils.openNewBrowserWindow({ private: true });
  let tab = win.gBrowser.selectedBrowser;
  tab.loadURI("about:privatebrowsing");
  yield BrowserTestUtils.browserLoaded(tab);
  return { win, tab };
}

/**
 * Clicks the given link and checks this opens a new tab with the given URI.
 */
function* testLinkOpensTab({ win, tab, elementId, expectedUrl }) {
  let newTabPromise = BrowserTestUtils.waitForNewTab(win.gBrowser, expectedUrl);
  yield ContentTask.spawn(tab, elementId, function* (elemId) {
    content.document.getElementById(elemId).click();
  });
  let newTab = yield newTabPromise;
  ok(true, `Clicking ${elementId} opened ${expectedUrl} in a new tab.`);
  yield BrowserTestUtils.removeTab(newTab);
}

/**
 * Clicks the given link and checks this opens the given URI in the same tab.
 *
 * This function does not return to the previous page.
 */
function* testLinkOpensUrl({ win, tab, elementId, expectedUrl }) {
  let loadedPromise = BrowserTestUtils.browserLoaded(tab);
  yield ContentTask.spawn(tab, elementId, function* (elemId) {
    content.document.getElementById(elemId).click();
  });
  yield loadedPromise;
  is(tab.currentURI.spec, expectedUrl,
     `Clicking ${elementId} opened ${expectedUrl} in the same tab.`);
}

/**
 * Tests the links in "about:privatebrowsing".
 */
add_task(function* test_links() {
  // Use full version and change the remote URLs to prevent network access.
  Services.prefs.setCharPref("app.support.baseURL", "https://example.com/");
  Services.prefs.setCharPref("privacy.trackingprotection.introURL",
                             "https://example.com/tour");
  registerCleanupFunction(function() {
    Services.prefs.clearUserPref("privacy.trackingprotection.introURL");
    Services.prefs.clearUserPref("app.support.baseURL");
  });

  let { win, tab } = yield openAboutPrivateBrowsing();

  yield testLinkOpensTab({ win, tab,
    elementId: "learnMore",
    expectedUrl: "https://example.com/private-browsing",
  });

  yield testLinkOpensUrl({ win, tab,
    elementId: "startTour",
    expectedUrl: "https://example.com/tour",
  });

  yield BrowserTestUtils.closeWindow(win);
});

/**
 * Tests the action to disable and re-enable Tracking Protection in
 * "about:privatebrowsing".
 */
add_task(function* test_toggleTrackingProtection() {
  // Use tour version but disable Tracking Protection.
  Services.prefs.setBoolPref("privacy.trackingprotection.pbmode.enabled",
                             true);
  registerCleanupFunction(function() {
    Services.prefs.clearUserPref("privacy.trackingprotection.pbmode.enabled");
  });

  let { win, tab } = yield openAboutPrivateBrowsing();

  // Set up the observer for the preference change before triggering the action.
  let prefBranch =
      Services.prefs.getBranch("privacy.trackingprotection.pbmode.");
  let waitForPrefChanged = () => new Promise(resolve => {
    let prefObserver = {
      QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver]),
      observe() {
        prefBranch.removeObserver("enabled", prefObserver);
        resolve();
      },
    };
    prefBranch.addObserver("enabled", prefObserver, false);
  });

  let promisePrefChanged = waitForPrefChanged();
  yield ContentTask.spawn(tab, {}, function* () {
    content.document.getElementById("tpButton").click();
  });
  yield promisePrefChanged;
  ok(!prefBranch.getBoolPref("enabled"), "Tracking Protection is disabled.");

  promisePrefChanged = waitForPrefChanged();
  yield ContentTask.spawn(tab, {}, function* () {
    content.document.getElementById("tpButton").click();
  });
  yield promisePrefChanged;
  ok(prefBranch.getBoolPref("enabled"), "Tracking Protection is enabled.");

  yield BrowserTestUtils.closeWindow(win);
});
