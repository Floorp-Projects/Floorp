/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
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
  const { client, tab, workerTargetFront, toolbox } = dbg;
  const workerThreadClient = await workerTargetFront.getFront("context");

  // Execute some basic math to make sure evaluations are working.
  const jsterm = await getSplitConsole(toolbox);
  let executed = await jsterm.execute("10000+1");
  ok(
    executed.textContent.includes("10001"),
    "Text for message appeared correct"
  );

  await clickElement(dbg, "pause");
  workerThreadClient.once("willInterrupt").then(() => {
    info("Posting message to worker, then waiting for a pause");
    postMessageToWorkerInTab(tab, WORKER_URL, "ping");
  });
  await waitForPaused(dbg);

  const command1 = jsterm.execute("10000+2");
  const command2 = jsterm.execute("10000+3");
  const command3 = jsterm.execute("foobar"); // throw an error

  info("Trying to get the result of command1");
  executed = await command1;
  ok(executed.textContent.includes("10002"), "command1 executed successfully");

  info("Trying to get the result of command2");
  executed = await command2;
  ok(executed.textContent.includes("10003"), "command2 executed successfully");

  info("Trying to get the result of command3");
  executed = await command3;
  ok(
    executed.textContent.includes("ReferenceError: foobar is not defined"),
    "command3 executed successfully"
  );

  await resume(dbg);

  terminateWorkerInTab(tab, WORKER_URL);
  await waitForWorkerClose(workerTargetFront);
  await toolbox.destroy();
  await close(client);
  await removeTab(tab);
});
