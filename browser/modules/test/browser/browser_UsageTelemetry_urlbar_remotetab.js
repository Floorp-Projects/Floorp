/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/**
 * This file tests urlbar telemetry with remote tab action.
 */

"use strict";

const SCALAR_URLBAR = "browser.engagement.navigation.urlbar";

XPCOMUtils.defineLazyModuleGetters(this, {
  SyncedTabs: "resource://services-sync/SyncedTabs.jsm",
  UrlbarTestUtils: "resource://testing-common/UrlbarTestUtils.jsm",
  URLBAR_SELECTED_RESULT_TYPES: "resource:///modules/BrowserUsageTelemetry.jsm",
  URLBAR_SELECTED_RESULT_METHODS: "resource:///modules/BrowserUsageTelemetry.jsm",
});
const {sinon} = ChromeUtils.import("resource://testing-common/Sinon.jsm");

function assertSearchTelemetryEmpty(search_hist) {
  const scalars = TelemetryTestUtils.getProcessScalars("parent", true, false);
  Assert.ok(!(SCALAR_URLBAR in scalars), `Should not have recorded ${SCALAR_URLBAR}`);

  // Make sure SEARCH_COUNTS contains identical values.
  TelemetryTestUtils.assertKeyedHistogramSum(search_hist, "other-MozSearch.urlbar", undefined);
  TelemetryTestUtils.assertKeyedHistogramSum(search_hist, "other-MozSearch.alias", undefined);

  // Also check events.
  let events = Services.telemetry.snapshotEvents(Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS, false);
  events = (events.parent || []).filter(e => e[1] == "navigation" && e[2] == "search");
  Assert.deepEqual(events, [], "Should not have recorded any navigation search events");
}

function snapshotHistograms() {
  Services.telemetry.clearScalars();
  Services.telemetry.clearEvents();
  return {
    resultIndexHist: TelemetryTestUtils.getAndClearHistogram("FX_URLBAR_SELECTED_RESULT_INDEX"),
    resultTypeHist: TelemetryTestUtils.getAndClearHistogram("FX_URLBAR_SELECTED_RESULT_TYPE"),
    resultIndexByTypeHist: TelemetryTestUtils.getAndClearKeyedHistogram("FX_URLBAR_SELECTED_RESULT_INDEX_BY_TYPE"),
    resultMethodHist: TelemetryTestUtils.getAndClearHistogram("FX_URLBAR_SELECTED_RESULT_METHOD"),
    search_hist: TelemetryTestUtils.getAndClearKeyedHistogram("SEARCH_COUNTS"),
  };
}

function assertHistogramResults(histograms, type, index, method) {
  TelemetryTestUtils.assertHistogram(histograms.resultIndexHist, index, 1);

  TelemetryTestUtils.assertHistogram(histograms.resultTypeHist,
    URLBAR_SELECTED_RESULT_TYPES[type], 1);

  TelemetryTestUtils.assertKeyedHistogramValue(histograms.resultIndexByTypeHist,
    type, index, 1);

  TelemetryTestUtils.assertHistogram(histograms.resultMethodHist,
    method, 1);
}


add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [
      // Disable search suggestions in the urlbar.
      ["browser.urlbar.suggest.searches", false],
      // Clear historical search suggestions to avoid interference from previous
      // tests.
      ["browser.urlbar.maxHistoricalSearchSuggestions", 0],
      // Use the default matching bucket configuration.
      ["browser.urlbar.matchBuckets", "general:5,suggestion:4"],
      // Turn autofill off.
      ["browser.urlbar.autoFill", false],
      // Special prefs for remote tabs.
      ["services.sync.username", "fake"],
      ["services.sync.syncedTabs.showRemoteTabs", true],
    ],
  });

  // Enable local telemetry recording for the duration of the tests.
  let oldCanRecord = Services.telemetry.canRecordExtended;
  Services.telemetry.canRecordExtended = true;

  // Enable event recording for the events tested here.
  Services.telemetry.setEventRecordingEnabled("navigation", true);

  // Clear history so that history added by previous tests doesn't mess up this
  // test when it selects results in the urlbar.
  await PlacesUtils.history.clear();
  await PlacesUtils.bookmarks.eraseEverything();

  const REMOTE_TAB = {
    "id": "7cqCr77ptzX3",
    "type": "client",
    "lastModified": 1492201200,
    "name": "zcarter's Nightly on MacBook-Pro-25",
    "clientType": "desktop",
    "tabs": [
      {
        "type": "tab",
        "title": "Test Remote",
        "url": "http://example.com",
        "icon": UrlbarUtils.ICON.DEFAULT,
        "client": "7cqCr77ptzX3",
        "lastUsed": 1452124677,
      },
    ],
  };

  const sandbox = sinon.createSandbox();

  let originalSyncedTabsInternal = SyncedTabs._internal;
  SyncedTabs._internal = {
    isConfiguredToSyncTabs: true,
    hasSyncedThisSession: true,
    getTabClients() { return Promise.resolve([]); },
    syncTabs() { return Promise.resolve(); },
  };

  // Tell the Sync XPCOM service it is initialized.
  let weaveXPCService = Cc["@mozilla.org/weave/service;1"]
                          .getService(Ci.nsISupports)
                          .wrappedJSObject;
  let oldWeaveServiceReady = weaveXPCService.ready;
  weaveXPCService.ready = true;

  sandbox.stub(SyncedTabs._internal, "getTabClients")
         .callsFake(() => Promise.resolve(Cu.cloneInto([REMOTE_TAB], {})));

  // Make sure to restore the engine once we're done.
  registerCleanupFunction(async function() {
    sandbox.restore();
    weaveXPCService.ready = oldWeaveServiceReady;
    SyncedTabs._internal = originalSyncedTabsInternal;
    Services.telemetry.canRecordExtended = oldCanRecord;
    await PlacesUtils.history.clear();
    await PlacesUtils.bookmarks.eraseEverything();
    Services.telemetry.setEventRecordingEnabled("navigation", false);
  });
});

add_task(async function test_remotetab() {
  const histograms = snapshotHistograms();

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:blank");

  let p = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    waitForFocus,
    value: "example",
    fireInputEvent: true,
  });
  EventUtils.synthesizeKey("KEY_ArrowDown");
  EventUtils.synthesizeKey("KEY_Enter");
  await p;

  assertSearchTelemetryEmpty(histograms.search_hist);
  assertHistogramResults(histograms, "remotetab", 1,
    URLBAR_SELECTED_RESULT_METHODS.arrowEnterSelection);

  BrowserTestUtils.removeTab(tab);
});
