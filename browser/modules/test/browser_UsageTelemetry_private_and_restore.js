"use strict";

const {Utils} = Cu.import("resource://gre/modules/sessionstore/Utils.jsm", {});
const triggeringPrincipal_base64 = Utils.SERIALIZED_SYSTEMPRINCIPAL;

const MAX_CONCURRENT_TABS = "browser.engagement.max_concurrent_tab_count";
const TAB_EVENT_COUNT = "browser.engagement.tab_open_event_count";
const MAX_CONCURRENT_WINDOWS = "browser.engagement.max_concurrent_window_count";
const WINDOW_OPEN_COUNT = "browser.engagement.window_open_event_count";
const TOTAL_URI_COUNT = "browser.engagement.total_uri_count";
const UNFILTERED_URI_COUNT = "browser.engagement.unfiltered_uri_count";
const UNIQUE_DOMAINS_COUNT = "browser.engagement.unique_domains_count";

function promiseBrowserStateRestored() {
  return new Promise(resolve => {
     Services.obs.addObserver(function observer(aSubject, aTopic) {
       Services.obs.removeObserver(observer, "sessionstore-browser-state-restored");
       resolve();
     }, "sessionstore-browser-state-restored", false);
  });
}

add_task(function* test_privateMode() {
  // Let's reset the counts.
  Services.telemetry.clearScalars();

  // Open a private window and load a website in it.
  let privateWin = yield BrowserTestUtils.openNewBrowserWindow({private: true});
  yield BrowserTestUtils.loadURI(privateWin.gBrowser.selectedBrowser, "http://example.com/");
  yield BrowserTestUtils.browserLoaded(privateWin.gBrowser.selectedBrowser);

  // Check that tab and window count is recorded.
  const scalars = getParentProcessScalars(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN);

  ok(!(TOTAL_URI_COUNT in scalars), "We should not track URIs in private mode.");
  ok(!(UNFILTERED_URI_COUNT in scalars), "We should not track URIs in private mode.");
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
          { entries: [{ url: "http://example.org", triggeringPrincipal_base64}],
            extData: { "uniq": 3785 } }
        ],
        selected: 1
      }
    ]
  };

  // Save the current session.
  let SessionStore =
    Cu.import("resource:///modules/sessionstore/SessionStore.jsm", {}).SessionStore;

  // Load the custom state and wait for SSTabRestored, as we want to make sure
  // that the URI counting code was hit.
  let tabRestored = BrowserTestUtils.waitForEvent(gBrowser.tabContainer, "SSTabRestored");
  SessionStore.setBrowserState(JSON.stringify(state));
  yield tabRestored;

  // Check that the URI is not recorded.
  const scalars = getParentProcessScalars(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN);

  ok(!(TOTAL_URI_COUNT in scalars), "We should not track URIs from restored sessions.");
  ok(!(UNFILTERED_URI_COUNT in scalars), "We should not track URIs from restored sessions.");
  ok(!(UNIQUE_DOMAINS_COUNT in scalars), "We should not track unique domains from restored sessions.");

  // Restore the original session and cleanup.
  let sessionRestored = promiseBrowserStateRestored();
  SessionStore.setBrowserState(JSON.stringify(state));
  yield sessionRestored;
});
