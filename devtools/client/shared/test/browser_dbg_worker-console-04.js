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

// Check that the date and regexp previewers work in the console of a worker debugger.

"use strict";

// There are shutdown issues for which multiple rejections are left uncaught.
// See bug 1018184 for resolving these issues.
const { PromiseTestUtils } = scopedCuImport("resource://testing-common/PromiseTestUtils.jsm");
PromiseTestUtils.whitelistRejectionsGlobally(/connection just closed/);

const TAB_URL = EXAMPLE_URL + "doc_WorkerTargetActor.attachThread-tab.html";
const WORKER_URL = "code_WorkerTargetActor.attachThread-worker.js";

add_task(async function testPausedByConsole() {
  const {client, tab, workerTargetFront, toolbox} =
    await initWorkerDebugger(TAB_URL, WORKER_URL);

  info("Check Date objects can be used in the console");
  const jsterm = await getSplitConsole(toolbox);
  let executed = await jsterm.execute("new Date(0)");
  ok(executed.textContent.includes("1970-01-01T00:00:00.000Z"),
      "Text for message appeared correct");

  info("Check RegExp objects can be used in the console");
  executed = await jsterm.execute("new RegExp('.*')");
  ok(executed.textContent.includes("/.*/"),
      "Text for message appeared correct");

  terminateWorkerInTab(tab, WORKER_URL);
  await waitForWorkerClose(workerTargetFront);
  await gDevTools.closeToolbox(TargetFactory.forWorker(workerTargetFront));
  await close(client);
  await removeTab(tab);
});
