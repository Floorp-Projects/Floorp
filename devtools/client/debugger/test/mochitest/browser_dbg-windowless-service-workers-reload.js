/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Test that service worker sources are still displayed after reloading the page
// and that we can hit breakpoints in them.
add_task(async function() {
  await pushPref("devtools.debugger.features.windowless-service-workers", true);
  await pushPref("devtools.debugger.workers-visible", true);
  await pushPref("dom.serviceWorkers.enabled", true);
  await pushPref("dom.serviceWorkers.testing.enabled", true);
  const dbg = await initDebugger("doc-service-workers.html");

  invokeInTab("registerWorker");
  await waitForSource(dbg, "service-worker.sjs");
  const workerSource = findSource(dbg, "service-worker.sjs");

  await reload(dbg, "service-worker.sjs");

  await addBreakpoint(dbg, "service-worker.sjs", 13);
  invokeInTab("fetchFromWorker");

  await waitForPaused(dbg);
  assertPausedAtSourceAndLine(dbg, workerSource.id, 13);

  await resume(dbg);
  await waitForRequestsToSettle(dbg);
  await removeTab(gBrowser.selectedTab);
});
