/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

PromiseTestUtils.allowMatchingRejectionsGlobally(/Connection closed/);

// Test the debugger when navigating using the BFCache.

add_task(async function() {
  info("Run test with bfcacheInParent DISABLED");
  await pushPref("fission.bfcacheInParent", false);
  await testSourcesOnNavigation();
  await testDebuggerPauseStateOnNavigation();

  // bfcacheInParent only works if sessionHistoryInParent is enable
  // so only test it if both settings are enabled.
  if (Services.appinfo.sessionHistoryInParent) {
    info("Run test with bfcacheInParent ENABLED");
    await pushPref("fission.bfcacheInParent", true);
    await pushPref("dom.security.https_first", false);
    await testSourcesOnNavigation();
    await testDebuggerPauseStateOnNavigation();
  }
});

async function testSourcesOnNavigation() {
  info("Test that sources appear in the debugger when navigating using the BFCache")
  const dbg = await initDebugger("doc-bfcache1.html");

  await navigate(dbg, "doc-bfcache2.html", "doc-bfcache2.html");

  invokeInTab("goBack");
  await waitForSources(dbg, "doc-bfcache1.html");

  invokeInTab("goForward");
  await waitForSources(dbg, "doc-bfcache2.html");
  ok(true, "Found sources after BFCache navigations");

  await dbg.toolbox.closeToolbox();
}

async function testDebuggerPauseStateOnNavigation() {
  info("Test the debugger pause state when navigating using the BFCache");

  if (Services.appinfo.sessionHistoryInParent) {
    enableTargetSwitching();
  }
  const dbg = await initDebugger("doc-bfcache1.html");

  await addBreakpoint(dbg, "doc-bfcache1.html", 4);

  await navigate(dbg, "doc-bfcache2.html");
  await waitForSources(dbg, "doc-bfcache2.html");

  await goBack(EXAMPLE_URL + "doc-bfcache1.html");
  await waitForSources(dbg, "doc-bfcache1.html");

  await reload(dbg);
  await waitForPaused(dbg);

  ok(dbg.toolbox.isHighlighted("jsdebugger"), "Debugger is highlighted");
  
  await goForward(EXAMPLE_URL + "doc-bfcache2.html");

  // This check should be removed when Bug 1722305 is fixed, which would cause will-navigate
  // to start firing properly with fission.
  if (!Services.appinfo.sessionHistoryInParent) {
    await waitUntil(() => !dbg.toolbox.isHighlighted("jsdebugger"));
    ok(true, "Debugger is not highlighted");
  }

  dbg.toolbox.closeToolbox();
}

async function goBack(expectedUrl) {
  const onLocationChange = BrowserTestUtils.waitForLocationChange(
    gBrowser,
    expectedUrl
  );
  gBrowser.goBack();
  await onLocationChange;
}

async function goForward(expectedUrl) {
  const onLocationChange = BrowserTestUtils.waitForLocationChange(
    gBrowser,
    expectedUrl
  );
  gBrowser.goForward();
  await onLocationChange;
}
