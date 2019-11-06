/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Test that we can detect a new service worker and hit breakpoints that we've
// set in it.
add_task(async function() {
  await pushPref("devtools.debugger.features.windowless-service-workers", true);
  await pushPref("devtools.debugger.workers-visible", true);
  await pushPref("dom.serviceWorkers.enabled", true);
  await pushPref("dom.serviceWorkers.testing.enabled", true);

  let dbg = await initDebugger("doc-service-workers.html");

  invokeInTab("registerWorker");

  await waitForSource(dbg, "service-worker.js");
  const workerSource = findSource(dbg, "service-worker.js");

  await addBreakpoint(dbg, "service-worker.js", 14);

  invokeInTab("fetchFromWorker");

  await waitForPaused(dbg);
  assertPausedAtSourceAndLine(dbg, workerSource.id, 14);

  await dbg.actions.removeAllBreakpoints(getContext(dbg));

  await resume(dbg);

  // Leave the worker registration in place for the next task.

  // Set startup breakpoint for the next test.
  //await addBreakpoint(dbg, "service-worker.js", 6);
});

async function checkWorkerThreads(dbg, count) {
  await waitUntil(() => dbg.selectors.getThreads().length == count);
  ok(true, `Have ${count} threads`);
}

// Test that service workers remain after reloading.
add_task(async function() {
  const toolbox = await openNewTabAndToolbox(EXAMPLE_URL + "doc-service-workers.html", "jsdebugger");
  const dbg = createDebuggerContext(toolbox);

  await checkWorkerThreads(dbg, 1);

  await reload(dbg);

  await waitForSource(dbg, "service-worker.js");
  const workerSource = findSource(dbg, "service-worker.js");

  await addBreakpoint(dbg, "service-worker.js", 14);

  invokeInTab("fetchFromWorker");

  await waitForPaused(dbg);
  assertPausedAtSourceAndLine(dbg, workerSource.id, 14);
  await checkWorkerThreads(dbg, 1);

  await resume(dbg);

  invokeInTab("unregisterWorker");

  await dbg.actions.removeAllBreakpoints(getContext(dbg));
  await checkWorkerThreads(dbg, 0);
});
