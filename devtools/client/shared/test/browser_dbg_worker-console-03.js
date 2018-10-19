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
  this);

var TAB_URL = EXAMPLE_URL + "doc_WorkerTargetActor.attachThread-tab.html";
var WORKER_URL = "code_WorkerTargetActor.attachThread-worker.js";

// Test to see if creating the pause from the console works.
add_task(async function testPausedByConsole() {
  const dbg = await initWorkerDebugger(TAB_URL, WORKER_URL);
  const {client, tab, workerTargetFront, toolbox} = dbg;

  const jsterm = await getSplitConsole(toolbox);
  let executed = await jsterm.execute("10000+1");
  ok(executed.textContent.includes("10001"),
      "Text for message appeared correct");

  await clickElement(dbg, "pause");

  const pausedExecution = jsterm.execute("10000+2");

  info("Executed a command with 'break on next' active, waiting for pause");
  await waitForPaused(dbg);

  executed = await jsterm.execute("10000+3");
  ok(executed.textContent.includes("10003"),
      "Text for message appeared correct");

  info("Waiting for a resume");
  await clickElement(dbg, "resume");

  executed = await pausedExecution;
  ok(executed.textContent.includes("10002"),
      "Text for message appeared correct");

  terminateWorkerInTab(tab, WORKER_URL);
  await waitForWorkerClose(workerTargetFront);
  await gDevTools.closeToolbox(TargetFactory.forWorker(workerTargetFront));
  await close(client);
  await removeTab(tab);
});
