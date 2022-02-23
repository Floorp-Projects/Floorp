/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Test basic windowless worker functionality: the main thread and worker can be
// separately controlled from the same debugger.
add_task(async function() {
  await pushPref("devtools.debugger.features.windowless-workers", true);
  await pushPref("devtools.debugger.workers-visible", true);

  const dbg = await initDebugger("doc-windowless-workers.html");
  const mainThread = dbg.toolbox.threadFront.actor;

  await waitForThreadCount(dbg, 2);
  const workers = dbg.selectors.getThreads();
  ok(workers.length == 2, "Got two workers");
  const thread1 = workers[0].actor;
  const thread2 = workers[1].actor;

  const mainThreadSource = findSource(dbg, "doc-windowless-workers.html");

  await waitForSource(dbg, "simple-worker.js");
  const workerSource = findSource(dbg, "simple-worker.js");

  info("Pause in the main thread");
  assertNotPaused(dbg);
  await dbg.actions.breakOnNext(getThreadContext(dbg));
  await waitForPaused(dbg, "doc-windowless-workers.html");
  assertPausedAtSourceAndLine(dbg, mainThreadSource.id, 10);
  threadIsSelected(dbg, 1);

  info("Pause in the first worker");
  await dbg.actions.selectThread(getContext(dbg), thread1);
  assertNotPaused(dbg);
  await dbg.actions.breakOnNext(getThreadContext(dbg));
  await waitForPaused(dbg, "simple-worker.js");
  threadIsSelected(dbg, 2);
  const workerSource2 = dbg.selectors.getSelectedSourceWithContent();
  assertPausedAtSourceAndLine(dbg, workerSource2.id, 3);

  info("Add a watch expression and view the value");
  await addExpression(dbg, "count");
  is(getWatchExpressionLabel(dbg, 1), "count");
  const v = getWatchExpressionValue(dbg, 1);
  ok(v == `${+v}`, "Value of count should be a number");

  info("StepOver in the first worker");
  await stepOver(dbg);
  assertPausedAtSourceAndLine(dbg, workerSource2.id, 4);

  info("Ensure that the watch expression has updated");
  await waitUntil(() => {
    const v2 = getWatchExpressionValue(dbg, 1);
    return +v2 == +v + 1;
  });

  info("Resume in the first worker");
  await resume(dbg);
  assertNotPaused(dbg);

  info("StepOver in the main thread");
  dbg.actions.selectThread(getContext(dbg), mainThread);
  await stepOver(dbg);
  assertPausedAtSourceAndLine(dbg, mainThreadSource.id, 11);

  info("Resume in the mainThread");
  await resume(dbg);
  assertNotPaused(dbg);

  info("Pause in both workers");
  await addBreakpoint(dbg, "simple-worker", 10);
  invokeInTab("sayHello");

  info("Wait for both workers to pause");
  // When a thread pauses the current thread changes,
  // and we don't want to get confused.
  await waitForPausedThread(dbg, thread1);
  await waitForPausedThread(dbg, thread2);
  threadIsPaused(dbg, 2);
  threadIsPaused(dbg, 3);

  info("View the first paused thread");
  dbg.actions.selectThread(getContext(dbg), thread1);
  await waitForPaused(dbg);
  assertPausedAtSourceAndLine(dbg, workerSource2.id, 10);

  info("View the second paused thread");
  await dbg.actions.selectThread(getContext(dbg), thread2);
  threadIsSelected(dbg, 3);
  await waitForPaused(dbg);
  const workerSource3 = dbg.selectors.getSelectedSourceWithContent();
  assertPausedAtSourceAndLine(dbg, workerSource3.id, 10);

  info("StepOver in second worker and not the first");
  await stepOver(dbg);
  assertPausedAtSourceAndLine(dbg, workerSource3.id, 11);
  await dbg.actions.selectThread(getContext(dbg), thread1);
  assertPausedAtSourceAndLine(dbg, workerSource2.id, 10);
});

function assertClass(dbg, selector, className, ...args) {
  ok(
    findElement(dbg, selector, ...args).classList.contains(className),
    `${className} class exists`
  );
}

function threadIsPaused(dbg, index) {
  return ok(findElement(dbg, "threadsPaneItemPause", index));
}

function threadIsSelected(dbg, index) {
  return assertClass(dbg, "threadsPaneItem", "selected", index);
}
