// Check to make sure that a worker can be attached to a toolbox
// and that the console works.

var TAB_URL = EXAMPLE_URL + "doc_WorkerActor.attachThread-tab.html";
var WORKER_URL = "code_WorkerActor.attachThread-worker.js";

// Test to see if creating the pause from the console works.
add_task(function* testPausedByConsole() {
  let {client, tab, tabClient, workerClient, toolbox, gDebugger} =
    yield initWorkerDebugger(TAB_URL, WORKER_URL);

  let gTarget = gDebugger.gTarget;
  let gResumeButton = gDebugger.document.getElementById("resume");
  let gResumeKey = gDebugger.document.getElementById("resumeKey");

  let jsterm = yield getSplitConsole(toolbox);
  let executed = yield jsterm.execute("10000+1");
  ok(executed.textContent.includes("10001"),
      "Text for message appeared correct");

  let oncePaused = gTarget.once("thread-paused");
  EventUtils.sendMouseEvent({ type: "mousedown" }, gResumeButton, gDebugger);
  let pausedExecution = jsterm.execute("10000+2");

  info("Executed a command with 'break on next' active, waiting for pause");
  yield oncePaused;

  executed = yield jsterm.execute("10000+3");
  ok(executed.textContent.includes("10003"),
      "Text for message appeared correct");

  info("Waiting for a resume");
  let onceResumed = gTarget.once("thread-resumed");
  EventUtils.sendMouseEvent({ type: "mousedown" }, gResumeButton, gDebugger);
  yield onceResumed;

  executed = yield pausedExecution;
  ok(executed.textContent.includes("10002"),
      "Text for message appeared correct");

  terminateWorkerInTab(tab, WORKER_URL);
  yield waitForWorkerClose(workerClient);
  yield gDevTools.closeToolbox(TargetFactory.forWorker(workerClient));
  yield close(client);
  yield removeTab(tab);
});
