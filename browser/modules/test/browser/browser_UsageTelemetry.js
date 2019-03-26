"use strict";

const MAX_CONCURRENT_TABS = "browser.engagement.max_concurrent_tab_count";
const TAB_EVENT_COUNT = "browser.engagement.tab_open_event_count";
const MAX_CONCURRENT_WINDOWS = "browser.engagement.max_concurrent_window_count";
const MAX_TAB_PINNED = "browser.engagement.max_concurrent_tab_pinned_count";
const TAB_PINNED_EVENT = "browser.engagement.tab_pinned_event_count";
const WINDOW_OPEN_COUNT = "browser.engagement.window_open_event_count";
const TOTAL_URI_COUNT = "browser.engagement.total_uri_count";
const UNIQUE_DOMAINS_COUNT = "browser.engagement.unique_domains_count";
const UNFILTERED_URI_COUNT = "browser.engagement.unfiltered_uri_count";

const TELEMETRY_SUBSESSION_TOPIC = "internal-telemetry-after-subsession-split";

ChromeUtils.defineModuleGetter(this, "MINIMUM_TAB_COUNT_INTERVAL_MS",
                               "resource:///modules/BrowserUsageTelemetry.jsm");

// Reset internal URI counter in case URIs were opened by other tests.
Services.obs.notifyObservers(null, TELEMETRY_SUBSESSION_TOPIC);

/**
 * Get a snapshot of the scalars and check them against the provided values.
 */
let checkScalars = (countsObject) => {
  const scalars = TelemetryTestUtils.getProcessScalars("parent");

  // Check the expected values. Scalars that are never set must not be reported.
  TelemetryTestUtils.assertScalar(scalars, MAX_CONCURRENT_TABS, countsObject.maxTabs,
    "The maximum tab count must match the expected value.");
  TelemetryTestUtils.assertScalar(scalars, TAB_EVENT_COUNT, countsObject.tabOpenCount,
    "The number of open tab event count must match the expected value.");
  TelemetryTestUtils.assertScalar(scalars, MAX_TAB_PINNED, countsObject.maxTabsPinned,
    "The maximum tabs pinned count must match the expected value.");
  TelemetryTestUtils.assertScalar(scalars, TAB_PINNED_EVENT, countsObject.tabPinnedCount,
    "The number of tab pinned event count must match the expected value.");
  TelemetryTestUtils.assertScalar(scalars, MAX_CONCURRENT_WINDOWS, countsObject.maxWindows,
    "The maximum window count must match the expected value.");
  TelemetryTestUtils.assertScalar(scalars, WINDOW_OPEN_COUNT, countsObject.windowsOpenCount,
    "The number of window open event count must match the expected value.");
  TelemetryTestUtils.assertScalar(scalars, TOTAL_URI_COUNT, countsObject.totalURIs,
    "The total URI count must match the expected value.");
  TelemetryTestUtils.assertScalar(scalars, UNIQUE_DOMAINS_COUNT, countsObject.domainCount,
    "The unique domains count must match the expected value.");
  TelemetryTestUtils.assertScalar(scalars, UNFILTERED_URI_COUNT, countsObject.totalUnfilteredURIs,
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
  let expectedMaxTabsPinned = 0;
  let expectedTabPinned = 0;

  // Add a new tab and check that the count is right.
  openedTabs.push(await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:blank"));

  gBrowser.pinTab(openedTabs[0]);
  gBrowser.unpinTab(openedTabs[0]);

  expectedTabOpenCount = 1;
  expectedMaxTabs = 2;
  expectedMaxTabsPinned = 1;
  expectedTabPinned += 1;
  // This, and all the checks below, also check that initial pages (about:newtab, about:blank, ..)
  // are not counted by the total_uri_count and the unfiltered_uri_count probes.
  checkScalars({maxTabs: expectedMaxTabs, tabOpenCount: expectedTabOpenCount, maxWindows: expectedMaxWins,
                windowsOpenCount: expectedWinOpenCount, totalURIs: 0, domainCount: 0,
                totalUnfilteredURIs: 0, maxTabsPinned: expectedMaxTabsPinned,
                tabPinnedCount: expectedTabPinned});

  // Add two new tabs in the same window.
  openedTabs.push(await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:blank"));
  openedTabs.push(await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:blank"));

  gBrowser.pinTab(openedTabs[1]);
  gBrowser.pinTab(openedTabs[2]);
  gBrowser.unpinTab(openedTabs[2]);
  gBrowser.unpinTab(openedTabs[1]);

  expectedTabOpenCount += 2;
  expectedMaxTabs += 2;
  expectedMaxTabsPinned = 2;
  expectedTabPinned += 2;
  checkScalars({maxTabs: expectedMaxTabs, tabOpenCount: expectedTabOpenCount, maxWindows: expectedMaxWins,
                windowsOpenCount: expectedWinOpenCount, totalURIs: 0, domainCount: 0,
                totalUnfilteredURIs: 0, maxTabsPinned: expectedMaxTabsPinned,
                tabPinnedCount: expectedTabPinned});

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
  BrowserTestUtils.removeTab(openedTabs.pop());
  checkScalars({maxTabs: expectedMaxTabs, tabOpenCount: expectedTabOpenCount, maxWindows: expectedMaxWins,
                windowsOpenCount: expectedWinOpenCount, totalURIs: 0, domainCount: 0,
                totalUnfilteredURIs: 0, maxTabsPinned: expectedMaxTabsPinned,
                tabPinnedCount: expectedTabPinned});

  // Remove all the extra windows and tabs.
  for (let tab of openedTabs) {
    BrowserTestUtils.removeTab(tab);
  }
  await BrowserTestUtils.closeWindow(win);

  // Make sure all the scalars still have the expected values.
  checkScalars({maxTabs: expectedMaxTabs, tabOpenCount: expectedTabOpenCount, maxWindows: expectedMaxWins,
                windowsOpenCount: expectedWinOpenCount, totalURIs: 0, domainCount: 0,
                totalUnfilteredURIs: 0, maxTabsPinned: expectedMaxTabsPinned,
                tabPinnedCount: expectedTabPinned});
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
                totalURIs: 1, domainCount: 1, totalUnfilteredURIs: 2, maxTabsPinned: 0,
                tabPinnedCount: 0});

  // Remove a tab.
  BrowserTestUtils.removeTab(openedTabs.pop());

  // Simulate a subsession split by clearing the scalars (via |getSnapshotForScalars|) and
  // notifying the subsession split topic.
  Services.telemetry.getSnapshotForScalars("main", true /* clearScalars */);
  Services.obs.notifyObservers(null, TELEMETRY_SUBSESSION_TOPIC);

  // After a subsession split, only the MAX_CONCURRENT_* scalars must be available
  // and have the correct value. No tabs, windows or URIs were opened so other scalars
  // must not be reported.
  checkScalars({maxTabs: 4, tabOpenCount: 0, maxWindows: 2, windowsOpenCount: 0,
                totalURIs: 0, domainCount: 0, totalUnfilteredURIs: 0, maxTabsPinned: 0,
                tabPinnedCount: 0});

  // Remove all the extra windows and tabs.
  for (let tab of openedTabs) {
    BrowserTestUtils.removeTab(tab);
  }
  await BrowserTestUtils.closeWindow(win);
});

function checkTabCountHistogram(result, expected, message) {
  Assert.deepEqual(result.values, expected, message);
}

add_task(async function test_tabsHistogram() {
  let openedTabs = [];
  let tabCountHist = TelemetryTestUtils.getAndClearHistogram("TAB_COUNT");

  checkTabCountHistogram(tabCountHist.snapshot(), {}, "TAB_COUNT telemetry - initial tab counts");

  // Add a new tab and check that the count is right.
  BrowserUsageTelemetry._lastRecordTabCount = 0;
  openedTabs.push(await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:blank"));
  checkTabCountHistogram(tabCountHist.snapshot(), {1: 0, 2: 1, 3: 0}, "TAB_COUNT telemetry - opening tabs");

  // Open a different page and check the counts.
  BrowserUsageTelemetry._lastRecordTabCount = 0;
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:blank");
  openedTabs.push(tab);
  BrowserUsageTelemetry._lastRecordTabCount = 0;
  await BrowserTestUtils.loadURI(tab.linkedBrowser, "http://example.com/");
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  checkTabCountHistogram(tabCountHist.snapshot(), {1: 0, 2: 1, 3: 2, 4: 0}, "TAB_COUNT telemetry - loading page");

  // Open another tab
  BrowserUsageTelemetry._lastRecordTabCount = 0;
  openedTabs.push(await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:blank"));
  checkTabCountHistogram(tabCountHist.snapshot(), {1: 0, 2: 1, 3: 2, 4: 1, 5: 0}, "TAB_COUNT telemetry - opening more tabs");

  // Add a new window and then some tabs in it. A new window starts with one tab.
  BrowserUsageTelemetry._lastRecordTabCount = 0;
  let win = await BrowserTestUtils.openNewBrowserWindow();
  checkTabCountHistogram(tabCountHist.snapshot(), {1: 0, 2: 1, 3: 2, 4: 1, 5: 1, 6: 0}, "TAB_COUNT telemetry - opening window");

  // Do not trigger a recount if _lastRecordTabCount is recent on new tab
  BrowserUsageTelemetry._lastRecordTabCount = Date.now() - (MINIMUM_TAB_COUNT_INTERVAL_MS / 2);
  {
    let oldLastRecordTabCount = BrowserUsageTelemetry._lastRecordTabCount;
    openedTabs.push(await BrowserTestUtils.openNewForegroundTab(win.gBrowser, "about:blank"));
    checkTabCountHistogram(tabCountHist.snapshot(), {1: 0, 2: 1, 3: 2, 4: 1, 5: 1, 6: 0}, "TAB_COUNT telemetry - new tab, recount event ignored");
    ok(BrowserUsageTelemetry._lastRecordTabCount == oldLastRecordTabCount, "TAB_COUNT telemetry - _lastRecordTabCount unchanged");
  }

  // Trigger a recount if _lastRecordTabCount has passed on new tab
  BrowserUsageTelemetry._lastRecordTabCount = Date.now() - (MINIMUM_TAB_COUNT_INTERVAL_MS + 1000);
  {
    let oldLastRecordTabCount = BrowserUsageTelemetry._lastRecordTabCount;
    openedTabs.push(await BrowserTestUtils.openNewForegroundTab(win.gBrowser, "about:blank"));
    checkTabCountHistogram(tabCountHist.snapshot(), {1: 0, 2: 1, 3: 2, 4: 1, 5: 1, 7: 1, 8: 0}, "TAB_COUNT telemetry - new tab, recount event included");
    ok(BrowserUsageTelemetry._lastRecordTabCount != oldLastRecordTabCount, "TAB_COUNT telemetry - _lastRecordTabCount updated");
    ok(BrowserUsageTelemetry._lastRecordTabCount > Date.now() - MINIMUM_TAB_COUNT_INTERVAL_MS, "TAB_COUNT telemetry - _lastRecordTabCount invariant");
  }

  // Do not trigger a recount if _lastRecordTabCount is recent on page load
  BrowserUsageTelemetry._lastRecordTabCount = Date.now() - (MINIMUM_TAB_COUNT_INTERVAL_MS / 2);
  {
    let oldLastRecordTabCount = BrowserUsageTelemetry._lastRecordTabCount;
    await BrowserTestUtils.loadURI(tab.linkedBrowser, "http://example.com/");
    await BrowserTestUtils.browserLoaded(tab.linkedBrowser);
    checkTabCountHistogram(tabCountHist.snapshot(), {1: 0, 2: 1, 3: 2, 4: 1, 5: 1, 7: 1, 8: 0}, "TAB_COUNT telemetry - page load, recount event ignored");
    ok(BrowserUsageTelemetry._lastRecordTabCount == oldLastRecordTabCount, "TAB_COUNT telemetry - _lastRecordTabCount unchanged");
  }

  // Trigger a recount if _lastRecordTabCount has passed on page load
  BrowserUsageTelemetry._lastRecordTabCount = Date.now() - (MINIMUM_TAB_COUNT_INTERVAL_MS + 1000);
  {
    let oldLastRecordTabCount = BrowserUsageTelemetry._lastRecordTabCount;
    await BrowserTestUtils.loadURI(tab.linkedBrowser, "http://example.com/");
    await BrowserTestUtils.browserLoaded(tab.linkedBrowser);
    checkTabCountHistogram(tabCountHist.snapshot(), {1: 0, 2: 1, 3: 2, 4: 1, 5: 1, 7: 2, 8: 0}, "TAB_COUNT telemetry - page load, recount event included");
    ok(BrowserUsageTelemetry._lastRecordTabCount != oldLastRecordTabCount, "TAB_COUNT telemetry - _lastRecordTabCount updated");
    ok(BrowserUsageTelemetry._lastRecordTabCount > Date.now() - MINIMUM_TAB_COUNT_INTERVAL_MS, "TAB_COUNT telemetry - _lastRecordTabCount invariant");
  }

  // Remove all the extra windows and tabs.
  for (let openTab of openedTabs) {
    BrowserTestUtils.removeTab(openTab);
  }
  await BrowserTestUtils.closeWindow(win);
});
