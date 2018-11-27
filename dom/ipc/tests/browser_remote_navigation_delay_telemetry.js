"use strict";

var session = ChromeUtils.import("resource://gre/modules/TelemetrySession.jsm", {});

add_task(async function test_memory_distribution() {
  if (Services.prefs.getIntPref("dom.ipc.processCount", 1) < 2) {
    ok(true, "Skip this test if e10s-multi is disabled.");
    return;
  }

  let canRecordExtended = Services.telemetry.canRecordExtended;
  Services.telemetry.canRecordExtended = true;
  registerCleanupFunction(() => Services.telemetry.canRecordExtended = canRecordExtended);

  Services.telemetry.snapshotKeyedHistograms(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN,
                                             true /* clear */);

  // Open a remote page in a new tab to trigger the WebNavigation:LoadURI.
  let tab1 = await BrowserTestUtils.openNewForegroundTab(gBrowser, "http://example.com");
  ok(tab1.linkedBrowser.isRemoteBrowser, "|tab1| should have a remote browser.");

  // Open a new tab with about:robots, so it ends up in the parent process with a non-remote browser.
  let tab2 = await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:robots");
  ok(!tab2.linkedBrowser.isRemoteBrowser, "|tab2| should have a non-remote browser.");
  // Navigate the tab, so it will change remotness and it triggers the SessionStore:restoreTabContent case.
  await BrowserTestUtils.loadURI(tab2.linkedBrowser, "http://example.com");
  ok(tab2.linkedBrowser.isRemoteBrowser, "|tab2| should have a remote browser by now.");

  // There is no good way to make sure that the parent received the histogram entries from the child processes.
  // Let's stick to the ugly, spinning the event loop until we have a good approach (Bug 1357509).
  await BrowserTestUtils.waitForCondition(() => {
    let s = Services.telemetry.snapshotKeyedHistograms(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN,
                                                       false).content["FX_TAB_REMOTE_NAVIGATION_DELAY_MS"];
    return s && "WebNavigation:LoadURI" in s && "SessionStore:restoreTabContent" in s;
  });

  let s = Services.telemetry.snapshotKeyedHistograms(1, false).content["FX_TAB_REMOTE_NAVIGATION_DELAY_MS"];
  let restoreTabSnapshot = s["SessionStore:restoreTabContent"];
  ok(restoreTabSnapshot.sum > 0, "Zero delay for the restoreTabContent case is unlikely.");
  ok(restoreTabSnapshot.sum < 10000, "More than 10 seconds delay for the restoreTabContent case is unlikely.");

  let loadURISnapshot = s["WebNavigation:LoadURI"];
  ok(loadURISnapshot.sum > 0, "Zero delay for the LoadURI case is unlikely.");
  ok(loadURISnapshot.sum < 10000, "More than 10 seconds delay for the LoadURI case is unlikely.");

  Services.telemetry.snapshotKeyedHistograms(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN,
                                             true /* clear */);

  BrowserTestUtils.removeTab(tab2);
  BrowserTestUtils.removeTab(tab1);
});
