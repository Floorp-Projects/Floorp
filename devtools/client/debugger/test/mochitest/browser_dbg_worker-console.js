// Check to make sure that a worker can be attached to a toolbox
// and that the console works.

var TAB_URL = EXAMPLE_URL + "doc_WorkerActor.attachThread-tab.html";
var WORKER_URL = "code_WorkerActor.attachThread-worker.js";

function* initWorkerDebugger(TAB_URL, WORKER_URL) {
  if (!DebuggerServer.initialized) {
    DebuggerServer.init();
    DebuggerServer.addBrowserActors();
  }

  let client = new DebuggerClient(DebuggerServer.connectPipe());
  yield connect(client);

  let tab = yield addTab(TAB_URL);
  let { tabs } = yield listTabs(client);
  let [, tabClient] = yield attachTab(client, findTab(tabs, TAB_URL));

  yield createWorkerInTab(tab, WORKER_URL);

  let { workers } = yield listWorkers(tabClient);
  let [, workerClient] = yield attachWorker(tabClient,
                                             findWorker(workers, WORKER_URL));

  let toolbox = yield gDevTools.showToolbox(TargetFactory.forWorker(workerClient),
                                            "jsdebugger",
                                            Toolbox.HostType.WINDOW);

  let debuggerPanel = toolbox.getCurrentPanel();
  let gDebugger = debuggerPanel.panelWin;

  return {client, tab, tabClient, workerClient, toolbox, gDebugger};
}

add_task(function* testNormalExecution() {
  let {client, tab, tabClient, workerClient, toolbox, gDebugger} =
    yield initWorkerDebugger(TAB_URL, WORKER_URL);

  let jsterm = yield getSplitConsole(toolbox);
  let executed = yield jsterm.execute("this.location.toString()");
  ok(executed.textContent.includes(WORKER_URL),
      "Evaluating the global's location works");

  yield gDevTools.closeToolbox(TargetFactory.forWorker(workerClient));
  terminateWorkerInTab(tab, WORKER_URL);
  yield waitForWorkerClose(workerClient);
  yield close(client);
  yield removeTab(tab);
});

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

  yield gDevTools.closeToolbox(TargetFactory.forWorker(workerClient));
  terminateWorkerInTab(tab, WORKER_URL);
  yield waitForWorkerClose(workerClient);
  yield close(client);
  yield removeTab(tab);
});

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

  yield gDevTools.closeToolbox(TargetFactory.forWorker(workerClient));
  terminateWorkerInTab(tab, WORKER_URL);
  yield waitForWorkerClose(workerClient);
  yield close(client);
  yield removeTab(tab);
});
