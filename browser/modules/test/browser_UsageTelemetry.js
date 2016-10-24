"use strict";

const MAX_CONCURRENT_TABS = "browser.engagement.max_concurrent_tab_count";
const TAB_EVENT_COUNT = "browser.engagement.tab_open_event_count";
const MAX_CONCURRENT_WINDOWS = "browser.engagement.max_concurrent_window_count";
const WINDOW_OPEN_COUNT = "browser.engagement.window_open_event_count";
const TOTAL_URI_COUNT = "browser.engagement.total_uri_count";
const UNIQUE_DOMAINS_COUNT = "browser.engagement.unique_domains_count";
const UNFILTERED_URI_COUNT = "browser.engagement.unfiltered_uri_count";

const TELEMETRY_SUBSESSION_TOPIC = "internal-telemetry-after-subsession-split";

/**
 * Waits for the web progress listener associated with this tab to fire an
 * onLocationChange for a non-error page.
 *
 * @param {xul:browser} browser
 *        A xul:browser.
 *
 * @return {Promise}
 * @resolves When navigating to a non-error page.
 */
function browserLocationChanged(browser) {
  return new Promise(resolve => {
    let wpl = {
      onStateChange() {},
      onSecurityChange() {},
      onStatusChange() {},
      onLocationChange(aWebProgress, aRequest, aURI, aFlags) {
        if (!(aFlags & Ci.nsIWebProgressListener.LOCATION_CHANGE_ERROR_PAGE)) {
          browser.webProgress.removeProgressListener(filter);
          filter.removeProgressListener(wpl);
          resolve();
        }
      },
      QueryInterface: XPCOMUtils.generateQI([
        Ci.nsIWebProgressListener,
        Ci.nsIWebProgressListener2,
      ]),
    };
    const filter = Cc["@mozilla.org/appshell/component/browser-status-filter;1"]
                     .createInstance(Ci.nsIWebProgress);
    filter.addProgressListener(wpl, Ci.nsIWebProgress.NOTIFY_ALL);
    browser.webProgress.addProgressListener(filter, Ci.nsIWebProgress.NOTIFY_ALL);
  });
}

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
let checkScalars = (countsObject) => {
  const scalars =
    Services.telemetry.snapshotScalars(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN);

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
  // This, and all the checks below, also check that initial pages (about:newtab, about:blank, ..)
  // are not counted by the total_uri_count and the unfiltered_uri_count probes.
  checkScalars({maxTabs: expectedMaxTabs, tabOpenCount: expectedTabOpenCount, maxWindows: expectedMaxWins,
                windowsOpenCount: expectedWinOpenCount, totalURIs: 0, domainCount: 0,
                totalUnfilteredURIs: 0});

  // Add two new tabs in the same window.
  openedTabs.push(yield BrowserTestUtils.openNewForegroundTab(gBrowser, "about:blank"));
  openedTabs.push(yield BrowserTestUtils.openNewForegroundTab(gBrowser, "about:blank"));
  expectedTabOpenCount += 2;
  expectedMaxTabs += 2;
  checkScalars({maxTabs: expectedMaxTabs, tabOpenCount: expectedTabOpenCount, maxWindows: expectedMaxWins,
                windowsOpenCount: expectedWinOpenCount, totalURIs: 0, domainCount: 0,
                totalUnfilteredURIs: 0});

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
  checkScalars({maxTabs: expectedMaxTabs, tabOpenCount: expectedTabOpenCount, maxWindows: expectedMaxWins,
                windowsOpenCount: expectedWinOpenCount, totalURIs: 0, domainCount: 0,
                totalUnfilteredURIs: 0});

  // Remove all the extra windows and tabs.
  for (let tab of openedTabs) {
    yield BrowserTestUtils.removeTab(tab);
  }
  yield BrowserTestUtils.closeWindow(win);

  // Make sure all the scalars still have the expected values.
  checkScalars({maxTabs: expectedMaxTabs, tabOpenCount: expectedTabOpenCount, maxWindows: expectedMaxWins,
                windowsOpenCount: expectedWinOpenCount, totalURIs: 0, domainCount: 0,
                totalUnfilteredURIs: 0});
});

add_task(function* test_subsessionSplit() {
  // Let's reset the counts.
  Services.telemetry.clearScalars();

  // Add a new window (that will have 4 tabs).
  let win = yield BrowserTestUtils.openNewBrowserWindow();
  let openedTabs = [];
  openedTabs.push(yield BrowserTestUtils.openNewForegroundTab(win.gBrowser, "about:blank"));
  openedTabs.push(yield BrowserTestUtils.openNewForegroundTab(win.gBrowser, "about:mozilla"));
  openedTabs.push(yield BrowserTestUtils.openNewForegroundTab(win.gBrowser, "http://www.example.com"));

  // Check that the scalars have the right values. We expect 2 unfiltered URI loads
  // (about:mozilla and www.example.com, but no about:blank) and 1 URI totalURIs
  // (only www.example.com).
  checkScalars({maxTabs: 5, tabOpenCount: 4, maxWindows: 2, windowsOpenCount: 1,
                totalURIs: 1, domainCount: 1, totalUnfilteredURIs: 2});

  // Remove a tab.
  yield BrowserTestUtils.removeTab(openedTabs.pop());

  // Simulate a subsession split by clearing the scalars (via |snapshotScalars|) and
  // notifying the subsession split topic.
  Services.telemetry.snapshotScalars(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN,
                                     true /* clearScalars */);
  Services.obs.notifyObservers(null, TELEMETRY_SUBSESSION_TOPIC, "");

  // After a subsession split, only the MAX_CONCURRENT_* scalars must be available
  // and have the correct value. No tabs, windows or URIs were opened so other scalars
  // must not be reported.
  checkScalars({maxTabs: 4, tabOpenCount: 0, maxWindows: 2, windowsOpenCount: 0,
                totalURIs: 0, domainCount: 0, totalUnfilteredURIs: 0});

  // Remove all the extra windows and tabs.
  for (let tab of openedTabs) {
    yield BrowserTestUtils.removeTab(tab);
  }
  yield BrowserTestUtils.closeWindow(win);
});

add_task(function* test_URIAndDomainCounts() {
  // Let's reset the counts.
  Services.telemetry.clearScalars();

  let checkCounts = (countsObject) => {
    // Get a snapshot of the scalars and then clear them.
    const scalars =
      Services.telemetry.snapshotScalars(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN);
    checkScalar(scalars, TOTAL_URI_COUNT, countsObject.totalURIs,
                "The URI scalar must contain the expected value.");
    checkScalar(scalars, UNIQUE_DOMAINS_COUNT, countsObject.domainCount,
                "The unique domains scalar must contain the expected value.");
    checkScalar(scalars, UNFILTERED_URI_COUNT, countsObject.totalUnfilteredURIs,
                "The unfiltered URI scalar must contain the expected value.");
  };

  // Check that about:blank doesn't get counted in the URI total.
  let firstTab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, "about:blank");
  checkCounts({totalURIs: 0, domainCount: 0, totalUnfilteredURIs: 0});

  // Open a different page and check the counts.
  yield BrowserTestUtils.loadURI(firstTab.linkedBrowser, "http://example.com/");
  yield BrowserTestUtils.browserLoaded(firstTab.linkedBrowser);
  checkCounts({totalURIs: 1, domainCount: 1, totalUnfilteredURIs: 1});

  // Activating a different tab must not increase the URI count.
  let secondTab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, "about:blank");
  yield BrowserTestUtils.switchTab(gBrowser, firstTab);
  checkCounts({totalURIs: 1, domainCount: 1, totalUnfilteredURIs: 1});
  yield BrowserTestUtils.removeTab(secondTab);

  // Open a new window and set the tab to a new address.
  let newWin = yield BrowserTestUtils.openNewBrowserWindow();
  yield BrowserTestUtils.loadURI(newWin.gBrowser.selectedBrowser, "http://example.com/");
  yield BrowserTestUtils.browserLoaded(newWin.gBrowser.selectedBrowser);
  checkCounts({totalURIs: 2, domainCount: 1, totalUnfilteredURIs: 2});

  // We should not count AJAX requests.
  const XHR_URL = "http://example.com/r";
  yield ContentTask.spawn(newWin.gBrowser.selectedBrowser, XHR_URL, function(url) {
    return new Promise(resolve => {
      var xhr = new content.window.XMLHttpRequest();
      xhr.open("GET", url);
      xhr.onload = () => resolve();
      xhr.send();
    });
  });
  checkCounts({totalURIs: 2, domainCount: 1, totalUnfilteredURIs: 2});

  // Check that we're counting page fragments.
  let loadingStopped = browserLocationChanged(newWin.gBrowser.selectedBrowser);
  yield BrowserTestUtils.loadURI(newWin.gBrowser.selectedBrowser, "http://example.com/#2");
  yield loadingStopped;
  checkCounts({totalURIs: 3, domainCount: 1, totalUnfilteredURIs: 3});

  // Check that a different URI from the example.com domain doesn't increment the unique count.
  yield BrowserTestUtils.loadURI(newWin.gBrowser.selectedBrowser, "http://test1.example.com/");
  yield BrowserTestUtils.browserLoaded(newWin.gBrowser.selectedBrowser);
  checkCounts({totalURIs: 4, domainCount: 1, totalUnfilteredURIs: 4});

  // Make sure that the unique domains counter is incrementing for a different domain.
  yield BrowserTestUtils.loadURI(newWin.gBrowser.selectedBrowser, "https://example.org/");
  yield BrowserTestUtils.browserLoaded(newWin.gBrowser.selectedBrowser);
  checkCounts({totalURIs: 5, domainCount: 2, totalUnfilteredURIs: 5});

  // Check that we only account for top level loads (e.g. we don't count URIs from
  // embedded iframes).
  yield ContentTask.spawn(newWin.gBrowser.selectedBrowser, null, function* () {
    let doc = content.document;
    let iframe = doc.createElement("iframe");
    let promiseIframeLoaded = ContentTaskUtils.waitForEvent(iframe, "load", false);
    iframe.src = "https://example.org/test";
    doc.body.insertBefore(iframe, doc.body.firstChild);
    yield promiseIframeLoaded;
  });
  checkCounts({totalURIs: 5, domainCount: 2, totalUnfilteredURIs: 5});

  // Check that uncommon protocols get counted in the unfiltered URI probe.
  const TEST_PAGE =
    "data:text/html,<a id='target' href='%23par1'>Click me</a><a name='par1'>The paragraph.</a>";
  yield BrowserTestUtils.loadURI(newWin.gBrowser.selectedBrowser, TEST_PAGE);
  yield BrowserTestUtils.browserLoaded(newWin.gBrowser.selectedBrowser);
  checkCounts({totalURIs: 5, domainCount: 2, totalUnfilteredURIs: 6});

  // Clean up.
  yield BrowserTestUtils.removeTab(firstTab);
  yield BrowserTestUtils.closeWindow(newWin);
});
