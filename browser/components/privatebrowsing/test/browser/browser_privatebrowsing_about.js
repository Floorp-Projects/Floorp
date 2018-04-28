/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Opens a new private window and loads "about:privatebrowsing" there.
 */
async function openAboutPrivateBrowsing() {
  let win = await BrowserTestUtils.openNewBrowserWindow({ private: true });
  let tab = win.gBrowser.selectedBrowser;
  tab.loadURI("about:privatebrowsing");
  await BrowserTestUtils.browserLoaded(tab);
  return { win, tab };
}

/**
 * Clicks the given link and checks this opens a new tab with the given URI.
 */
async function testLinkOpensTab({ win, tab, elementId, expectedUrl }) {
  let newTabPromise = BrowserTestUtils.waitForNewTab(win.gBrowser, expectedUrl);
  await ContentTask.spawn(tab, elementId, async function(elemId) {
    content.document.getElementById(elemId).click();
  });
  let newTab = await newTabPromise;
  ok(true, `Clicking ${elementId} opened ${expectedUrl} in a new tab.`);
  BrowserTestUtils.removeTab(newTab);
}

/**
 * Clicks the given link and checks this opens the given URI in the same tab.
 *
 * This function does not return to the previous page.
 */
async function testLinkOpensUrl({ win, tab, elementId, expectedUrl }) {
  let loadedPromise = BrowserTestUtils.browserLoaded(tab);
  await ContentTask.spawn(tab, elementId, async function(elemId) {
    content.document.getElementById(elemId).click();
  });
  await loadedPromise;
  is(tab.currentURI.spec, expectedUrl,
     `Clicking ${elementId} opened ${expectedUrl} in the same tab.`);
}

/**
 * Tests the links in "about:privatebrowsing".
 */
add_task(async function test_links() {
  // Use full version and change the remote URLs to prevent network access.
  Services.prefs.setCharPref("app.support.baseURL", "https://example.com/");
  Services.prefs.setCharPref("privacy.trackingprotection.introURL",
                             "https://example.com/tour");
  registerCleanupFunction(function() {
    Services.prefs.clearUserPref("privacy.trackingprotection.introURL");
    Services.prefs.clearUserPref("app.support.baseURL");
  });

  let { win, tab } = await openAboutPrivateBrowsing();

  await testLinkOpensTab({ win, tab,
    elementId: "learnMore",
    expectedUrl: "https://example.com/private-browsing",
  });

  await testLinkOpensUrl({ win, tab,
    elementId: "startTour",
    expectedUrl: "https://example.com/tour",
  });

  await BrowserTestUtils.closeWindow(win);
});

/**
 * Tests the action to disable and re-enable Tracking Protection in
 * "about:privatebrowsing".
 */
add_task(async function test_toggleTrackingProtection() {
  // Use tour version but disable Tracking Protection.
  Services.prefs.setBoolPref("privacy.trackingprotection.pbmode.enabled",
                             true);
  registerCleanupFunction(function() {
    Services.prefs.clearUserPref("privacy.trackingprotection.pbmode.enabled");
  });

  let { win, tab } = await openAboutPrivateBrowsing();

  // Set up the observer for the preference change before triggering the action.
  let prefBranch =
      Services.prefs.getBranch("privacy.trackingprotection.pbmode.");
  let waitForPrefChanged = () => new Promise(resolve => {
    let prefObserver = {
      QueryInterface: ChromeUtils.generateQI([Ci.nsIObserver]),
      observe() {
        prefBranch.removeObserver("enabled", prefObserver);
        resolve();
      },
    };
    prefBranch.addObserver("enabled", prefObserver);
  });

  let promisePrefChanged = waitForPrefChanged();
  await ContentTask.spawn(tab, {}, async function() {
    content.document.getElementById("tpButton").click();
  });
  await promisePrefChanged;
  ok(!prefBranch.getBoolPref("enabled"), "Tracking Protection is disabled.");

  promisePrefChanged = waitForPrefChanged();
  await ContentTask.spawn(tab, {}, async function() {
    content.document.getElementById("tpButton").click();
  });
  await promisePrefChanged;
  ok(prefBranch.getBoolPref("enabled"), "Tracking Protection is enabled.");

  await BrowserTestUtils.closeWindow(win);
});
