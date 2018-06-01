// Check to make sure that a worker can be attached to a toolbox
// and that the console works.

var TAB_URL = EXAMPLE_URL + "doc_WorkerTargetActor.attachThread-tab.html";
var WORKER_URL = "code_WorkerTargetActor.attachThread-worker.js";

add_task(async function testNormalExecution() {
  let {client, tab, tabClient, workerClient, toolbox, gDebugger} =
    await initWorkerDebugger(TAB_URL, WORKER_URL);

  let jsterm = await getSplitConsole(toolbox);
  let executed = await jsterm.execute("this.location.toString()");
  ok(executed.textContent.includes(WORKER_URL),
      "Evaluating the global's location works");

  terminateWorkerInTab(tab, WORKER_URL);
  await waitForWorkerClose(workerClient);
  await gDevTools.closeToolbox(TargetFactory.forWorker(workerClient));
  await close(client);
  await removeTab(tab);
});
