/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const TP_PB_ENABLED_PREF = "privacy.trackingprotection.pbmode.enabled";
const CB_ENABLED_PREF = "browser.contentblocking.enabled";
const CB_UI_ENABLED_PREF = "browser.contentblocking.ui.enabled";

/**
 * Opens a new private window and loads "about:privatebrowsing" there.
 */
async function openAboutPrivateBrowsing() {
  let win = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
    waitForTabURL: "about:privatebrowsing",
  });
  let tab = win.gBrowser.selectedBrowser;
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
  Services.prefs.setBoolPref(CB_UI_ENABLED_PREF, false);
  registerCleanupFunction(function() {
    Services.prefs.clearUserPref(CB_UI_ENABLED_PREF);
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
    expectedUrl: "https://example.com/tour?variation=0",
  });

  await BrowserTestUtils.closeWindow(win);
});

add_task(async function test_links_CB() {
  // Use full version and change the remote URLs to prevent network access.
  Services.prefs.setCharPref("app.support.baseURL", "https://example.com/");
  Services.prefs.setCharPref("privacy.trackingprotection.introURL",
                             "https://example.com/tour");
  Services.prefs.setBoolPref(CB_UI_ENABLED_PREF, true);
  registerCleanupFunction(function() {
    Services.prefs.clearUserPref(CB_UI_ENABLED_PREF);
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
    expectedUrl: "https://example.com/tour?variation=1",
  });

  await BrowserTestUtils.closeWindow(win);
});

function waitForPrefChanged(pref) {
  return new Promise(resolve => {
    let prefObserver = {
      QueryInterface: ChromeUtils.generateQI([Ci.nsIObserver]),
      observe() {
        Services.prefs.removeObserver(pref, prefObserver);
        resolve();
      },
    };
    Services.prefs.addObserver(pref, prefObserver);
  });
}

/**
 * Tests the action to disable and re-enable Tracking Protection in
 * "about:privatebrowsing".
 */
add_task(async function test_toggleTrackingProtection() {
  // Use tour version but disable Tracking Protection.
  Services.prefs.setBoolPref(TP_PB_ENABLED_PREF, true);
  // For good measure, check that content blocking being off
  // has no impact if the contentblocking UI is not shown.
  Services.prefs.setBoolPref(CB_ENABLED_PREF, false);
  Services.prefs.setBoolPref(CB_UI_ENABLED_PREF, false);

  registerCleanupFunction(function() {
    Services.prefs.clearUserPref(TP_PB_ENABLED_PREF);
    Services.prefs.clearUserPref(CB_ENABLED_PREF);
    Services.prefs.clearUserPref(CB_UI_ENABLED_PREF);
  });

  let { win, tab } = await openAboutPrivateBrowsing();

  // Set up the observer for the preference change before triggering the action.
  let promisePrefChanged = waitForPrefChanged(TP_PB_ENABLED_PREF);
  await ContentTask.spawn(tab, {}, async function() {
    is(content.document.getElementById("tpToggle").checked, true, "toggle is active");
    content.document.getElementById("tpButton").click();
  });
  await promisePrefChanged;
  ok(!Services.prefs.getBoolPref(TP_PB_ENABLED_PREF), "Tracking Protection is disabled.");

  promisePrefChanged = waitForPrefChanged(TP_PB_ENABLED_PREF);
  await ContentTask.spawn(tab, {}, async function() {
    is(content.document.getElementById("tpToggle").checked, false, "toggle is not active");
    content.document.getElementById("tpButton").click();
  });
  await promisePrefChanged;
  ok(Services.prefs.getBoolPref(TP_PB_ENABLED_PREF), "Tracking Protection is enabled.");

  await BrowserTestUtils.closeWindow(win);
});

/**
 * Tests the action to disable and re-enable Tracking Protection in
 * "about:privatebrowsing" when content blocking is disabled.
 */
add_task(async function test_toggleTrackingProtectionContentBlocking() {
  Services.prefs.setBoolPref(TP_PB_ENABLED_PREF, true);
  Services.prefs.setBoolPref(CB_ENABLED_PREF, false);
  Services.prefs.setBoolPref(CB_UI_ENABLED_PREF, true);

  registerCleanupFunction(function() {
    Services.prefs.clearUserPref(TP_PB_ENABLED_PREF);
    Services.prefs.clearUserPref(CB_ENABLED_PREF);
    Services.prefs.clearUserPref(CB_UI_ENABLED_PREF);
  });

  let { win, tab } = await openAboutPrivateBrowsing();

  let promiseCBPrefChanged = waitForPrefChanged(CB_ENABLED_PREF);
  await ContentTask.spawn(tab, {}, async function() {
    is(content.document.getElementById("tpToggle").checked, false, "toggle is not active");
    content.document.getElementById("tpButton").click();
  });
  await promiseCBPrefChanged;
  ok(Services.prefs.getBoolPref(TP_PB_ENABLED_PREF), "Tracking Protection is enabled.");
  ok(Services.prefs.getBoolPref(CB_ENABLED_PREF), "Content Blocking is enabled.");

  let promiseTPPrefChanged = waitForPrefChanged(TP_PB_ENABLED_PREF);
  await ContentTask.spawn(tab, {}, async function() {
    is(content.document.getElementById("tpToggle").checked, true, "toggle is active");
    content.document.getElementById("tpButton").click();
  });
  await promiseTPPrefChanged;
  ok(!Services.prefs.getBoolPref(TP_PB_ENABLED_PREF), "Tracking Protection is disabled.");
  ok(Services.prefs.getBoolPref(CB_ENABLED_PREF), "Content Blocking is enabled.");

  await BrowserTestUtils.closeWindow(win);
});
