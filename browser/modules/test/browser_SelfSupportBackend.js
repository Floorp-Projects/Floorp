/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Pass an empty scope object to the import to prevent "leaked window property"
// errors in tests.
let Preferences = Cu.import("resource://gre/modules/Preferences.jsm", {}).Preferences;
let PromiseUtils = Cu.import("resource://gre/modules/PromiseUtils.jsm", {}).PromiseUtils;
let SelfSupportBackend =
  Cu.import("resource:///modules/SelfSupportBackend.jsm", {}).SelfSupportBackend;

const PREF_SELFSUPPORT_ENABLED = "browser.selfsupport.enabled";
const PREF_SELFSUPPORT_URL = "browser.selfsupport.url";
const PREF_UITOUR_ENABLED = "browser.uitour.enabled";

const TEST_WAIT_RETRIES = 60;

const TEST_PAGE_URL = getRootDirectory(gTestPath) + "uitour.html";

/**
 * Find a browser, with an IFRAME as parent, who has aURL as the source attribute.
 *
 * @param aURL The URL to look for to identify the browser.
 *
 * @returns {Object} The browser element or null on failure.
 */
function findSelfSupportBrowser(aURL) {
  let frames = Services.appShell.hiddenDOMWindow.document.querySelectorAll('iframe');
  for (let frame of frames) {
    try {
      let browser = frame.contentDocument.getElementById("win").querySelectorAll('browser')[0];
      let url = browser.getAttribute("src");
      if (url == aURL) {
        return browser;
      }
    } catch (e) {
      continue;
    }
  }
  return null;
}

/**
 * Wait for self support page to load.
 *
 * @param aURL The URL to look for to identify the browser.
 *
 * @returns {Promise} Return a promise which is resolved when SelfSupport page is fully
 *          loaded.
 */
function promiseSelfSupportLoad(aURL) {
  return new Promise((resolve, reject) => {
    // Find the SelfSupport browser.
    let browserPromise = waitForConditionPromise(() => !!findSelfSupportBrowser(aURL),
                                                 "SelfSupport browser not found.",
                                                 TEST_WAIT_RETRIES);

    // Once found, append a "load" listener to catch page loads.
    browserPromise.then(() => {
      let browser = findSelfSupportBrowser(aURL);
      if (browser.contentDocument.readyState === "complete") {
        resolve(browser);
      } else {
        let handler = () => {
          browser.removeEventListener("load", handler, true);
          resolve(browser);
        };
        browser.addEventListener("load", handler, true);
      }
    }, reject);
  });
}

/**
 * Wait for self support to close.
 *
 * @param aURL The URL to look for to identify the browser.
 *
 * @returns {Promise} Return a promise which is resolved when SelfSupport browser cannot
 *          be found anymore.
 */
function promiseSelfSupportClose(aURL) {
  return waitForConditionPromise(() => !findSelfSupportBrowser(aURL),
                                 "SelfSupport browser is still open.", TEST_WAIT_RETRIES);
}

/**
 * Prepare the test environment.
 */
add_task(function* setupEnvironment() {
  // We always run the SelfSupportBackend in tests to check for weird behaviours.
  // Disable it to test its start-up.
  SelfSupportBackend.uninit();

  // Testing prefs are set via |user_pref|, so we need to get their value in order
  // to restore them.
  let selfSupportEnabled = Preferences.get(PREF_SELFSUPPORT_ENABLED, true);
  let uitourEnabled = Preferences.get(PREF_UITOUR_ENABLED, false);
  let selfSupportURL = Preferences.get(PREF_SELFSUPPORT_URL, "");

  // Enable the SelfSupport backend and set the page URL. We also make sure UITour
  // is enabled.
  Preferences.set(PREF_SELFSUPPORT_ENABLED, true);
  Preferences.set(PREF_UITOUR_ENABLED, true);
  Preferences.set(PREF_SELFSUPPORT_URL, TEST_PAGE_URL);

  registerCleanupFunction(() => {
    Preferences.set(PREF_SELFSUPPORT_ENABLED, selfSupportEnabled);
    Preferences.set(PREF_UITOUR_ENABLED, uitourEnabled);
    Preferences.set(PREF_SELFSUPPORT_URL, selfSupportURL);
  });
});

/**
 * Test that the self support page can use the UITour API and close itself.
 */
add_task(function* test_selfSupport() {
  // Initialise the SelfSupport backend and trigger the load.
  SelfSupportBackend.init();

  // SelfSupportBackend waits for "sessionstore-windows-restored" to start loading. Send it.
  info("Sending sessionstore-windows-restored");
  Services.obs.notifyObservers(null, "sessionstore-windows-restored", null);

  // Wait for the SelfSupport page to load.
  info("Waiting for the SelfSupport local page to load.");
  let selfSupportBrowser = yield promiseSelfSupportLoad(TEST_PAGE_URL);
  Assert.ok(!!selfSupportBrowser, "SelfSupport browser must exist.");

  // Get a reference to the UITour API.
  info("Testing access to the UITour API.");
  let contentWindow =
    Cu.waiveXrays(selfSupportBrowser.contentDocument.defaultView);
  let uitourAPI = contentWindow.Mozilla.UITour;

  // Test the UITour API with a ping.
  let pingPromise = new Promise((resolve) => {
    uitourAPI.ping(resolve);
  });
  yield pingPromise;

  // Close SelfSupport from content.
  contentWindow.close();

  // Wait until SelfSupport closes.
  info("Waiting for the SelfSupport to close.");
  yield promiseSelfSupportClose(TEST_PAGE_URL);

  // Find the SelfSupport browser, again. We don't expect to find it.
  selfSupportBrowser = findSelfSupportBrowser(TEST_PAGE_URL);
  Assert.ok(!selfSupportBrowser, "SelfSupport browser must not exist.");

  // We shouldn't need this, but let's keep it to make sure closing SelfSupport twice
  // doesn't create any problem.
  SelfSupportBackend.uninit();
});
