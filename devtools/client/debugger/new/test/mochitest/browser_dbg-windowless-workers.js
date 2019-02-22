/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */


function threadIsPaused(dbg, index) {
  return findElement(dbg, "threadsPaneItem", index).querySelector(
    ".pause-badge"
  );
}

function threadIsSelected(dbg, index) {
  return findElement(dbg, "threadsPaneItem", index).classList.contains(
    "selected"
  );
}

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

  is(
    findElement(dbg, "threadSourceTreeHeader", 2).innerText,
    "simple-worker.js",
    "simple-worker.js - worker is shown in the source tree"
  );

  clickElement(dbg, "threadSourceTreeSourceNode", 2, 2);

  await waitForElement(dbg, "threadSourceTreeSourceNode", 2, 3);

  is(
    findElement(dbg, "threadSourceTreeSourceNode", 2, 3).innerText,
    "simple-worker.js",
    "simple-worker.js - source is shown in the source tree"
  );

  is(
    findElement(dbg, "threadSourceTreeSourceNode", 2, 3).innerText,
    "simple-worker.js",
    "simple-worker.js - source is shown in the source tree"
  );
  // Threads Pane
  is(
    findElement(dbg, "threadsPaneItem", 1).innerText,
    "Main Thread",
    "Main Thread is shown in the threads pane"
  );

  is(
    findElement(dbg, "threadsPaneItem", 2).innerText,
    "simple-worker.js",
    "simple-worker.js is shown in the threads pane"
  );

  ok(threadIsSelected(dbg, 1), "main thread is selected");
  ok(threadIsPaused(dbg, 1), "main thread is paused");
  ok(threadIsPaused(dbg, 2), "simple-worker.js is paused");
  is(findAllElements(dbg, "frames").length, 1, "Main Thread has one frame");

  clickElement(dbg, "threadsPaneItem", 2);
  await waitFor(() => threadIsSelected(dbg, 2));
  is(findAllElements(dbg, "frames").length, 1, "simple worker has one frame");
});
