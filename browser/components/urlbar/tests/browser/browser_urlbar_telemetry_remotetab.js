/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/**
 * This file tests urlbar telemetry with remote tab action.
 */

"use strict";

const SCALAR_URLBAR = "browser.engagement.navigation.urlbar";

ChromeUtils.defineESModuleGetters(this, {
  UrlbarTestUtils: "resource://testing-common/UrlbarTestUtils.sys.mjs",
});

XPCOMUtils.defineLazyModuleGetters(this, {
  SyncedTabs: "resource://services-sync/SyncedTabs.jsm",
});

function assertSearchTelemetryEmpty(search_hist) {
  const scalars = TelemetryTestUtils.getProcessScalars("parent", true, false);
  Assert.ok(
    !(SCALAR_URLBAR in scalars),
    `Should not have recorded ${SCALAR_URLBAR}`
  );

  // Make sure SEARCH_COUNTS contains identical values.
  TelemetryTestUtils.assertKeyedHistogramSum(
    search_hist,
    "other-MozSearch.urlbar",
    undefined
  );
  TelemetryTestUtils.assertKeyedHistogramSum(
    search_hist,
    "other-MozSearch.alias",
    undefined
  );

  // Also check events.
  let events = Services.telemetry.snapshotEvents(
    Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS,
    false
  );
  events = (events.parent || []).filter(
    e => e[1] == "navigation" && e[2] == "search"
  );
  Assert.deepEqual(
    events,
    [],
    "Should not have recorded any navigation search events"
  );
}

function snapshotHistograms() {
  Services.telemetry.clearScalars();
  Services.telemetry.clearEvents();
  return {
    resultMethodHist: TelemetryTestUtils.getAndClearHistogram(
      "FX_URLBAR_SELECTED_RESULT_METHOD"
    ),
    search_hist: TelemetryTestUtils.getAndClearKeyedHistogram("SEARCH_COUNTS"),
  };
}

function assertTelemetryResults(histograms, type, index, method) {
  TelemetryTestUtils.assertHistogram(histograms.resultMethodHist, method, 1);

  TelemetryTestUtils.assertKeyedScalar(
    TelemetryTestUtils.getProcessScalars("parent", true, true),
    `urlbar.picked.${type}`,
    index,
    1
  );
}

add_setup(async function() {
  await SpecialPowers.pushPrefEnv({
    set: [
      // Disable search suggestions in the urlbar.
      ["browser.urlbar.suggest.searches", false],
      // Clear historical search suggestions to avoid interference from previous
      // tests.
      ["browser.urlbar.maxHistoricalSearchSuggestions", 0],
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
    id: "7cqCr77ptzX3",
    type: "client",
    lastModified: 1492201200,
    name: "zcarter's Nightly on MacBook-Pro-25",
    clientType: "desktop",
    tabs: [
      {
        type: "tab",
        title: "Test Remote",
        url: "http://example.com",
        icon: UrlbarUtils.ICON.DEFAULT,
        client: "7cqCr77ptzX3",
        lastUsed: Math.floor(Date.now() / 1000),
      },
    ],
  };

  const sandbox = sinon.createSandbox();

  let originalSyncedTabsInternal = SyncedTabs._internal;
  SyncedTabs._internal = {
    isConfiguredToSyncTabs: true,
    hasSyncedThisSession: true,
    getTabClients() {
      return Promise.resolve([]);
    },
    syncTabs() {
      return Promise.resolve();
    },
  };

  // Tell the Sync XPCOM service it is initialized.
  let weaveXPCService = Cc["@mozilla.org/weave/service;1"].getService(
    Ci.nsISupports
  ).wrappedJSObject;
  let oldWeaveServiceReady = weaveXPCService.ready;
  weaveXPCService.ready = true;

  sandbox
    .stub(SyncedTabs._internal, "getTabClients")
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

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:blank"
  );

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
  assertTelemetryResults(
    histograms,
    "remotetab",
    1,
    UrlbarTestUtils.SELECTED_RESULT_METHODS.arrowEnterSelection
  );

  BrowserTestUtils.removeTab(tab);
});
