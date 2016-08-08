// Check to make sure that a worker can be attached to a toolbox
// and that the console works.

var TAB_URL = EXAMPLE_URL + "doc_WorkerActor.attachThread-tab.html";
var WORKER_URL = "code_WorkerActor.attachThread-worker.js";

add_task(function* testWhilePaused() {
  let {client, tab, tabClient, workerClient, toolbox, gDebugger} =
    yield initWorkerDebugger(TAB_URL, WORKER_URL);

  let gTarget = gDebugger.gTarget;
  let gResumeButton = gDebugger.document.getElementById("resume");
  let gResumeKey = gDebugger.document.getElementById("resumeKey");

  // Execute some basic math to make sure evaluations are working.
  let jsterm = yield getSplitConsole(toolbox);
  let executed = yield jsterm.execute("10000+1");
  ok(executed.textContent.includes("10001"), "Text for message appeared correct");

  // Pause the worker by waiting for next execution and then sending a message to
  // it from the main thread.
  let oncePaused = gTarget.once("thread-paused");
  EventUtils.sendMouseEvent({ type: "mousedown" }, gResumeButton, gDebugger);
  once(gDebugger.gClient, "willInterrupt").then(() => {
    info("Posting message to worker, then waiting for a pause");
    postMessageToWorkerInTab(tab, WORKER_URL, "ping");
  });
  yield oncePaused;

  let command1 = jsterm.execute("10000+2");
  let command2 = jsterm.execute("10000+3");
  let command3 = jsterm.execute("foobar"); // throw an error

  info("Trying to get the result of command1");
  executed = yield command1;
  ok(executed.textContent.includes("10002"),
      "command1 executed successfully");

  info("Trying to get the result of command2");
  executed = yield command2;
  ok(executed.textContent.includes("10003"),
      "command2 executed successfully");

  info("Trying to get the result of command3");
  executed = yield command3;
  // XXXworkers This is failing until Bug 1215120 is resolved.
  todo(executed.textContent.includes("ReferenceError: foobar is not defined"),
      "command3 executed successfully");

  let onceResumed = gTarget.once("thread-resumed");
  EventUtils.sendMouseEvent({ type: "mousedown" }, gResumeButton, gDebugger);
  yield onceResumed;

  terminateWorkerInTab(tab, WORKER_URL);
  yield waitForWorkerClose(workerClient);
  yield gDevTools.closeToolbox(TargetFactory.forWorker(workerClient));
  yield close(client);
  yield removeTab(tab);
});
