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
  "chrome://marionette/content/sync.js"
);
add_task(async function test_telemetryMoreSiteOrigin() {
  await SpecialPowers.pushPrefEnv({
    set: [["telemetry.number_of_site_origin.min_interval", 0]],
  });
  const histogram = TelemetryTestUtils.getAndClearHistogram(
    "FX_NUMBER_OF_UNIQUE_SITE_ORIGINS_ALL_TABS"
  );
  // Need to open the tab twice because the first one is going
  // to be discarded
  let newTab1 = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    opening: gTestRoot + "contain_iframe.html",
    waitForStateStop: true,
  });

  let newTab2 = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    opening: gTestRoot + "contain_iframe.html",
    waitForStateStop: true,
  });

  await new Promise(resolve =>
    setTimeout(function() {
      window.requestIdleCallback(resolve);
    }, 0)
  );

  TelemetryTestUtils.assertHistogram(histogram, 2, 1);
  await BrowserTestUtils.removeTab(newTab1);
  await BrowserTestUtils.removeTab(newTab2);
});
