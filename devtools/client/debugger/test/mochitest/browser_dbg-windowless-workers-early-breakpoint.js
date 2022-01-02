/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Test that breakpoints at worker startup are hit when using windowless workers.
add_task(async function() {
  const dbg = await initDebugger("doc-windowless-workers-early-breakpoint.html", "simple-worker.js");

  const workerSource = findSource(dbg, "simple-worker.js");

  await selectSource(dbg, "simple-worker.js");
  await waitForSelectedSource(dbg, "simple-worker.js");
  await addBreakpoint(dbg, workerSource, 1);
  invokeInTab("startWorker");
  await waitForPaused(dbg, "simple-worker.js");

  // We should be paused at the first line of simple-worker.js
  // Each worker has its own sources, so we have to retrieve the new source,
  // which has just been opened on pause
  const workerSource2 = dbg.selectors.getSelectedSourceWithContent();
  assertPausedAtSourceAndLine(dbg, workerSource2.id, 1);
  // We have to remove the first breakpoint, set on the fist worker.
  // The first worker is loaded on the html page load.
  await removeBreakpoint(dbg, workerSource.id, 1, 12);
  // And also remove the second breakpoint automatically created for the second worker
  // The second worker is created when calling `startWorker`
  await removeBreakpoint(dbg, workerSource2.id, 1, 12);
  await resume(dbg);

  // Make sure that suspending activity in the worker when attaching does not
  // interfere with sending messages to the worker.
  await addBreakpoint(dbg, workerSource2, 10);
  invokeInTab("startWorkerWithMessage");
  await waitForPaused(dbg, "simple-worker.js");

  // We should be paused in the message listener in simple-worker.js
  const workerSource3 = dbg.selectors.getSelectedSourceWithContent();
  assertPausedAtSourceAndLine(dbg, workerSource3.id, 10);
  await removeBreakpoint(dbg, workerSource2.id, 10, 2);
});
