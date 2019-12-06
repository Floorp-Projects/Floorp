/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

async function checkWorkerThreads(dbg, count) {
  await waitUntil(() => dbg.selectors.getThreads().length == count);
  ok(true, `Have ${count} threads`);
}

// Test that we can detect a new service worker and hit breakpoints that we've
// set in it.
add_task(async function() {
  await pushPref("devtools.debugger.features.windowless-service-workers", true);
  await pushPref("devtools.debugger.workers-visible", true);
  await pushPref("dom.serviceWorkers.enabled", true);
  await pushPref("dom.serviceWorkers.testing.enabled", true);

  let dbg = await initDebugger("doc-service-workers.html");

  invokeInTab("registerWorker");

  await waitForSource(dbg, "service-worker.sjs");
  const workerSource = findSource(dbg, "service-worker.sjs");

  await addBreakpoint(dbg, "service-worker.sjs", 13);

  invokeInTab("fetchFromWorker");

  await waitForPaused(dbg);
  assertPausedAtSourceAndLine(dbg, workerSource.id, 13);

  // Leave the breakpoint in place for the next subtest.
  await resume(dbg);
  await removeTab(gBrowser.selectedTab);
});

// Test that breakpoints can be immediately hit in service workers when reloading.
add_task(async function() {
  const toolbox = await openNewTabAndToolbox(EXAMPLE_URL + "doc-service-workers.html", "jsdebugger");
  const dbg = createDebuggerContext(toolbox);

  await checkWorkerThreads(dbg, 1);

  // The test page will immediately fetch from the service worker if registered.
  await reload(dbg);

  await waitForSource(dbg, "service-worker.sjs");
  const workerSource = findSource(dbg, "service-worker.sjs");

  await waitForPaused(dbg);
  assertPausedAtSourceAndLine(dbg, workerSource.id, 13);
  await checkWorkerThreads(dbg, 1);

  await resume(dbg);
  await dbg.actions.removeAllBreakpoints(getContext(dbg));

  invokeInTab("unregisterWorker");

  await checkWorkerThreads(dbg, 0);
  await removeTab(gBrowser.selectedTab);
});
