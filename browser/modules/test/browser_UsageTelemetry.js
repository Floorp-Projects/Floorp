"use strict";

const MAX_CONCURRENT_TABS = "browser.engagement.max_concurrent_tab_count";
const TAB_EVENT_COUNT = "browser.engagement.tab_open_event_count";
const MAX_CONCURRENT_WINDOWS = "browser.engagement.max_concurrent_window_count";
const WINDOW_OPEN_COUNT = "browser.engagement.window_open_event_count";

const TELEMETRY_SUBSESSION_TOPIC = "internal-telemetry-after-subsession-split";

/**
 * An helper that checks the value of a scalar if it's expected to be > 0,
 * otherwise makes sure that the scalar it's not reported.
 */
let checkScalar = (scalars, scalarName, value, msg) => {
  if (value > 0) {
    is(scalars[scalarName], value, msg);
    return;
  }
  ok(!(scalarName in scalars), scalarName + " must not be reported.");
};

/**
 * Get a snapshot of the scalars and check them against the provided values.
 */
let checkScalars = (maxTabs, tabOpenCount, maxWindows, windowsOpenCount) => {
  const scalars =
    Services.telemetry.snapshotScalars(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN);

  // Check the expected values. Scalars that are never set must not be reported.
  checkScalar(scalars, MAX_CONCURRENT_TABS, maxTabs,
              "The maximum tab count must match the expected value.");
  checkScalar(scalars, TAB_EVENT_COUNT, tabOpenCount,
              "The number of open tab event count must match the expected value.");
  checkScalar(scalars, MAX_CONCURRENT_WINDOWS, maxWindows,
              "The maximum window count must match the expected value.");
  checkScalar(scalars, WINDOW_OPEN_COUNT, windowsOpenCount,
              "The number of window open event count must match the expected value.");
};

add_task(function* test_tabsAndWindows() {
  // Let's reset the counts.
  Services.telemetry.clearScalars();

  let openedTabs = [];
  let expectedTabOpenCount = 0;
  let expectedWinOpenCount = 0;
  let expectedMaxTabs = 0;
  let expectedMaxWins = 0;

  // Add a new tab and check that the count is right.
  openedTabs.push(yield BrowserTestUtils.openNewForegroundTab(gBrowser, "about:blank"));
  expectedTabOpenCount = 1;
  expectedMaxTabs = 2;
  checkScalars(expectedMaxTabs, expectedTabOpenCount, expectedMaxWins, expectedWinOpenCount);

  // Add two new tabs in the same window.
  openedTabs.push(yield BrowserTestUtils.openNewForegroundTab(gBrowser, "about:blank"));
  openedTabs.push(yield BrowserTestUtils.openNewForegroundTab(gBrowser, "about:blank"));
  expectedTabOpenCount += 2;
  expectedMaxTabs += 2;
  checkScalars(expectedMaxTabs, expectedTabOpenCount, expectedMaxWins, expectedWinOpenCount);

  // Add a new window and then some tabs in it. An empty new windows counts as a tab.
  let win = yield BrowserTestUtils.openNewBrowserWindow();
  openedTabs.push(yield BrowserTestUtils.openNewForegroundTab(win.gBrowser, "about:blank"));
  openedTabs.push(yield BrowserTestUtils.openNewForegroundTab(win.gBrowser, "about:blank"));
  openedTabs.push(yield BrowserTestUtils.openNewForegroundTab(gBrowser, "about:blank"));
  // The new window started with a new tab, so account for it.
  expectedTabOpenCount += 4;
  expectedWinOpenCount += 1;
  expectedMaxWins = 2;
  expectedMaxTabs += 4;

  // Remove a tab from the first window, the max shouldn't change.
  yield BrowserTestUtils.removeTab(openedTabs.pop());
  checkScalars(expectedMaxTabs, expectedTabOpenCount, expectedMaxWins, expectedWinOpenCount);

  // Remove all the extra windows and tabs.
  for (let tab of openedTabs) {
    yield BrowserTestUtils.removeTab(tab);
  }
  yield BrowserTestUtils.closeWindow(win);

  // Make sure all the scalars still have the expected values.
  checkScalars(expectedMaxTabs, expectedTabOpenCount, expectedMaxWins, expectedWinOpenCount);
});

add_task(function* test_subsessionSplit() {
  // Let's reset the counts.
  Services.telemetry.clearScalars();

  // Add a new window (that will have 4 tabs).
  let win = yield BrowserTestUtils.openNewBrowserWindow();
  let openedTabs = [];
  openedTabs.push(yield BrowserTestUtils.openNewForegroundTab(win.gBrowser, "about:blank"));
  openedTabs.push(yield BrowserTestUtils.openNewForegroundTab(win.gBrowser, "about:blank"));
  openedTabs.push(yield BrowserTestUtils.openNewForegroundTab(win.gBrowser, "about:blank"));

  // Check that the scalars have the right values.
  checkScalars(5 /*maxTabs*/, 4 /*tabOpen*/, 2 /*maxWins*/, 1 /*winOpen*/);

  // Remove a tab.
  yield BrowserTestUtils.removeTab(openedTabs.pop());

  // Simulate a subsession split by clearing the scalars (via |snapshotScalars|) and
  // notifying the subsession split topic.
  Services.telemetry.snapshotScalars(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN,
                                     true /* clearScalars*/);
  Services.obs.notifyObservers(null, TELEMETRY_SUBSESSION_TOPIC, "");

  // After a subsession split, only the MAX_CONCURRENT_* scalars must be available
  // and have the correct value. No tabs or windows were opened so other scalars
  // must not be reported.
  checkScalars(4 /*maxTabs*/, 0 /*tabOpen*/, 2 /*maxWins*/, 0 /*winOpen*/);

  // Remove all the extra windows and tabs.
  for (let tab of openedTabs) {
    yield BrowserTestUtils.removeTab(tab);
  }
  yield BrowserTestUtils.closeWindow(win);
});

add_task(function* test_privateMode() {
  // Let's reset the counts.
  Services.telemetry.clearScalars();

  // Open a private window and load a website in it.
  let privateWin = yield BrowserTestUtils.openNewBrowserWindow({private: true});
  yield BrowserTestUtils.loadURI(privateWin.gBrowser.selectedBrowser, "http://example.com/");
  yield BrowserTestUtils.browserLoaded(privateWin.gBrowser.selectedBrowser);

  // Check that tab and window count is recorded.
  const scalars =
    Services.telemetry.snapshotScalars(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN);

  is(scalars[TAB_EVENT_COUNT], 1, "The number of open tab event count must match the expected value.");
  is(scalars[MAX_CONCURRENT_TABS], 2, "The maximum tab count must match the expected value.");
  is(scalars[WINDOW_OPEN_COUNT], 1, "The number of window open event count must match the expected value.");
  is(scalars[MAX_CONCURRENT_WINDOWS], 2, "The maximum window count must match the expected value.");

  // Clean up.
  yield BrowserTestUtils.closeWindow(privateWin);
});
