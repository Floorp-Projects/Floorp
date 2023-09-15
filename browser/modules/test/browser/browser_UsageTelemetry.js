"use strict";

requestLongerTimeout(2);

const MAX_CONCURRENT_TABS = "browser.engagement.max_concurrent_tab_count";
const TAB_EVENT_COUNT = "browser.engagement.tab_open_event_count";
const MAX_CONCURRENT_WINDOWS = "browser.engagement.max_concurrent_window_count";
const MAX_TAB_PINNED = "browser.engagement.max_concurrent_tab_pinned_count";
const TAB_PINNED_EVENT = "browser.engagement.tab_pinned_event_count";
const WINDOW_OPEN_COUNT = "browser.engagement.window_open_event_count";
const TOTAL_URI_COUNT = "browser.engagement.total_uri_count";
const UNIQUE_DOMAINS_COUNT = "browser.engagement.unique_domains_count";
const UNFILTERED_URI_COUNT = "browser.engagement.unfiltered_uri_count";
const TOTAL_URI_COUNT_NORMAL_AND_PRIVATE_MODE =
  "browser.engagement.total_uri_count_normal_and_private_mode";

const TELEMETRY_SUBSESSION_TOPIC = "internal-telemetry-after-subsession-split";

const RESTORE_ON_DEMAND_PREF = "browser.sessionstore.restore_on-demand";

ChromeUtils.defineESModuleGetters(this, {
  MINIMUM_TAB_COUNT_INTERVAL_MS:
    "resource:///modules/BrowserUsageTelemetry.sys.mjs",
});

const { ObjectUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/ObjectUtils.sys.mjs"
);

BrowserUsageTelemetry._onTabsOpenedTask._timeoutMs = 0;
registerCleanupFunction(() => {
  BrowserUsageTelemetry._onTabsOpenedTask._timeoutMs = undefined;
});

// Reset internal URI counter in case URIs were opened by other tests.
Services.obs.notifyObservers(null, TELEMETRY_SUBSESSION_TOPIC);

/**
 * Get a snapshot of the scalars and check them against the provided values.
 */
let checkScalars = (countsObject, skipGleanCheck = false) => {
  const scalars = TelemetryTestUtils.getProcessScalars("parent");

  // Check the expected values. Scalars that are never set must not be reported.
  const checkScalar = (key, val, msg) =>
    val > 0
      ? TelemetryTestUtils.assertScalar(scalars, key, val, msg)
      : TelemetryTestUtils.assertScalarUnset(scalars, key);
  checkScalar(
    MAX_CONCURRENT_TABS,
    countsObject.maxTabs,
    "The maximum tab count must match the expected value."
  );
  checkScalar(
    TAB_EVENT_COUNT,
    countsObject.tabOpenCount,
    "The number of open tab event count must match the expected value."
  );
  checkScalar(
    MAX_TAB_PINNED,
    countsObject.maxTabsPinned,
    "The maximum tabs pinned count must match the expected value."
  );
  checkScalar(
    TAB_PINNED_EVENT,
    countsObject.tabPinnedCount,
    "The number of tab pinned event count must match the expected value."
  );
  checkScalar(
    MAX_CONCURRENT_WINDOWS,
    countsObject.maxWindows,
    "The maximum window count must match the expected value."
  );
  checkScalar(
    WINDOW_OPEN_COUNT,
    countsObject.windowsOpenCount,
    "The number of window open event count must match the expected value."
  );
  checkScalar(
    TOTAL_URI_COUNT,
    countsObject.totalURIs,
    "The total URI count must match the expected value."
  );
  checkScalar(
    UNIQUE_DOMAINS_COUNT,
    countsObject.domainCount,
    "The unique domains count must match the expected value."
  );
  checkScalar(
    UNFILTERED_URI_COUNT,
    countsObject.totalUnfilteredURIs,
    "The unfiltered URI count must match the expected value."
  );
  checkScalar(
    TOTAL_URI_COUNT_NORMAL_AND_PRIVATE_MODE,
    countsObject.totalURIsNormalAndPrivateMode,
    "The total URI count for both normal and private mode must match the expected value."
  );
  if (!skipGleanCheck) {
    if (countsObject.totalURIsNormalAndPrivateMode == 0) {
      Assert.equal(
        Glean.browserEngagement.uriCount.testGetValue(),
        undefined,
        "Total URI count reported in Glean must be unset."
      );
    } else {
      Assert.equal(
        countsObject.totalURIsNormalAndPrivateMode,
        Glean.browserEngagement.uriCount.testGetValue(),
        "The total URI count reported in Glean must be as expected."
      );
    }
  }
};

add_task(async function test_tabsAndWindows() {
  // Let's reset the counts.
  Services.telemetry.clearScalars();
  Services.fog.testResetFOG();

  let openedTabs = [];
  let expectedTabOpenCount = 0;
  let expectedWinOpenCount = 0;
  let expectedMaxTabs = 0;
  let expectedMaxWins = 0;
  let expectedMaxTabsPinned = 0;
  let expectedTabPinned = 0;
  let expectedTotalURIs = 0;

  // Add a new tab and check that the count is right.
  openedTabs.push(
    await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:blank")
  );

  gBrowser.pinTab(openedTabs[0]);
  gBrowser.unpinTab(openedTabs[0]);

  expectedTabOpenCount = 1;
  expectedMaxTabs = 2;
  expectedMaxTabsPinned = 1;
  expectedTabPinned += 1;
  // This, and all the checks below, also check that initial pages (about:newtab, about:blank, ..)
  // are not counted by the total_uri_count and the unfiltered_uri_count probes.
  checkScalars({
    maxTabs: expectedMaxTabs,
    tabOpenCount: expectedTabOpenCount,
    maxWindows: expectedMaxWins,
    windowsOpenCount: expectedWinOpenCount,
    totalURIs: expectedTotalURIs,
    domainCount: 0,
    totalUnfilteredURIs: 0,
    maxTabsPinned: expectedMaxTabsPinned,
    tabPinnedCount: expectedTabPinned,
    totalURIsNormalAndPrivateMode: expectedTotalURIs,
  });

  // Add two new tabs in the same window.
  openedTabs.push(
    await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:blank")
  );
  openedTabs.push(
    await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:blank")
  );

  gBrowser.pinTab(openedTabs[1]);
  gBrowser.pinTab(openedTabs[2]);
  gBrowser.unpinTab(openedTabs[2]);
  gBrowser.unpinTab(openedTabs[1]);

  expectedTabOpenCount += 2;
  expectedMaxTabs += 2;
  expectedMaxTabsPinned = 2;
  expectedTabPinned += 2;
  checkScalars({
    maxTabs: expectedMaxTabs,
    tabOpenCount: expectedTabOpenCount,
    maxWindows: expectedMaxWins,
    windowsOpenCount: expectedWinOpenCount,
    totalURIs: expectedTotalURIs,
    domainCount: 0,
    totalUnfilteredURIs: 0,
    maxTabsPinned: expectedMaxTabsPinned,
    tabPinnedCount: expectedTabPinned,
    totalURIsNormalAndPrivateMode: expectedTotalURIs,
  });

  // Add a new window and then some tabs in it. An empty new windows counts as a tab.
  let win = await BrowserTestUtils.openNewBrowserWindow();
  await BrowserTestUtils.firstBrowserLoaded(win);
  openedTabs.push(
    await BrowserTestUtils.openNewForegroundTab(win.gBrowser, "about:blank")
  );
  openedTabs.push(
    await BrowserTestUtils.openNewForegroundTab(win.gBrowser, "about:blank")
  );
  openedTabs.push(
    await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:blank")
  );
  // The new window started with a new tab, so account for it.
  expectedTabOpenCount += 4;
  expectedWinOpenCount += 1;
  expectedMaxWins = 2;
  expectedMaxTabs += 4;

  // Remove a tab from the first window, the max shouldn't change.
  BrowserTestUtils.removeTab(openedTabs.pop());
  checkScalars({
    maxTabs: expectedMaxTabs,
    tabOpenCount: expectedTabOpenCount,
    maxWindows: expectedMaxWins,
    windowsOpenCount: expectedWinOpenCount,
    totalURIs: expectedTotalURIs,
    domainCount: 0,
    totalUnfilteredURIs: 0,
    maxTabsPinned: expectedMaxTabsPinned,
    tabPinnedCount: expectedTabPinned,
    totalURIsNormalAndPrivateMode: expectedTotalURIs,
  });

  // Remove all the extra windows and tabs.
  for (let tab of openedTabs) {
    BrowserTestUtils.removeTab(tab);
  }
  await BrowserTestUtils.closeWindow(win);

  // Make sure all the scalars still have the expected values.
  checkScalars({
    maxTabs: expectedMaxTabs,
    tabOpenCount: expectedTabOpenCount,
    maxWindows: expectedMaxWins,
    windowsOpenCount: expectedWinOpenCount,
    totalURIs: expectedTotalURIs,
    domainCount: 0,
    totalUnfilteredURIs: 0,
    maxTabsPinned: expectedMaxTabsPinned,
    tabPinnedCount: expectedTabPinned,
    totalURIsNormalAndPrivateMode: expectedTotalURIs,
  });
});

add_task(async function test_subsessionSplit() {
  // Let's reset the counts.
  Services.telemetry.clearScalars();

  // Add a new window (that will have 4 tabs).
  let win = await BrowserTestUtils.openNewBrowserWindow();
  await BrowserTestUtils.firstBrowserLoaded(win);
  let openedTabs = [];
  openedTabs.push(
    await BrowserTestUtils.openNewForegroundTab(win.gBrowser, "about:blank")
  );
  openedTabs.push(
    await BrowserTestUtils.openNewForegroundTab(win.gBrowser, "about:mozilla")
  );
  openedTabs.push(
    await BrowserTestUtils.openNewForegroundTab(
      win.gBrowser,
      "http://www.example.com"
    )
  );

  // Check that the scalars have the right values. We expect 2 unfiltered URI loads
  // (about:mozilla and www.example.com, but no about:blank) and 1 URI totalURIs
  // (only www.example.com).
  let expectedTotalURIs = 1;

  checkScalars({
    maxTabs: 5,
    tabOpenCount: 4,
    maxWindows: 2,
    windowsOpenCount: 1,
    totalURIs: expectedTotalURIs,
    domainCount: 1,
    totalUnfilteredURIs: 2,
    maxTabsPinned: 0,
    tabPinnedCount: 0,
    totalURIsNormalAndPrivateMode: expectedTotalURIs,
  });

  // Remove a tab.
  BrowserTestUtils.removeTab(openedTabs.pop());

  // Simulate a subsession split by clearing the scalars (via |getSnapshotForScalars|) and
  // notifying the subsession split topic.
  Services.telemetry.getSnapshotForScalars("main", true /* clearScalars */);
  Services.obs.notifyObservers(null, TELEMETRY_SUBSESSION_TOPIC);

  // After a subsession split, only the MAX_CONCURRENT_* scalars must be available
  // and have the correct value. No tabs, windows or URIs were opened so other scalars
  // must not be reported.
  expectedTotalURIs = 0;

  checkScalars(
    {
      maxTabs: 4,
      tabOpenCount: 0,
      maxWindows: 2,
      windowsOpenCount: 0,
      totalURIs: expectedTotalURIs,
      domainCount: 0,
      totalUnfilteredURIs: 0,
      maxTabsPinned: 0,
      tabPinnedCount: 0,
      totalURIsNormalAndPrivateMode: expectedTotalURIs,
    },
    true
  );

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

  checkTabCountHistogram(
    tabCountHist.snapshot(),
    {},
    "TAB_COUNT telemetry - initial tab counts"
  );

  // Add a new tab and check that the count is right.
  BrowserUsageTelemetry._lastRecordTabCount = 0;
  openedTabs.push(
    await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:blank")
  );
  checkTabCountHistogram(
    tabCountHist.snapshot(),
    { 1: 0, 2: 1, 3: 0 },
    "TAB_COUNT telemetry - opening tabs"
  );

  // Open a different page and check the counts.
  BrowserUsageTelemetry._lastRecordTabCount = 0;
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:blank"
  );
  openedTabs.push(tab);
  BrowserUsageTelemetry._lastRecordTabCount = 0;
  BrowserTestUtils.startLoadingURIString(
    tab.linkedBrowser,
    "http://example.com/"
  );
  await BrowserTestUtils.browserLoaded(
    tab.linkedBrowser,
    false,
    "http://example.com/"
  );
  checkTabCountHistogram(
    tabCountHist.snapshot(),
    { 1: 0, 2: 1, 3: 2, 4: 0 },
    "TAB_COUNT telemetry - loading page"
  );

  // Open another tab
  BrowserUsageTelemetry._lastRecordTabCount = 0;
  openedTabs.push(
    await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:blank")
  );
  checkTabCountHistogram(
    tabCountHist.snapshot(),
    { 1: 0, 2: 1, 3: 2, 4: 1, 5: 0 },
    "TAB_COUNT telemetry - opening more tabs"
  );

  // Add a new window and then some tabs in it. A new window starts with one tab.
  BrowserUsageTelemetry._lastRecordTabCount = 0;
  let win = await BrowserTestUtils.openNewBrowserWindow();
  await BrowserTestUtils.firstBrowserLoaded(win);
  checkTabCountHistogram(
    tabCountHist.snapshot(),
    { 1: 0, 2: 1, 3: 2, 4: 1, 5: 1, 6: 0 },
    "TAB_COUNT telemetry - opening window"
  );

  // Do not trigger a recount if _lastRecordTabCount is recent on new tab
  BrowserUsageTelemetry._lastRecordTabCount =
    Date.now() - MINIMUM_TAB_COUNT_INTERVAL_MS / 2;
  {
    let oldLastRecordTabCount = BrowserUsageTelemetry._lastRecordTabCount;
    openedTabs.push(
      await BrowserTestUtils.openNewForegroundTab(win.gBrowser, "about:blank")
    );
    checkTabCountHistogram(
      tabCountHist.snapshot(),
      { 1: 0, 2: 1, 3: 2, 4: 1, 5: 1, 6: 0 },
      "TAB_COUNT telemetry - new tab, recount event ignored"
    );
    ok(
      BrowserUsageTelemetry._lastRecordTabCount == oldLastRecordTabCount,
      "TAB_COUNT telemetry - _lastRecordTabCount unchanged"
    );
  }

  // Trigger a recount if _lastRecordTabCount has passed on new tab
  BrowserUsageTelemetry._lastRecordTabCount =
    Date.now() - (MINIMUM_TAB_COUNT_INTERVAL_MS + 1000);
  {
    let oldLastRecordTabCount = BrowserUsageTelemetry._lastRecordTabCount;
    openedTabs.push(
      await BrowserTestUtils.openNewForegroundTab(win.gBrowser, "about:blank")
    );
    checkTabCountHistogram(
      tabCountHist.snapshot(),
      { 1: 0, 2: 1, 3: 2, 4: 1, 5: 1, 7: 1, 8: 0 },
      "TAB_COUNT telemetry - new tab, recount event included"
    );
    ok(
      BrowserUsageTelemetry._lastRecordTabCount != oldLastRecordTabCount,
      "TAB_COUNT telemetry - _lastRecordTabCount updated"
    );
    ok(
      BrowserUsageTelemetry._lastRecordTabCount >
        Date.now() - MINIMUM_TAB_COUNT_INTERVAL_MS,
      "TAB_COUNT telemetry - _lastRecordTabCount invariant"
    );
  }

  // Do not trigger a recount if _lastRecordTabCount is recent on page load
  BrowserUsageTelemetry._lastRecordTabCount =
    Date.now() - MINIMUM_TAB_COUNT_INTERVAL_MS / 2;
  {
    let oldLastRecordTabCount = BrowserUsageTelemetry._lastRecordTabCount;
    BrowserTestUtils.startLoadingURIString(
      tab.linkedBrowser,
      "http://example.com/"
    );
    await BrowserTestUtils.browserLoaded(
      tab.linkedBrowser,
      false,
      "http://example.com/"
    );
    checkTabCountHistogram(
      tabCountHist.snapshot(),
      { 1: 0, 2: 1, 3: 2, 4: 1, 5: 1, 7: 1, 8: 0 },
      "TAB_COUNT telemetry - page load, recount event ignored"
    );
    ok(
      BrowserUsageTelemetry._lastRecordTabCount == oldLastRecordTabCount,
      "TAB_COUNT telemetry - _lastRecordTabCount unchanged"
    );
  }

  // Trigger a recount if _lastRecordTabCount has passed on page load
  BrowserUsageTelemetry._lastRecordTabCount =
    Date.now() - (MINIMUM_TAB_COUNT_INTERVAL_MS + 1000);
  {
    let oldLastRecordTabCount = BrowserUsageTelemetry._lastRecordTabCount;
    BrowserTestUtils.startLoadingURIString(
      tab.linkedBrowser,
      "http://example.com/"
    );
    await BrowserTestUtils.browserLoaded(
      tab.linkedBrowser,
      false,
      "http://example.com/"
    );
    checkTabCountHistogram(
      tabCountHist.snapshot(),
      { 1: 0, 2: 1, 3: 2, 4: 1, 5: 1, 7: 2, 8: 0 },
      "TAB_COUNT telemetry - page load, recount event included"
    );
    ok(
      BrowserUsageTelemetry._lastRecordTabCount != oldLastRecordTabCount,
      "TAB_COUNT telemetry - _lastRecordTabCount updated"
    );
    ok(
      BrowserUsageTelemetry._lastRecordTabCount >
        Date.now() - MINIMUM_TAB_COUNT_INTERVAL_MS,
      "TAB_COUNT telemetry - _lastRecordTabCount invariant"
    );
  }

  // Remove all the extra windows and tabs.
  for (let openTab of openedTabs) {
    BrowserTestUtils.removeTab(openTab);
  }
  await BrowserTestUtils.closeWindow(win);
});

add_task(async function test_loadedTabsHistogram() {
  Services.prefs.setBoolPref(RESTORE_ON_DEMAND_PREF, true);
  registerCleanupFunction(() =>
    Services.prefs.clearUserPref(RESTORE_ON_DEMAND_PREF)
  );

  function resetTimestamps() {
    BrowserUsageTelemetry._lastRecordTabCount = 0;
    BrowserUsageTelemetry._lastRecordLoadedTabCount = 0;
  }

  resetTimestamps();
  const tabCount = TelemetryTestUtils.getAndClearHistogram("TAB_COUNT");
  const loadedTabCount =
    TelemetryTestUtils.getAndClearHistogram("LOADED_TAB_COUNT");

  checkTabCountHistogram(tabCount.snapshot(), {}, "TAB_COUNT - initial count");
  checkTabCountHistogram(
    loadedTabCount.snapshot(),
    {},
    "LOADED_TAB_COUNT - initial count"
  );

  resetTimestamps();
  const tabs = [
    await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:blank"),
  ];

  // There are two tabs open: the mochi.test tab and the foreground tab.
  const snapshot = loadedTabCount.snapshot();
  checkTabCountHistogram(snapshot, { 1: 0, 2: 1, 3: 0 }, "TAB_COUNT - new tab");

  // Open a pending tab, as if by session restore.
  resetTimestamps();
  const lazyTab = BrowserTestUtils.addTab(gBrowser, "about:mozilla", {
    createLazyBrowser: true,
  });
  tabs.push(lazyTab);

  await BrowserTestUtils.waitForCondition(
    () => !ObjectUtils.deepEqual(snapshot, tabCount.snapshot())
  );

  checkTabCountHistogram(
    tabCount.snapshot(),
    { 1: 0, 2: 1, 3: 1, 4: 0 },
    "TAB_COUNT - Added pending tab"
  );

  // Only the mochi.test and foreground tab are loaded.
  checkTabCountHistogram(
    loadedTabCount.snapshot(),
    { 1: 0, 2: 2, 3: 0 },
    "LOADED_TAB_COUNT - Added pending tab"
  );

  resetTimestamps();
  const restoredEvent = BrowserTestUtils.waitForEvent(lazyTab, "SSTabRestored");
  await BrowserTestUtils.switchTab(gBrowser, lazyTab);
  await restoredEvent;

  checkTabCountHistogram(
    tabCount.snapshot(),
    { 1: 0, 2: 1, 3: 1, 4: 0 },
    "TAB_COUNT - Restored pending tab"
  );

  checkTabCountHistogram(
    loadedTabCount.snapshot(),
    { 1: 0, 2: 2, 3: 1, 4: 0 },
    "LOADED_TAB_COUNT - Restored pending tab"
  );

  resetTimestamps();

  await Promise.all([
    BrowserTestUtils.startLoadingURIString(
      lazyTab.linkedBrowser,
      "http://example.com/"
    ),
    BrowserTestUtils.browserLoaded(
      lazyTab.linkedBrowser,
      false,
      "http://example.com/"
    ),
  ]);

  checkTabCountHistogram(
    tabCount.snapshot(),
    { 1: 0, 2: 1, 3: 2, 4: 0 },
    "TAB_COUNT - Navigated in existing tab"
  );

  checkTabCountHistogram(
    loadedTabCount.snapshot(),
    { 1: 0, 2: 2, 3: 2, 4: 0 },
    "LOADED_TAB_COUNT - Navigated in existing tab"
  );

  resetTimestamps();
  let win = await BrowserTestUtils.openNewBrowserWindow();
  await BrowserTestUtils.firstBrowserLoaded(win);

  // The new window will have a new tab.
  checkTabCountHistogram(
    tabCount.snapshot(),
    { 1: 0, 2: 1, 3: 2, 4: 1, 5: 0 },
    "TAB_COUNT - Opened new window"
  );

  checkTabCountHistogram(
    loadedTabCount.snapshot(),
    { 1: 0, 2: 2, 3: 2, 4: 1, 5: 0 },
    "LOADED_TAB_COUNT - Opened new window"
  );

  resetTimestamps();
  await BrowserTestUtils.openNewForegroundTab(win.gBrowser, "about:robots");
  checkTabCountHistogram(
    tabCount.snapshot(),
    { 1: 0, 2: 1, 3: 2, 4: 1, 5: 1, 6: 0 },
    "TAB_COUNT - Opened new tab in new window"
  );

  checkTabCountHistogram(
    loadedTabCount.snapshot(),
    { 1: 0, 2: 2, 3: 2, 4: 1, 5: 1, 6: 0 },
    "LOADED_TAB_COUNT - Opened new tab in new window"
  );

  for (const tab of tabs) {
    BrowserTestUtils.removeTab(tab);
  }

  await BrowserTestUtils.closeWindow(win);
});

add_task(async function test_restored_max_pinned_count() {
  // Following pinned tab testing example from
  // https://searchfox.org/mozilla-central/rev/1843375acbbca68127713e402be222350ac99301/browser/components/sessionstore/test/browser_pinned_tabs.js
  Services.telemetry.clearScalars();
  const { E10SUtils } = ChromeUtils.importESModule(
    "resource://gre/modules/E10SUtils.sys.mjs"
  );
  const BACKUP_STATE = SessionStore.getBrowserState();
  const triggeringPrincipal_base64 = E10SUtils.SERIALIZED_SYSTEMPRINCIPAL;
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.sessionstore.restore_on_demand", true],
      ["browser.sessionstore.restore_tabs_lazily", true],
    ],
  });
  let sessionRestoredPromise = new Promise(resolve => {
    Services.obs.addObserver(resolve, "sessionstore-browser-state-restored");
  });

  info("Set browser state to 1 pinned tab.");
  await SessionStore.setBrowserState(
    JSON.stringify({
      windows: [
        {
          selected: 1,
          tabs: [
            {
              pinned: true,
              entries: [
                { url: "https://example.com", triggeringPrincipal_base64 },
              ],
            },
          ],
        },
      ],
    })
  );

  info("Await `sessionstore-browser-state-restored` promise.");
  await sessionRestoredPromise;

  const scalars = TelemetryTestUtils.getProcessScalars("parent");

  TelemetryTestUtils.assertScalar(
    scalars,
    MAX_TAB_PINNED,
    1,
    "The maximum tabs pinned count must match the expected value."
  );

  gBrowser.unpinTab(gBrowser.selectedTab);

  TelemetryTestUtils.assertScalar(
    scalars,
    MAX_TAB_PINNED,
    1,
    "The maximum tabs pinned count must match the expected value."
  );

  sessionRestoredPromise = new Promise(resolve => {
    Services.obs.addObserver(resolve, "sessionstore-browser-state-restored");
  });
  await SessionStore.setBrowserState(BACKUP_STATE);
  await SpecialPowers.popPrefEnv();
  await sessionRestoredPromise;
});
