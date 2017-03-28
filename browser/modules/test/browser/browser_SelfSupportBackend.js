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

function toggleSelfSupportTestMode(testing) {
  let selfSupportBackendImpl =
    Cu.import("resource:///modules/SelfSupportBackend.jsm", {}).SelfSupportBackendInternal;
  selfSupportBackendImpl._testing = testing;
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
    let browser = null;
    let browserPromise = TestUtils.topicObserved("self-support-browser-created",
        (subject, topic) => {
          let url = subject.getAttribute("src");
          Cu.reportError("Got browser with src: " + url);
          if (url == aURL) {
            browser = subject;
          }
          return url == aURL;
        });

    // Once found, append a "load" listener to catch page loads.
    browserPromise.then(() => {
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
  toggleSelfSupportTestMode(true);
  registerCleanupFunction(toggleSelfSupportTestMode.bind(null, false));
  // Initialise the SelfSupport backend and trigger the load.
  SelfSupportBackend.init();

  // Wait for the SelfSupport page to load.
  let selfSupportBrowserPromise = promiseSelfSupportLoad(TEST_PAGE_URL_HTTPS);

  // SelfSupportBackend waits for "sessionstore-windows-restored" to start loading. Send it.
  info("Sending sessionstore-windows-restored");
  sendSessionRestoredNotification();

  // Wait for the SelfSupport page to load.
  info("Waiting for the SelfSupport local page to load.");
  let selfSupportBrowser = yield selfSupportBrowserPromise;
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

  let selfSupportClosed = TestUtils.topicObserved("self-support-browser-destroyed");
  // Close SelfSupport from content.
  contentWindow.close();

  yield selfSupportClosed;
  Assert.ok(!selfSupportBrowser.parentNode, "SelfSupport browser must have been removed.");

  // We shouldn't need this, but let's keep it to make sure closing SelfSupport twice
  // doesn't create any problem.
  SelfSupportBackend.uninit();
});

