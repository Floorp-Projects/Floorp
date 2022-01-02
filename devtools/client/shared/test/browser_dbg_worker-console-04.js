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

// Check that the date and regexp previewers work in the console of a worker debugger.

("use strict");

// There are shutdown issues for which multiple rejections are left uncaught.
// See bug 1018184 for resolving these issues.
PromiseTestUtils.allowMatchingRejectionsGlobally(/connection just closed/);

const TAB_URL = EXAMPLE_URL + "doc_WorkerTargetActor.attachThread-tab.html";
const WORKER_URL = "code_WorkerTargetActor.attachThread-worker.js";

add_task(async function testPausedByConsole() {
  const {
    client,
    tab,
    workerDescriptorFront,
    toolbox,
  } = await initWorkerDebugger(TAB_URL, WORKER_URL);

  info("Check Date objects can be used in the console");
  const console = await getSplitConsole(toolbox);
  let executed = await executeAndWaitForMessage(
    console,
    "new Date(2013, 3, 1)",
    "Mon Apr 01 2013 00:00:00"
  );
  ok(executed, "Date object has expected text content");

  info("Check RegExp objects can be used in the console");
  executed = await executeAndWaitForMessage(
    console,
    "new RegExp('.*')",
    "/.*/"
  );
  ok(executed, "Text for message appeared correct");

  terminateWorkerInTab(tab, WORKER_URL);
  await waitForWorkerClose(workerDescriptorFront);
  await toolbox.destroy();
  await close(client);
  await removeTab(tab);
});
