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

function getLabel(dbg, index) {
  return findElement(dbg, "expressionNode", index).innerText;
}

function getValue(dbg, index) {
  return findElement(dbg, "expressionValue", index).innerText;
}

// Test basic windowless worker functionality: the main thread and worker can be
// separately controlled from the same debugger.
add_task(async function() {
  await pushPref("devtools.debugger.features.windowless-workers", true);

  const dbg = await initDebugger("doc-windowless-workers.html");
  const mainThread = dbg.toolbox.threadClient.actor;

  // NOTE: by default we do not wait on worker
  // commands to complete because the thread could be
  // shutting down.
  dbg.client.waitForWorkers(true);

  const workers = await getWorkers(dbg);
  ok(workers.length == 2, "Got two workers");
  const worker1Thread = workers[0].actor;
  const worker2Thread = workers[1].actor;

  const mainThreadSource = findSource(dbg, "doc-windowless-workers.html");
  const workerSource = findSource(dbg, "simple-worker.js");

  info("Test pausing in the main thread");
  assertNotPaused(dbg);
  await dbg.actions.breakOnNext();
  await waitForPaused(dbg, "doc-windowless-workers.html");
  assertPausedAtSourceAndLine(dbg, mainThreadSource.id, 10);

  info("Test pausing in a worker");
  await dbg.actions.selectThread(worker1Thread);
  assertNotPaused(dbg);
  await dbg.actions.breakOnNext();
  await waitForPaused(dbg, "simple-worker.js");
  assertPausedAtSourceAndLine(dbg, workerSource.id, 3);

  await addExpression(dbg, "count");
  is(getLabel(dbg, 1), "count");
  const v = getValue(dbg, 1);
  ok(v == "" + +v, "Value of count should be a number");

  info("Test stepping in a worker");
  await stepOver(dbg);
  assertPausedAtSourceAndLine(dbg, workerSource.id, 4);

  // The watch expression should update with an incremented value.
  await waitUntil(() => {
    const v2 = getValue(dbg, 1);
    return +v2 == +v + 1;
  });

  info("Test resuming in a worker");
  await resume(dbg);
  assertNotPaused(dbg);

  info("Test stepping in the main thread");
  dbg.actions.selectThread(mainThread);
  await stepOver(dbg);
  assertPausedAtSourceAndLine(dbg, mainThreadSource.id, 11);

  info("Test resuming in the mainThread");
  await resume(dbg);
  assertNotPaused(dbg);

  info("Test pausing in both workers");
  await addBreakpoint(dbg, "simple-worker", 10);
  invokeInTab("sayHello");

  // Wait for both workers to pause. When a thread pauses the current thread
  // changes, and we don't want to get confused.
  const {
    selectors: { getIsPaused },
    getState
  } = dbg;
  await waitFor(() => {
    const state = getState();
    return getIsPaused(state, worker1Thread) && getIsPaused(state, worker2Thread);
  });

  dbg.actions.selectThread(worker1Thread);

  await waitForPaused(dbg);
  assertPausedAtSourceAndLine(dbg, workerSource.id, 10);

  dbg.actions.selectThread(worker2Thread);
  await waitForPaused(dbg);
  assertPausedAtSourceAndLine(dbg, workerSource.id, 10);

  info("Test stepping in second worker and not the first");
  await stepOver(dbg);
  assertPausedAtSourceAndLine(dbg, workerSource.id, 11);
  dbg.actions.selectThread(worker1Thread);
  assertPausedAtSourceAndLine(dbg, workerSource.id, 10);
});
