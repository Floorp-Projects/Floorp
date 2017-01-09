/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Pass an empty scope object to the import to prevent "leaked window property"
// errors in tests.
var Preferences = Cu.import("resource://gre/modules/Preferences.jsm", {}).Preferences;
var PromiseUtils = Cu.import("resource://gre/modules/PromiseUtils.jsm", {}).PromiseUtils;
var SelfSupportBackend =
  Cu.import("resource:///modules/SelfSupportBackend.jsm", {}).SelfSupportBackend;

const PREF_SELFSUPPORT_ENABLED = "browser.selfsupport.enabled";
const PREF_SELFSUPPORT_URL = "browser.selfsupport.url";
const PREF_UITOUR_ENABLED = "browser.uitour.enabled";

const TEST_WAIT_RETRIES = 60;

const TEST_PAGE_URL = getRootDirectory(gTestPath) + "uitour.html";
const TEST_PAGE_URL_HTTPS = TEST_PAGE_URL.replace("chrome://mochitests/content/", "https://example.com/");

function sendSessionRestoredNotification() {
  let selfSupportBackendImpl =
    Cu.import("resource:///modules/SelfSupportBackend.jsm", {}).SelfSupportBackendInternal;
  selfSupportBackendImpl.observe(null, "sessionstore-windows-restored", null);
}

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
  Preferences.set(PREF_SELFSUPPORT_URL, TEST_PAGE_URL_HTTPS);

  // Whitelist the HTTPS page to use UITour.
  let pageURI = Services.io.newURI(TEST_PAGE_URL_HTTPS);
  Services.perms.add(pageURI, "uitour", Services.perms.ALLOW_ACTION);

  registerCleanupFunction(() => {
    Services.perms.remove(pageURI, "uitour");
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
  sendSessionRestoredNotification();

  // Wait for the SelfSupport page to load.
  info("Waiting for the SelfSupport local page to load.");
  let selfSupportBrowser = yield promiseSelfSupportLoad(TEST_PAGE_URL_HTTPS);
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
  info("Ping succeeded");

  let observePromise = ContentTask.spawn(selfSupportBrowser, null, function* checkObserve() {
    yield new Promise(resolve => {
      let win = Cu.waiveXrays(content);
      win.Mozilla.UITour.observe((event, data) => {
        if (event != "Heartbeat:Engaged") {
          return;
        }
        Assert.equal(data.flowId, "myFlowID", "Check flowId");
        Assert.ok(!!data.timestamp, "Check timestamp");
        resolve(data);
      }, () => {});
    });
  });

  info("Notifying Heartbeat:Engaged");
  UITour.notify("Heartbeat:Engaged", {
    flowId: "myFlowID",
    timestamp: Date.now(),
  });
  yield observePromise;
  info("Observed in the hidden frame");

  // Close SelfSupport from content.
  contentWindow.close();

  // Wait until SelfSupport closes.
  info("Waiting for the SelfSupport to close.");
  yield promiseSelfSupportClose(TEST_PAGE_URL_HTTPS);

  // Find the SelfSupport browser, again. We don't expect to find it.
  selfSupportBrowser = findSelfSupportBrowser(TEST_PAGE_URL_HTTPS);
  Assert.ok(!selfSupportBrowser, "SelfSupport browser must not exist.");

  // We shouldn't need this, but let's keep it to make sure closing SelfSupport twice
  // doesn't create any problem.
  SelfSupportBackend.uninit();
});

/**
 * Test that SelfSupportBackend only allows HTTPS.
 */
add_task(function* test_selfSupport_noHTTPS() {
  Preferences.set(PREF_SELFSUPPORT_URL, TEST_PAGE_URL);

  SelfSupportBackend.init();

  // SelfSupportBackend waits for "sessionstore-windows-restored" to start loading. Send it.
  info("Sending sessionstore-windows-restored");
  sendSessionRestoredNotification();

  // Find the SelfSupport browser. We don't expect to find it since we are not using https.
  let selfSupportBrowser = findSelfSupportBrowser(TEST_PAGE_URL);
  Assert.ok(!selfSupportBrowser, "SelfSupport browser must not exist.");

  // We shouldn't need this, but let's keep it to make sure closing SelfSupport twice
  // doesn't create any problem.
  SelfSupportBackend.uninit();
})
