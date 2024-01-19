"use strict";

const { TelemetryTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/TelemetryTestUtils.sys.mjs"
);

let listService;

const STRIP_ON_SHARE_PARAMS_REMOVED = "STRIP_ON_SHARE_PARAMS_REMOVED";
const STRIP_ON_SHARE_LENGTH_DECREASE = "STRIP_ON_SHARE_LENGTH_DECREASE";

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["privacy.query_stripping.strip_on_share.enabled", true],
      ["privacy.query_stripping.enabled", false],
    ],
  });

  // Get the list service so we can wait for it to be fully initialized before running tests.
  listService = Cc["@mozilla.org/query-stripping-list-service;1"].getService(
    Ci.nsIURLQueryStrippingListService
  );

  await listService.testWaitForInit();
});

// Checking telemetry for single query params being stripped
add_task(async function testSingleQueryParam() {
  let originalURI = "https://www.example.com/?utm_source=1";
  let strippedURI = "https://www.example.com/";

  // Calculating length difference between URLs to check correct telemetry label
  let lengthDiff = originalURI.length - strippedURI.length;

  let paramHistogram = TelemetryTestUtils.getAndClearHistogram(
    STRIP_ON_SHARE_PARAMS_REMOVED
  );
  let lengthHistogram = TelemetryTestUtils.getAndClearHistogram(
    STRIP_ON_SHARE_LENGTH_DECREASE
  );

  await testStripOnShare(originalURI, strippedURI);

  // The "1" Label is being checked as 1 Query Param is being stripped
  TelemetryTestUtils.assertHistogram(paramHistogram, 1, 1);
  TelemetryTestUtils.assertHistogram(lengthHistogram, lengthDiff, 1);

  await testStripOnShare(originalURI, strippedURI);

  TelemetryTestUtils.assertHistogram(paramHistogram, 1, 2);
  TelemetryTestUtils.assertHistogram(lengthHistogram, lengthDiff, 2);
});

// Checking telemetry for mutliple query params being stripped
add_task(async function testMultiQueryParams() {
  let originalURI = "https://www.example.com/?utm_source=1&utm_ad=1&utm_id=1";
  let strippedURI = "https://www.example.com/";

  // Calculating length difference between URLs to check correct telemetry label
  let lengthDiff = originalURI.length - strippedURI.length;

  let paramHistogram = TelemetryTestUtils.getAndClearHistogram(
    STRIP_ON_SHARE_PARAMS_REMOVED
  );
  let lengthHistogram = TelemetryTestUtils.getAndClearHistogram(
    STRIP_ON_SHARE_LENGTH_DECREASE
  );

  await testStripOnShare(originalURI, strippedURI);

  // The "3" Label is being checked as 3 Query Params are being stripped
  TelemetryTestUtils.assertHistogram(paramHistogram, 3, 1);
  TelemetryTestUtils.assertHistogram(lengthHistogram, lengthDiff, 1);

  await testStripOnShare(originalURI, strippedURI);

  TelemetryTestUtils.assertHistogram(paramHistogram, 3, 2);
  TelemetryTestUtils.assertHistogram(lengthHistogram, lengthDiff, 2);
});

async function testStripOnShare(validUrl, strippedUrl) {
  await BrowserTestUtils.withNewTab(validUrl, async function (browser) {
    gURLBar.focus();
    gURLBar.select();
    let menuitem = await promiseContextualMenuitem("strip-on-share");
    Assert.ok(BrowserTestUtils.isVisible(menuitem), "Menu item is visible");
    let hidePromise = BrowserTestUtils.waitForEvent(
      menuitem.parentElement,
      "popuphidden"
    );
    // Make sure the clean copy of the link will be copied to the clipboard
    await SimpleTest.promiseClipboardChange(strippedUrl, () => {
      menuitem.closest("menupopup").activateItem(menuitem);
    });
    await hidePromise;
  });
}
