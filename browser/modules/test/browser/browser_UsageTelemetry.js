"use strict";

const MAX_CONCURRENT_TABS = "browser.engagement.max_concurrent_tab_count";
const TAB_EVENT_COUNT = "browser.engagement.tab_open_event_count";
const MAX_CONCURRENT_WINDOWS = "browser.engagement.max_concurrent_window_count";
const WINDOW_OPEN_COUNT = "browser.engagement.window_open_event_count";
const TOTAL_URI_COUNT = "browser.engagement.total_uri_count";
const UNIQUE_DOMAINS_COUNT = "browser.engagement.unique_domains_count";
const UNFILTERED_URI_COUNT = "browser.engagement.unfiltered_uri_count";

const TELEMETRY_SUBSESSION_TOPIC = "internal-telemetry-after-subsession-split";

XPCOMUtils.defineLazyModuleGetter(this, "MINIMUM_TAB_COUNT_INTERVAL_MS",
                                  "resource:///modules/BrowserUsageTelemetry.jsm");

// Reset internal URI counter in case URIs were opened by other tests.
Services.obs.notifyObservers(null, TELEMETRY_SUBSESSION_TOPIC);

/**
 * Get a snapshot of the scalars and check them against the provided values.
 */
let checkScalars = (countsObject) => {
  const scalars = getParentProcessScalars(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN);

  // Check the expected values. Scalars that are never set must not be reported.
  checkScalar(scalars, MAX_CONCURRENT_TABS, countsObject.maxTabs,
              "The maximum tab count must match the expected value.");
  checkScalar(scalars, TAB_EVENT_COUNT, countsObject.tabOpenCount,
              "The number of open tab event count must match the expected value.");
  checkScalar(scalars, MAX_CONCURRENT_WINDOWS, countsObject.maxWindows,
              "The maximum window count must match the expected value.");
  checkScalar(scalars, WINDOW_OPEN_COUNT, countsObject.windowsOpenCount,
              "The number of window open event count must match the expected value.");
  checkScalar(scalars, TOTAL_URI_COUNT, countsObject.totalURIs,
              "The total URI count must match the expected value.");
  checkScalar(scalars, UNIQUE_DOMAINS_COUNT, countsObject.domainCount,
              "The unique domains count must match the expected value.");
  checkScalar(scalars, UNFILTERED_URI_COUNT, countsObject.totalUnfilteredURIs,
              "The unfiltered URI count must match the expected value.");
};

add_task(async function test_tabsAndWindows() {
  // Let's reset the counts.
  Services.telemetry.clearScalars();

  let openedTabs = [];
  let expectedTabOpenCount = 0;
  let expectedWinOpenCount = 0;
  let expectedMaxTabs = 0;
  let expectedMaxWins = 0;

  // Add a new tab and check that the count is right.
  openedTabs.push(await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:blank"));
  expectedTabOpenCount = 1;
  expectedMaxTabs = 2;
  // This, and all the checks below, also check that initial pages (about:newtab, about:blank, ..)
  // are not counted by the total_uri_count and the unfiltered_uri_count probes.
  checkScalars({maxTabs: expectedMaxTabs, tabOpenCount: expectedTabOpenCount, maxWindows: expectedMaxWins,
                windowsOpenCount: expectedWinOpenCount, totalURIs: 0, domainCount: 0,
                totalUnfilteredURIs: 0});

  // Add two new tabs in the same window.
  openedTabs.push(await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:blank"));
  openedTabs.push(await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:blank"));
  expectedTabOpenCount += 2;
  expectedMaxTabs += 2;
  checkScalars({maxTabs: expectedMaxTabs, tabOpenCount: expectedTabOpenCount, maxWindows: expectedMaxWins,
                windowsOpenCount: expectedWinOpenCount, totalURIs: 0, domainCount: 0,
                totalUnfilteredURIs: 0});

  // Add a new window and then some tabs in it. An empty new windows counts as a tab.
  let win = await BrowserTestUtils.openNewBrowserWindow();
  openedTabs.push(await BrowserTestUtils.openNewForegroundTab(win.gBrowser, "about:blank"));
  openedTabs.push(await BrowserTestUtils.openNewForegroundTab(win.gBrowser, "about:blank"));
  openedTabs.push(await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:blank"));
  // The new window started with a new tab, so account for it.
  expectedTabOpenCount += 4;
  expectedWinOpenCount += 1;
  expectedMaxWins = 2;
  expectedMaxTabs += 4;

  // Remove a tab from the first window, the max shouldn't change.
  await BrowserTestUtils.removeTab(openedTabs.pop());
  checkScalars({maxTabs: expectedMaxTabs, tabOpenCount: expectedTabOpenCount, maxWindows: expectedMaxWins,
                windowsOpenCount: expectedWinOpenCount, totalURIs: 0, domainCount: 0,
                totalUnfilteredURIs: 0});

  // Remove all the extra windows and tabs.
  for (let tab of openedTabs) {
    await BrowserTestUtils.removeTab(tab);
  }
  await BrowserTestUtils.closeWindow(win);

  // Make sure all the scalars still have the expected values.
  checkScalars({maxTabs: expectedMaxTabs, tabOpenCount: expectedTabOpenCount, maxWindows: expectedMaxWins,
                windowsOpenCount: expectedWinOpenCount, totalURIs: 0, domainCount: 0,
                totalUnfilteredURIs: 0});
});

add_task(async function test_subsessionSplit() {
  // Let's reset the counts.
  Services.telemetry.clearScalars();

  // Add a new window (that will have 4 tabs).
  let win = await BrowserTestUtils.openNewBrowserWindow();
  let openedTabs = [];
  openedTabs.push(await BrowserTestUtils.openNewForegroundTab(win.gBrowser, "about:blank"));
  openedTabs.push(await BrowserTestUtils.openNewForegroundTab(win.gBrowser, "about:mozilla"));
  openedTabs.push(await BrowserTestUtils.openNewForegroundTab(win.gBrowser, "http://www.example.com"));

  // Check that the scalars have the right values. We expect 2 unfiltered URI loads
  // (about:mozilla and www.example.com, but no about:blank) and 1 URI totalURIs
  // (only www.example.com).
  checkScalars({maxTabs: 5, tabOpenCount: 4, maxWindows: 2, windowsOpenCount: 1,
                totalURIs: 1, domainCount: 1, totalUnfilteredURIs: 2});

  // Remove a tab.
  await BrowserTestUtils.removeTab(openedTabs.pop());

  // Simulate a subsession split by clearing the scalars (via |snapshotScalars|) and
  // notifying the subsession split topic.
  Services.telemetry.snapshotScalars(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN,
                                     true /* clearScalars */);
  Services.obs.notifyObservers(null, TELEMETRY_SUBSESSION_TOPIC);

  // After a subsession split, only the MAX_CONCURRENT_* scalars must be available
  // and have the correct value. No tabs, windows or URIs were opened so other scalars
  // must not be reported.
  checkScalars({maxTabs: 4, tabOpenCount: 0, maxWindows: 2, windowsOpenCount: 0,
                totalURIs: 0, domainCount: 0, totalUnfilteredURIs: 0});

  // Remove all the extra windows and tabs.
  for (let tab of openedTabs) {
    await BrowserTestUtils.removeTab(tab);
  }
  await BrowserTestUtils.closeWindow(win);
});

function checkTabCountHistogram(result, expected, message) {
  let expectedPadded = result.counts.map((val, idx) => idx < expected.length ? expected[idx] : 0);
  Assert.deepEqual(result.counts, expectedPadded, message);
}

add_task(async function test_tabsHistogram() {
  let openedTabs = [];
  let tabCountHist = getAndClearHistogram("TAB_COUNT");

  checkTabCountHistogram(tabCountHist.snapshot(), [0, 0], "TAB_COUNT telemetry - initial tab counts");

  // Add a new tab and check that the count is right.
  BrowserUsageTelemetry._lastRecordTabCount = 0;
  openedTabs.push(await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:blank"));
  checkTabCountHistogram(tabCountHist.snapshot(), [0, 0, 1], "TAB_COUNT telemetry - opening tabs");

  // Open a different page and check the counts.
  BrowserUsageTelemetry._lastRecordTabCount = 0;
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:blank");
  openedTabs.push(tab);
  BrowserUsageTelemetry._lastRecordTabCount = 0;
  await BrowserTestUtils.loadURI(tab.linkedBrowser, "http://example.com/");
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  checkTabCountHistogram(tabCountHist.snapshot(), [0, 0, 1, 2], "TAB_COUNT telemetry - loading page");

  // Open another tab
  BrowserUsageTelemetry._lastRecordTabCount = 0;
  openedTabs.push(await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:blank"));
  checkTabCountHistogram(tabCountHist.snapshot(), [0, 0, 1, 2, 1], "TAB_COUNT telemetry - opening more tabs");

  // Add a new window and then some tabs in it. A new window starts with one tab.
  BrowserUsageTelemetry._lastRecordTabCount = 0;
  let win = await BrowserTestUtils.openNewBrowserWindow();
  checkTabCountHistogram(tabCountHist.snapshot(), [0, 0, 1, 2, 1, 1], "TAB_COUNT telemetry - opening window");

  // Do not trigger a recount if _lastRecordTabCount is recent on new tab
  BrowserUsageTelemetry._lastRecordTabCount = Date.now() - (MINIMUM_TAB_COUNT_INTERVAL_MS / 2);
  {
    let oldLastRecordTabCount = BrowserUsageTelemetry._lastRecordTabCount;
    openedTabs.push(await BrowserTestUtils.openNewForegroundTab(win.gBrowser, "about:blank"));
    checkTabCountHistogram(tabCountHist.snapshot(), [0, 0, 1, 2, 1, 1, 0], "TAB_COUNT telemetry - new tab, recount event ignored");
    ok(BrowserUsageTelemetry._lastRecordTabCount == oldLastRecordTabCount, "TAB_COUNT telemetry - _lastRecordTabCount unchanged");
  }

  // Trigger a recount if _lastRecordTabCount has passed on new tab
  BrowserUsageTelemetry._lastRecordTabCount = Date.now() - (MINIMUM_TAB_COUNT_INTERVAL_MS + 1000);
  {
    let oldLastRecordTabCount = BrowserUsageTelemetry._lastRecordTabCount;
    openedTabs.push(await BrowserTestUtils.openNewForegroundTab(win.gBrowser, "about:blank"));
    checkTabCountHistogram(tabCountHist.snapshot(), [0, 0, 1, 2, 1, 1, 0, 1], "TAB_COUNT telemetry - new tab, recount event included");
    ok(BrowserUsageTelemetry._lastRecordTabCount != oldLastRecordTabCount, "TAB_COUNT telemetry - _lastRecordTabCount updated");
    ok(BrowserUsageTelemetry._lastRecordTabCount > Date.now() - MINIMUM_TAB_COUNT_INTERVAL_MS, "TAB_COUNT telemetry - _lastRecordTabCount invariant");
  }

  // Do not trigger a recount if _lastRecordTabCount is recent on page load
  BrowserUsageTelemetry._lastRecordTabCount = Date.now() - (MINIMUM_TAB_COUNT_INTERVAL_MS / 2);
  {
    let oldLastRecordTabCount = BrowserUsageTelemetry._lastRecordTabCount;
    await BrowserTestUtils.loadURI(tab.linkedBrowser, "http://example.com/");
    await BrowserTestUtils.browserLoaded(tab.linkedBrowser);
    checkTabCountHistogram(tabCountHist.snapshot(), [0, 0, 1, 2, 1, 1, 0, 1], "TAB_COUNT telemetry - page load, recount event ignored");
    ok(BrowserUsageTelemetry._lastRecordTabCount == oldLastRecordTabCount, "TAB_COUNT telemetry - _lastRecordTabCount unchanged");
  }

  // Trigger a recount if _lastRecordTabCount has passed on page load
  BrowserUsageTelemetry._lastRecordTabCount = Date.now() - (MINIMUM_TAB_COUNT_INTERVAL_MS + 1000);
  {
    let oldLastRecordTabCount = BrowserUsageTelemetry._lastRecordTabCount;
    await BrowserTestUtils.loadURI(tab.linkedBrowser, "http://example.com/");
    await BrowserTestUtils.browserLoaded(tab.linkedBrowser);
    checkTabCountHistogram(tabCountHist.snapshot(), [0, 0, 1, 2, 1, 1, 0, 2], "TAB_COUNT telemetry - page load, recount event included");
    ok(BrowserUsageTelemetry._lastRecordTabCount != oldLastRecordTabCount, "TAB_COUNT telemetry - _lastRecordTabCount updated");
    ok(BrowserUsageTelemetry._lastRecordTabCount > Date.now() - MINIMUM_TAB_COUNT_INTERVAL_MS, "TAB_COUNT telemetry - _lastRecordTabCount invariant");
  }

  // Remove all the extra windows and tabs.
  for (let openTab of openedTabs) {
    await BrowserTestUtils.removeTab(openTab);
  }
  await BrowserTestUtils.closeWindow(win);
});
