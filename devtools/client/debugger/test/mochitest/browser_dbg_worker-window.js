// Check to make sure that a worker can be attached to a toolbox
// directly, and that the toolbox has expected properties.

"use strict";

// Whitelisting this test.
// As part of bug 1077403, the leaking uncaught rejections should be fixed.
thisTestLeaksUncaughtRejectionsAndShouldBeFixed("[object Object]");

var TAB_URL = EXAMPLE_URL + "doc_WorkerActor.attachThread-tab.html";
var WORKER_URL = "code_WorkerActor.attachThread-worker.js";

add_task(function* () {
  yield pushPrefs(["devtools.scratchpad.enabled", true]);

  DebuggerServer.init();
  DebuggerServer.addBrowserActors();

  let client = new DebuggerClient(DebuggerServer.connectPipe());
  yield connect(client);

  let tab = yield addTab(TAB_URL);
  let { tabs } = yield listTabs(client);
  let [, tabClient] = yield attachTab(client, findTab(tabs, TAB_URL));

  yield listWorkers(tabClient);
  yield createWorkerInTab(tab, WORKER_URL);

  let { workers } = yield listWorkers(tabClient);
  let [, workerClient] = yield attachWorker(tabClient,
                                             findWorker(workers, WORKER_URL));

  let toolbox = yield gDevTools.showToolbox(TargetFactory.forWorker(workerClient),
                                            "jsdebugger",
                                            Toolbox.HostType.WINDOW);

  is(toolbox._host.type, "window", "correct host");
  ok(toolbox._host._window.document.title.includes(WORKER_URL),
     "worker URL in host title");

  let toolTabs = toolbox.doc.querySelectorAll(".devtools-tab");
  let activeTools = [...toolTabs].map(tab=>tab.getAttribute("toolid"));

  is(activeTools.join(","), "webconsole,jsdebugger,scratchpad,options",
    "Correct set of tools supported by worker");

  terminateWorkerInTab(tab, WORKER_URL);
  yield waitForWorkerClose(workerClient);
  yield close(client);

  yield toolbox.destroy();
});
