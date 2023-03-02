/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let TEST_PATH = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "https://example.com"
);

let TEST_PATH_AUTH = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "https://example.org"
);

const CROSS_DOMAIN_URL = TEST_PATH + "redirect-crossDomain.html";

const SAME_DOMAIN_URL = TEST_PATH + "redirect-sameDomain.html";

const CROSS_DOMAIN_SUB_URL = TEST_PATH + "cross-domain-iframe.html";

const SAME_DOMAIN_SUB_URL = TEST_PATH + "same-domain-iframe.html";

const AUTH_URL = TEST_PATH_AUTH + "auth-route.sjs";

const TOP_LEVEL_SAME_DOMAIN = 0;
const TOP_LEVEL_CROSS_DOMAIN = 1;
const SAME_DOMAIN_SUBRESOURCE = 2;
const CROSS_DOMAIN_SUBRESOURCE = 3;

/**
 * Opens a new tab with the given url, this will trigger an auth prompt for type
 * cross or same domain, top level or subresouce, depending on the url.
 * It checks whether the right index of the histogram "HTTP_AUTH_DIALOG_STATS_3"
 * was increases, according to the type
 * @param {String} urlToLoad - url to be loaded.
 * @param {Integer} index - index at which we check the count of the histogram.
 */
async function loadAndHandlePrompt(urlToLoad, index) {
  let histogram = TelemetryTestUtils.getAndClearHistogram(
    "HTTP_AUTH_RESOURCE_TYPE"
  );
  let dialogShown = waitForDialog(index, histogram);
  await BrowserTestUtils.withNewTab(urlToLoad, async function() {
    await dialogShown;
  });
  await new Promise(resolve => {
    Services.clearData.deleteData(
      Ci.nsIClearDataService.CLEAR_AUTH_CACHE,
      resolve
    );
  });
}

/**
 * Tests that top level cross domain 401s are properly recorded by telemetry
 */
add_task(async function testCrossDomainTopLevel() {
  await loadAndHandlePrompt(CROSS_DOMAIN_URL, TOP_LEVEL_CROSS_DOMAIN);
});

/**
 Tests that top level same domain 401s are properly recorded by telemetry
 */
add_task(async function testSameDomainTopLevel() {
  await loadAndHandlePrompt(SAME_DOMAIN_URL, TOP_LEVEL_SAME_DOMAIN);
});

/**
 Tests that cross domain 401s from sub resouces are properly recorded by telemetry
 */
add_task(async function testCrossDomainSubresource() {
  await loadAndHandlePrompt(CROSS_DOMAIN_SUB_URL, CROSS_DOMAIN_SUBRESOURCE);
});

/**
 Tests that same domain 401s from sub resouces are properly recorded by telemetry
 */
add_task(async function testSameDomainSubresource() {
  await loadAndHandlePrompt(SAME_DOMAIN_SUB_URL, SAME_DOMAIN_SUBRESOURCE);
});
