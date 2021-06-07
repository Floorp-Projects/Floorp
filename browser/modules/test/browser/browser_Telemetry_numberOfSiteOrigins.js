/* eslint-disable mozilla/no-arbitrary-setTimeout */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/**
 * This file tests page reload key combination telemetry
 */

"use strict";

ChromeUtils.defineModuleGetter(
  this,
  "TelemetryTestUtils",
  "resource://testing-common/TelemetryTestUtils.jsm"
);

const gTestRoot = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "http://mochi.test:8888"
);

const { TimedPromise } = ChromeUtils.import(
  "chrome://remote/content/marionette/sync.js"
);

async function run_test(count) {
  const histogram = TelemetryTestUtils.getAndClearHistogram(
    "FX_NUMBER_OF_UNIQUE_SITE_ORIGINS_ALL_TABS"
  );

  let newTab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    opening: gTestRoot + "contain_iframe.html",
    waitForStateStop: true,
  });

  await new Promise(resolve =>
    setTimeout(function() {
      window.requestIdleCallback(resolve);
    }, 1000)
  );

  if (count < 2) {
    await BrowserTestUtils.removeTab(newTab);
    await run_test(count + 1);
  } else {
    TelemetryTestUtils.assertHistogram(histogram, 2, 1);
    await BrowserTestUtils.removeTab(newTab);
  }
}

add_task(async function test_telemetryMoreSiteOrigin() {
  await run_test(1);
});
