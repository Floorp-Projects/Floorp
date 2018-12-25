/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test basic windowless worker functionality: the main thread and worker can be
// separately controlled from the same debugger.
add_task(async function() {
  await pushPref("devtools.debugger.features.windowless-workers", true);

  const dbg = await initDebugger("doc-windowless-workers.html");
  const mainThread = dbg.toolbox.threadClient.actor;

  const workers = await getWorkers(dbg);
  ok(workers.length == 1, "Got one worker");
  const workerThread = workers[0].actor;

  const mainThreadSource = findSource(dbg, "doc-windowless-workers.html");
  const workerSource = findSource(dbg, "simple-worker.js");

  assertNotPaused(dbg);

  await dbg.actions.breakOnNext();
  await waitForPaused(dbg, "doc-windowless-workers.html");

  // We should be paused at the timer in doc-windowless-workers.html
  assertPausedAtSourceAndLine(dbg, mainThreadSource.id, 9);

  await dbg.actions.selectThread(workerThread);
  assertNotPaused(dbg);

  await dbg.actions.breakOnNext();
  await waitForPaused(dbg, "simple-worker.js");

  // We should be paused at the timer in simple-worker.js
  assertPausedAtSourceAndLine(dbg, workerSource.id, 3);

  await stepOver(dbg);
  assertPausedAtSourceAndLine(dbg, workerSource.id, 4);

  await dbg.actions.selectThread(mainThread);

  await stepOver(dbg);
  assertPausedAtSourceAndLine(dbg, mainThreadSource.id, 10);
});
