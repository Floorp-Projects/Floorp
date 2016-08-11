"use strict";

const MAX_CONCURRENT_TABS = "browser.engagement.max_concurrent_tab_count";
const TAB_EVENT_COUNT = "browser.engagement.tab_open_event_count";
const MAX_CONCURRENT_WINDOWS = "browser.engagement.max_concurrent_window_count";
const WINDOW_OPEN_COUNT = "browser.engagement.window_open_event_count";
const TOTAL_URI_COUNT = "browser.engagement.total_uri_count";
const UNIQUE_DOMAINS_COUNT = "browser.engagement.unique_domains_count";

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

function promiseBrowserStateRestored() {
  return new Promise(resolve => {
     Services.obs.addObserver(function observer(aSubject, aTopic) {
       Services.obs.removeObserver(observer, "sessionstore-browser-state-restored");
       resolve();
     }, "sessionstore-browser-state-restored", false);
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
let checkScalars = (maxTabs, tabOpenCount, maxWindows, windowsOpenCount, totalURIs, domainCount) => {
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
  checkScalar(scalars, TOTAL_URI_COUNT, totalURIs,
              "The total URI count must match the expected value.");
  checkScalar(scalars, UNIQUE_DOMAINS_COUNT, domainCount,
              "The unique domains count must match the expected value.");
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
  checkScalars(expectedMaxTabs, expectedTabOpenCount, expectedMaxWins, expectedWinOpenCount, 0, 0);

  // Add two new tabs in the same window.
  openedTabs.push(yield BrowserTestUtils.openNewForegroundTab(gBrowser, "about:blank"));
  openedTabs.push(yield BrowserTestUtils.openNewForegroundTab(gBrowser, "about:blank"));
  expectedTabOpenCount += 2;
  expectedMaxTabs += 2;
  checkScalars(expectedMaxTabs, expectedTabOpenCount, expectedMaxWins, expectedWinOpenCount, 0, 0);

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
  checkScalars(expectedMaxTabs, expectedTabOpenCount, expectedMaxWins, expectedWinOpenCount, 0, 0);

  // Remove all the extra windows and tabs.
  for (let tab of openedTabs) {
    yield BrowserTestUtils.removeTab(tab);
  }
  yield BrowserTestUtils.closeWindow(win);

  // Make sure all the scalars still have the expected values.
  checkScalars(expectedMaxTabs, expectedTabOpenCount, expectedMaxWins, expectedWinOpenCount, 0, 0);
});

add_task(function* test_subsessionSplit() {
  // Let's reset the counts.
  Services.telemetry.clearScalars();

  // Add a new window (that will have 4 tabs).
  let win = yield BrowserTestUtils.openNewBrowserWindow();
  let openedTabs = [];
  openedTabs.push(yield BrowserTestUtils.openNewForegroundTab(win.gBrowser, "about:blank"));
  openedTabs.push(yield BrowserTestUtils.openNewForegroundTab(win.gBrowser, "about:blank"));
  openedTabs.push(yield BrowserTestUtils.openNewForegroundTab(win.gBrowser, "http://www.example.com"));

  // Check that the scalars have the right values.
  checkScalars(5 /*maxTabs*/, 4 /*tabOpen*/, 2 /*maxWins*/, 1 /*winOpen*/,
               1 /* toalURIs */, 1 /* uniqueDomains */);

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
  checkScalars(4 /*maxTabs*/, 0 /*tabOpen*/, 2 /*maxWins*/, 0 /*winOpen*/,
               0 /* toalURIs */, 0 /* uniqueDomains */);

  // Remove all the extra windows and tabs.
  for (let tab of openedTabs) {
    yield BrowserTestUtils.removeTab(tab);
  }
  yield BrowserTestUtils.closeWindow(win);
});

add_task(function* test_URIAndDomainCounts() {
  // Let's reset the counts.
  Services.telemetry.clearScalars();

  let checkCounts = (URICount, domainCount) => {
    // Get a snapshot of the scalars and then clear them.
    const scalars =
      Services.telemetry.snapshotScalars(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN);
    checkScalar(scalars, TOTAL_URI_COUNT, URICount,
                "The URI scalar must contain the expected value.");
    checkScalar(scalars, UNIQUE_DOMAINS_COUNT, domainCount,
                "The unique domains scalar must contain the expected value.");
  };

  // Check that about:blank doesn't get counted in the URI total.
  let firstTab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, "about:blank");
  checkCounts(0, 0);

  // Open a different page and check the counts.
  yield BrowserTestUtils.loadURI(firstTab.linkedBrowser, "http://example.com/");
  yield BrowserTestUtils.browserLoaded(firstTab.linkedBrowser);
  checkCounts(1, 1);

  // Activating a different tab must not increase the URI count.
  let secondTab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, "about:blank");
  yield BrowserTestUtils.switchTab(gBrowser, firstTab);
  checkCounts(1, 1);
  yield BrowserTestUtils.removeTab(secondTab);

  // Open a new window and set the tab to a new address.
  let newWin = yield BrowserTestUtils.openNewBrowserWindow();
  yield BrowserTestUtils.loadURI(newWin.gBrowser.selectedBrowser, "http://example.com/");
  yield BrowserTestUtils.browserLoaded(newWin.gBrowser.selectedBrowser);
  checkCounts(2, 1);

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
  checkCounts(2, 1);

  // Check that we're counting page fragments.
  let loadingStopped = browserLocationChanged(newWin.gBrowser.selectedBrowser);
  yield BrowserTestUtils.loadURI(newWin.gBrowser.selectedBrowser, "http://example.com/#2");
  yield loadingStopped;
  checkCounts(3, 1);

  // Check test.domain.com and some.domain.com are only counted once unique.
  yield BrowserTestUtils.loadURI(newWin.gBrowser.selectedBrowser, "http://test1.example.com/");
  yield BrowserTestUtils.browserLoaded(newWin.gBrowser.selectedBrowser);
  checkCounts(4, 1);

  // Make sure that the unique domains counter is incrementing for a different domain.
  yield BrowserTestUtils.loadURI(newWin.gBrowser.selectedBrowser, "https://example.org/");
  yield BrowserTestUtils.browserLoaded(newWin.gBrowser.selectedBrowser);
  checkCounts(5, 2);

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
  checkCounts(5, 2);

  // Clean up.
  yield BrowserTestUtils.removeTab(firstTab);
  yield BrowserTestUtils.closeWindow(newWin);
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

  ok(!(TOTAL_URI_COUNT in scalars), "We should not track URIs in private mode.");
  ok(!(UNIQUE_DOMAINS_COUNT in scalars), "We should not track unique domains in private mode.");
  is(scalars[TAB_EVENT_COUNT], 1, "The number of open tab event count must match the expected value.");
  is(scalars[MAX_CONCURRENT_TABS], 2, "The maximum tab count must match the expected value.");
  is(scalars[WINDOW_OPEN_COUNT], 1, "The number of window open event count must match the expected value.");
  is(scalars[MAX_CONCURRENT_WINDOWS], 2, "The maximum window count must match the expected value.");

  // Clean up.
  yield BrowserTestUtils.closeWindow(privateWin);
});

add_task(function* test_sessionRestore() {
  const PREF_RESTORE_ON_DEMAND = "browser.sessionstore.restore_on_demand";
  Services.prefs.setBoolPref(PREF_RESTORE_ON_DEMAND, false);
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref(PREF_RESTORE_ON_DEMAND);
  });

  // Let's reset the counts.
  Services.telemetry.clearScalars();

  // The first window will be put into the already open window and the second
  // window will be opened with _openWindowWithState, which is the source of the problem.
  const state = {
    windows: [
      {
        tabs: [
          { entries: [{ url: "http://example.org" }], extData: { "uniq": 3785 } }
        ],
        selected: 1
      }
    ]
  };

  // Save the current session.
  let SessionStore =
    Cu.import("resource:///modules/sessionstore/SessionStore.jsm", {}).SessionStore;
  let backupState = SessionStore.getBrowserState();

  // Load the custom state and wait for SSTabRestored, as we want to make sure
  // that the URI counting code was hit.
  let tabRestored = BrowserTestUtils.waitForEvent(gBrowser.tabContainer, "SSTabRestored");
  SessionStore.setBrowserState(JSON.stringify(state));
  yield tabRestored;

  // Check that the URI is not recorded.
  const scalars =
    Services.telemetry.snapshotScalars(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN);

  ok(!(TOTAL_URI_COUNT in scalars), "We should not track URIs from restored sessions.");
  ok(!(UNIQUE_DOMAINS_COUNT in scalars), "We should not track unique domains from restored sessions.");

  // Restore the original session and cleanup.
  let sessionRestored = promiseBrowserStateRestored();
  SessionStore.setBrowserState(JSON.stringify(state));
  yield sessionRestored;
});
