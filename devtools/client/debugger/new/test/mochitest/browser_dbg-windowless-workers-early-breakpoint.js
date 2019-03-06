/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that breakpoints at worker startup are hit when using windowless workers.
add_task(async function() {
  const dbg = await initDebugger("doc-windowless-workers-early-breakpoint.html", "simple-worker.js");

  const workerSource = findSource(dbg, "simple-worker.js");

  // NOTE: by default we do not wait on worker
  // commands to complete because the thread could be
  // shutting down.
  dbg.client.waitForWorkers(true);

  await selectSource(dbg, "simple-worker.js");
  await addBreakpoint(dbg, workerSource, 1);
  invokeInTab("startWorker");
  await waitForPaused(dbg, "simple-worker.js");

  // We should be paused at the first line of simple-worker.js
  assertPausedAtSourceAndLine(dbg, workerSource.id, 1);
  await removeBreakpoint(dbg, workerSource.id, 1, 12);
  await resume(dbg);

  // Make sure that suspending activity in the worker when attaching does not
  // interfere with sending messages to the worker.
  await addBreakpoint(dbg, workerSource, 10);
  invokeInTab("startWorkerWithMessage");
  await waitForPaused(dbg, "simple-worker.js");

  // We should be paused in the message listener in simple-worker.js
  assertPausedAtSourceAndLine(dbg, workerSource.id, 10);
  await removeBreakpoint(dbg, workerSource.id, 10, 2);
});
