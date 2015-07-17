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
  yield ContentTask.spawn(tab, { elementId }, function* ({ elementId }) {
    content.document.getElementById(elementId).click();
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
  yield ContentTask.spawn(tab, { elementId }, function* ({ elementId }) {
    content.document.getElementById(elementId).click();
  });
  yield loadedPromise;
  is(tab.currentURI.spec, expectedUrl,
     `Clicking ${elementId} opened ${expectedUrl} in the same tab.`);
}

/**
 * Tests the Learn More action in the classic "about:privatebrowsing" window.
 */
add_task(function* test_classicActions() {
  // Use classic version and change the remote URL to prevent network access.
  Services.prefs.setBoolPref("privacy.trackingprotection.ui.enabled", false);
  Services.prefs.setCharPref("app.support.baseURL", "https://example.com/");
  registerCleanupFunction(function () {
    Services.prefs.clearUserPref("app.support.baseURL");
    Services.prefs.clearUserPref("privacy.trackingprotection.ui.enabled");
  });

  let { win, tab } = yield openAboutPrivateBrowsing();

  yield testLinkOpensTab({ win, tab,
    elementId: "learnMore",
    expectedUrl: "https://example.com/private-browsing",
  });

  yield BrowserTestUtils.closeWindow(win);
});

/**
 * Tests the Tracking Protection tour actions in "about:privatebrowsing".
 */
add_task(function* test_tourActions() {
  // Use tour version and change the remote URLs to prevent network access.
  Services.prefs.setBoolPref("privacy.trackingprotection.ui.enabled", true);
  Services.prefs.setCharPref("app.support.baseURL", "https://example.com/");
  Services.prefs.setCharPref("privacy.trackingprotection.introURL",
                             "https://example.com/tour");
  registerCleanupFunction(function () {
    Services.prefs.clearUserPref("privacy.trackingprotection.introURL");
    Services.prefs.clearUserPref("app.support.baseURL");
    Services.prefs.clearUserPref("privacy.trackingprotection.ui.enabled");
  });

  let { win, tab } = yield openAboutPrivateBrowsing();

  yield testLinkOpensTab({ win, tab,
    elementId: "showPreferences",
    expectedUrl: "about:preferences#privacy",
  });

  yield testLinkOpensTab({ win, tab,
    elementId: "tourLearnMore",
    expectedUrl: "https://example.com/private-browsing",
  });

  yield testLinkOpensUrl({ win, tab,
    elementId: "startTour",
    expectedUrl: "https://example.com/tour",
  });

  yield BrowserTestUtils.closeWindow(win);
});
