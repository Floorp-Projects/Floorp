// Check to make sure that a worker can be attached to a toolbox
// and that the console works.

var TAB_URL = EXAMPLE_URL + "doc_WorkerActor.attachThread-tab.html";
var WORKER_URL = "code_WorkerActor.attachThread-worker.js";

add_task(function* testNormalExecution() {
  let {client, tab, tabClient, workerClient, toolbox, gDebugger} =
    yield initWorkerDebugger(TAB_URL, WORKER_URL);

  let jsterm = yield getSplitConsole(toolbox);
  let executed = yield jsterm.execute("this.location.toString()");
  ok(executed.textContent.includes(WORKER_URL),
      "Evaluating the global's location works");

  terminateWorkerInTab(tab, WORKER_URL);
  yield waitForWorkerClose(workerClient);
  yield gDevTools.closeToolbox(TargetFactory.forWorker(workerClient));
  yield close(client);
  yield removeTab(tab);
});
