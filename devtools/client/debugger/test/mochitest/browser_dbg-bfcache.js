/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

PromiseTestUtils.allowMatchingRejectionsGlobally(/Connection closed/);

// Test that sources appear in the debugger when navigating using the BFCache.
add_task(async function() {
  info("Run test with bfcacheInParent DISABLED");
  await pushPref("fission.bfcacheInParent", false);
  await testCase();

  // bfcacheInParent only works if sessionHistoryInParent is enable
  // so only test it if both settings are enabled.
  if (Services.appinfo.sessionHistoryInParent) {
    info("Run test with bfcacheInParent ENABLED");
    await pushPref("fission.bfcacheInParent", true);
    await testCase();
  }
});

async function testCase() {
  const dbg = await initDebugger("doc-bfcache1.html");

  await navigate(dbg, "doc-bfcache2.html", "doc-bfcache2.html");

  invokeInTab("goBack");
  await waitForSources(dbg, "doc-bfcache1.html");

  invokeInTab("goForward");
  await waitForSources(dbg, "doc-bfcache2.html");
  ok(true, "Found sources after BFCache navigations");

  await dbg.toolbox.closeToolbox();
}
