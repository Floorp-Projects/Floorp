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

add_task(async function testNormalExecution() {
  const { client, tab, workerTargetFront, toolbox } = await initWorkerDebugger(
    TAB_URL,
    WORKER_URL
  );

  const hud = await getSplitConsole(toolbox);
  await executeAndWaitForMessage(hud, "this.location.toString()", WORKER_URL);
  ok(true, "Evaluating the global's location works");

  terminateWorkerInTab(tab, WORKER_URL);
  await waitForWorkerClose(workerTargetFront);
  await toolbox.destroy();
  await close(client);
  await removeTab(tab);
});
