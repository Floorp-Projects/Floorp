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
 * Create a simple network event owner, with empty implementations of all
 * the expected APIs for a NetworkEventOwner.
 */
function createNetworkEventOwner(event) {
  return {
    addEventTimings: () => {},
    addResponseCache: () => {},
    addResponseContent: () => {},
    addResponseStart: () => {},
    addSecurityInfo: () => {},
    addServerTimings: () => {},
  };
}

/**
 * Wait for network events matching the provided URL, until the count reaches
 * the provided expected count.
 *
 * @param {string} expectedUrl
 *     The URL which should be monitored by the NetworkObserver.
 * @param {number} expectedRequestsCount
 *     How many different events (requests) are expected.
 * @returns {Promise}
 *     A promise which will resolve with the current count, when the expected
 *     count is reached.
 */
async function waitForNetworkEvents(expectedUrl, expectedRequestsCount) {
  let eventsCount = 0;
  const networkObserver = new NetworkObserver({
    ignoreChannelFunction: channel => channel.URI.spec !== expectedUrl,
    onNetworkEvent: event => {
      info("waitForNetworkEvents received a new event");
      eventsCount++;
      return createNetworkEventOwner(event);
    },
  });
  registerCleanupFunction(() => networkObserver.destroy());

  info("Wait until the events count reaches " + expectedRequestsCount);
  await BrowserTestUtils.waitForCondition(
    () => eventsCount >= expectedRequestsCount
  );
  return eventsCount;
}
