/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  NetworkObserver:
    "resource://devtools/shared/network-observer/NetworkObserver.sys.mjs",
});

const TEST_DIR = gTestPath.substr(0, gTestPath.lastIndexOf("/"));
const CHROME_URL_ROOT = TEST_DIR + "/";
const URL_ROOT = CHROME_URL_ROOT.replace(
  "chrome://mochitests/content/",
  "https://example.com/"
);

/**
 * Load the provided url in an existing browser.
 * Returns a promise which will resolve when the page is loaded.
 *
 * @param {Browser} browser
 *     The browser element where the URL should be loaded.
 * @param {String} url
 *     The URL to load in the new tab
 */
async function loadURL(browser, url) {
  const loaded = BrowserTestUtils.browserLoaded(browser);
  BrowserTestUtils.startLoadingURIString(browser, url);
  return loaded;
}

/**
 * Create a new foreground tab loading the provided url.
 * Returns a promise which will resolve when the page is loaded.
 *
 * @param {String} url
 *     The URL to load in the new tab
 */
async function addTab(url) {
  const tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, url);
  registerCleanupFunction(() => {
    gBrowser.removeTab(tab);
  });
  return tab;
}

/**
 * Base network event owner class implementing all mandatory callbacks and
 * keeping track of which callbacks have been called.
 */
class NetworkEventOwner {
  hasEventTimings = false;
  hasResponseCache = false;
  hasResponseContent = false;
  hasResponseStart = false;
  hasSecurityInfo = false;
  hasServerTimings = false;

  addEventTimings() {
    this.hasEventTimings = true;
  }
  addResponseCache() {
    this.hasResponseCache = true;
  }
  addResponseContent() {
    this.hasResponseContent = true;
  }
  addResponseStart() {
    this.hasResponseStart = true;
  }
  addSecurityInfo() {
    this.hasSecurityInfo = true;
  }
  addServerTimings() {
    this.hasServerTimings = true;
  }
  addServiceWorkerTimings() {
    this.hasServiceWorkerTimings = true;
  }
}

/**
 * Create a simple network event owner, with mock implementations of all
 * the expected APIs for a NetworkEventOwner.
 */
function createNetworkEventOwner(event) {
  return new NetworkEventOwner();
}

/**
 * Wait for network events matching the provided URL, until the count reaches
 * the provided expected count.
 *
 * @param {string|null} expectedUrl
 *     The URL which should be monitored by the NetworkObserver.If set to null watch for
 *      all requests
 * @param {number} expectedRequestsCount
 *     How many different events (requests) are expected.
 * @returns {Promise}
 *     A promise which will resolve with an array of network event owners, when
 *     the expected event count is reached.
 */
async function waitForNetworkEvents(expectedUrl = null, expectedRequestsCount) {
  const events = [];
  const networkObserver = new NetworkObserver({
    ignoreChannelFunction: channel =>
      expectedUrl ? channel.URI.spec !== expectedUrl : false,
    onNetworkEvent: () => {
      info("waitForNetworkEvents received a new event");
      const owner = createNetworkEventOwner();
      events.push(owner);
      return owner;
    },
  });
  registerCleanupFunction(() => networkObserver.destroy());

  info("Wait until the events count reaches " + expectedRequestsCount);
  await BrowserTestUtils.waitForCondition(
    () => events.length >= expectedRequestsCount
  );
  return events;
}
