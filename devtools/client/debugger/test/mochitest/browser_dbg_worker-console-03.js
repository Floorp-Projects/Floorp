// Check to make sure that a worker can be attached to a toolbox
// and that the console works.

var TAB_URL = EXAMPLE_URL + "doc_WorkerActor.attachThread-tab.html";
var WORKER_URL = "code_WorkerActor.attachThread-worker.js";

// Test to see if creating the pause from the console works.
add_task(async function testPausedByConsole() {
  let {client, tab, tabClient, workerClient, toolbox, gDebugger} =
    await initWorkerDebugger(TAB_URL, WORKER_URL);

  let gTarget = gDebugger.gTarget;
  let gResumeButton = gDebugger.document.getElementById("resume");
  let gResumeKey = gDebugger.document.getElementById("resumeKey");

  let jsterm = await getSplitConsole(toolbox);
  let executed = await jsterm.execute("10000+1");
  ok(executed.textContent.includes("10001"),
      "Text for message appeared correct");

  let oncePaused = gTarget.once("thread-paused");
  EventUtils.sendMouseEvent({ type: "mousedown" }, gResumeButton, gDebugger);
  let pausedExecution = jsterm.execute("10000+2");

  info("Executed a command with 'break on next' active, waiting for pause");
  await oncePaused;

  executed = await jsterm.execute("10000+3");
  ok(executed.textContent.includes("10003"),
      "Text for message appeared correct");

  info("Waiting for a resume");
  let onceResumed = gTarget.once("thread-resumed");
  EventUtils.sendMouseEvent({ type: "mousedown" }, gResumeButton, gDebugger);
  await onceResumed;

  executed = await pausedExecution;
  ok(executed.textContent.includes("10002"),
      "Text for message appeared correct");

  terminateWorkerInTab(tab, WORKER_URL);
  await waitForWorkerClose(workerClient);
  await gDevTools.closeToolbox(TargetFactory.forWorker(workerClient));
  await close(client);
  await removeTab(tab);
});
