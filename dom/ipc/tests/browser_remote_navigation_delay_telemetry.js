"use strict";

var session = Cu.import("resource://gre/modules/TelemetrySession.jsm", {});

add_task(function* test_memory_distribution() {
  if (Services.prefs.getIntPref("dom.ipc.processCount", 1) < 2) {
    ok(true, "Skip this test if e10s-multi is disabled.");
    return;
  }

  yield SpecialPowers.pushPrefEnv({set: [["toolkit.telemetry.enabled", true]]});
  let canRecordExtended = Services.telemetry.canRecordExtended;
  Services.telemetry.canRecordExtended = true;
  registerCleanupFunction(() => Services.telemetry.canRecordExtended = canRecordExtended);

  // Note the #content suffix after the id. This is the only way this API lets us fetch the
  // histogram entries reported by a content process.
  let histogram = Services.telemetry.getKeyedHistogramById("FX_TAB_REMOTE_NAVIGATION_DELAY_MS#content");
  histogram.clear();

  // Open a remote page in a new tab to trigger the WebNavigation:LoadURI.
  let tab1 = yield BrowserTestUtils.openNewForegroundTab(gBrowser, "http://example.com");
  ok(tab1.linkedBrowser.isRemoteBrowser, "|tab1| should have a remote browser.");

  // Open a new tab with about:robots, so it ends up in the parent process with a non-remote browser.
  let tab2 = yield BrowserTestUtils.openNewForegroundTab(gBrowser, "about:robots");
  ok(!tab2.linkedBrowser.isRemoteBrowser, "|tab2| should have a non-remote browser.");
  // Navigate the tab, so it will change remotness and it triggers the SessionStore:restoreTabContent case.
  yield BrowserTestUtils.loadURI(tab2.linkedBrowser, "http://example.com");
  ok(tab2.linkedBrowser.isRemoteBrowser, "|tab2| should have a remote browser by now.");

  // There is no good way to make sure that the parent received the histogram entries from the child processes.
  // Let's stick to the ugly, spinning the event loop until we have a good approach (Bug 1357509).
  yield BrowserTestUtils.waitForCondition(() => {
    let s = histogram.snapshot();
    return "WebNavigation:LoadURI" in s && "SessionStore:restoreTabContent" in s;
  });

  let s = histogram.snapshot();
  let restoreTabSnapshot = s["SessionStore:restoreTabContent"];
  ok(restoreTabSnapshot.sum > 0, "Zero delay for the restoreTabContent case is unlikely.");
  ok(restoreTabSnapshot.sum < 10000, "More than 10 seconds delay for the restoreTabContent case is unlikely.");

  let loadURISnapshot = s["WebNavigation:LoadURI"];
  ok(loadURISnapshot.sum > 0, "Zero delay for the LoadURI case is unlikely.");
  ok(loadURISnapshot.sum < 10000, "More than 10 seconds delay for the LoadURI case is unlikely.");

  histogram.clear();

  yield BrowserTestUtils.removeTab(tab2);
  yield BrowserTestUtils.removeTab(tab1);
});
