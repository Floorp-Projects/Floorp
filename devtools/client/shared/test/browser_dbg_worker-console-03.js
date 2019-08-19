/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check to make sure that a worker can be attached to a toolbox
// and that the console works.

// Import helpers for the workers
/* import-globals-from helper_workers.js */
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/shared/test/helper_workers.js",
  this
);

var TAB_URL = EXAMPLE_URL + "doc_WorkerTargetActor.attachThread-tab.html";
var WORKER_URL = "code_WorkerTargetActor.attachThread-worker.js";

// Test to see if creating the pause from the console works.
add_task(async function testPausedByConsole() {
  const dbg = await initWorkerDebugger(TAB_URL, WORKER_URL);
  const { client, tab, workerTargetFront, toolbox } = dbg;

  const console = await getSplitConsole(toolbox);
  let executed = await executeAndWaitForMessage(console, "10000+1", "10001");
  ok(executed, "Text for message appeared correct");

  await clickElement(dbg, "pause");

  const pausedExecution = executeAndWaitForMessage(console, "10000+2", "10002");

  info("Executed a command with 'break on next' active, waiting for pause");
  await waitForPaused(dbg);

  executed = await executeAndWaitForMessage(console, "10000+3", "10003");
  ok(executed, "Text for message appeared correct");

  info("Waiting for a resume");
  await clickElement(dbg, "resume");

  executed = await pausedExecution;
  ok(executed, "Text for message appeared correct");

  terminateWorkerInTab(tab, WORKER_URL);
  await waitForWorkerClose(workerTargetFront);
  await toolbox.destroy();
  await close(client);
  await removeTab(tab);
});
