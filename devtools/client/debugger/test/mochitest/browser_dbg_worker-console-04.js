// Check that the date and regexp previewers work in the console of a worker debugger.

"use strict";

const TAB_URL = EXAMPLE_URL + "doc_WorkerActor.attachThread-tab.html";
const WORKER_URL = "code_WorkerActor.attachThread-worker.js";

add_task(function* testPausedByConsole() {
  let {client, tab, workerClient, toolbox} =
    yield initWorkerDebugger(TAB_URL, WORKER_URL);

  info("Check Date objects can be used in the console");
  let jsterm = yield getSplitConsole(toolbox);
  let executed = yield jsterm.execute("new Date(0)");
  ok(executed.textContent.includes("1970-01-01T00:00:00.000Z"),
      "Text for message appeared correct");

  info("Check RegExp objects can be used in the console");
  executed = yield jsterm.execute("new RegExp('.*')");
  ok(executed.textContent.includes("/.*/"),
      "Text for message appeared correct");

  terminateWorkerInTab(tab, WORKER_URL);
  yield waitForWorkerClose(workerClient);
  yield gDevTools.closeToolbox(TargetFactory.forWorker(workerClient));
  yield close(client);
  yield removeTab(tab);
});
