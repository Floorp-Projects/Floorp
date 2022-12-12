"use strict";

const { TelemetryTestUtils } = ChromeUtils.import(
  "resource://testing-common/TelemetryTestUtils.jsm"
);

const ALL_CHANNELS = Ci.nsITelemetry.DATASET_ALL_CHANNELS;

add_task(async function() {
  let tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    waitForLoad: true,
  });

  let browser = tab.linkedBrowser;

  // Reset event counts.
  Services.telemetry.clearEvents();
  TelemetryTestUtils.assertNumberOfEvents(0);

  // Perform page load
  BrowserTestUtils.loadURI(browser, "https://example.com");
  await BrowserTestUtils.browserLoaded(browser);

  // Check that a page load event exists
  let event_found = await TestUtils.waitForCondition(() => {
    let events = Services.telemetry.snapshotEvents(ALL_CHANNELS, true).content;
    if (events && events.length) {
      events = events.filter(e => e[1] == "page_load");
      if (events.length >= 1) {
        return true;
      }
    }
    return false;
  }, "waiting for page load event to be flushed.");

  Assert.ok(event_found);

  BrowserTestUtils.removeTab(tab);
});
