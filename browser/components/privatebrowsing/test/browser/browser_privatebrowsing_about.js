/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const TP_PB_ENABLED_PREF = "privacy.trackingprotection.pbmode.enabled";
const CB_ENABLED_PREF = "browser.contentblocking.enabled";

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
    expectedUrl: "https://example.com/tour?variation=1",
  });

  await BrowserTestUtils.closeWindow(win);
});

