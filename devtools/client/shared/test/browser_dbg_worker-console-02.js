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

add_task(async function testWhilePaused() {
  const dbg = await initWorkerDebugger(TAB_URL, WORKER_URL);
  const { client, tab, workerDescriptorFront, toolbox } = dbg;
  const {
    selectors: { getIsWaitingOnBreak, getCurrentThread },
  } = dbg;

  await waitForSourcesInSourceTree(dbg, [WORKER_URL]);

  // Execute some basic math to make sure evaluations are working.
  const hud = await getSplitConsole(toolbox);
  await executeAndWaitForMessage(hud, "10000+1", "10001");
  ok(true, "Text for message appeared correct");

  await clickElement(dbg, "pause");
  await waitForState(dbg, state => getIsWaitingOnBreak(getCurrentThread()));

  info("Posting message to worker, then waiting for a pause");
  postMessageToWorkerInTab(tab, WORKER_URL, "ping");

  await waitForPaused(dbg);

  const command1 = executeAndWaitForMessage(hud, "10000+2", "10002");
  const command2 = executeAndWaitForMessage(hud, "10000+3", "10003");
  // throw an error
  const command3 = executeAndWaitForMessage(
    hud,
    "foobar",
    "ReferenceError: foobar is not defined",
    "error"
  );

  info("Trying to get the result of command1");
  let executed = await command1;
  ok(executed, "command1 executed successfully");

  info("Trying to get the result of command2");
  executed = await command2;
  ok(executed, "command2 executed successfully");

  info("Trying to get the result of command3");
  executed = await command3;
  ok(executed, "command3 executed successfully");

  await resume(dbg);

  terminateWorkerInTab(tab, WORKER_URL);
  await waitForWorkerClose(workerDescriptorFront);
  await toolbox.destroy();
  await close(client);
  await removeTab(tab);
});
