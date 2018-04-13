// Check that the date and regexp previewers work in the console of a worker debugger.

"use strict";

// There are shutdown issues for which multiple rejections are left uncaught.
// See bug 1018184 for resolving these issues.
const { PromiseTestUtils } = scopedCuImport("resource://testing-common/PromiseTestUtils.jsm");
PromiseTestUtils.whitelistRejectionsGlobally(/connection just closed/);

const TAB_URL = EXAMPLE_URL + "doc_WorkerActor.attachThread-tab.html";
const WORKER_URL = "code_WorkerActor.attachThread-worker.js";

add_task(async function testPausedByConsole() {
  let {client, tab, workerClient, toolbox} =
    await initWorkerDebugger(TAB_URL, WORKER_URL);

  info("Check Date objects can be used in the console");
  let jsterm = await getSplitConsole(toolbox);
  let executed = await jsterm.execute("new Date(0)");
  ok(executed.textContent.includes("1970-01-01T00:00:00.000Z"),
      "Text for message appeared correct");

  info("Check RegExp objects can be used in the console");
  executed = await jsterm.execute("new RegExp('.*')");
  ok(executed.textContent.includes("/.*/"),
      "Text for message appeared correct");

  terminateWorkerInTab(tab, WORKER_URL);
  await waitForWorkerClose(workerClient);
  await gDevTools.closeToolbox(TargetFactory.forWorker(workerClient));
  await close(client);
  await removeTab(tab);
});
