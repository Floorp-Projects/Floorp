// Check to make sure that a worker can be attached to a toolbox
// and that the console works.

var TAB_URL = EXAMPLE_URL + "doc_WorkerActor.attachThread-tab.html";
var WORKER_URL = "code_WorkerActor.attachThread-worker.js";

add_task(async function testWhilePaused() {
  let {client, tab, tabClient, workerClient, toolbox, gDebugger} =
    await initWorkerDebugger(TAB_URL, WORKER_URL);

  let gTarget = gDebugger.gTarget;
  let gResumeButton = gDebugger.document.getElementById("resume");
  let gResumeKey = gDebugger.document.getElementById("resumeKey");

  // Execute some basic math to make sure evaluations are working.
  let jsterm = await getSplitConsole(toolbox);
  let executed = await jsterm.execute("10000+1");
  ok(executed.textContent.includes("10001"), "Text for message appeared correct");

  // Pause the worker by waiting for next execution and then sending a message to
  // it from the main thread.
  let oncePaused = gTarget.once("thread-paused");
  EventUtils.sendMouseEvent({ type: "mousedown" }, gResumeButton, gDebugger);
  once(gDebugger.gClient, "willInterrupt").then(() => {
    info("Posting message to worker, then waiting for a pause");
    postMessageToWorkerInTab(tab, WORKER_URL, "ping");
  });
  await oncePaused;

  let command1 = jsterm.execute("10000+2");
  let command2 = jsterm.execute("10000+3");
  let command3 = jsterm.execute("foobar"); // throw an error

  info("Trying to get the result of command1");
  executed = await command1;
  ok(executed.textContent.includes("10002"),
      "command1 executed successfully");

  info("Trying to get the result of command2");
  executed = await command2;
  ok(executed.textContent.includes("10003"),
      "command2 executed successfully");

  info("Trying to get the result of command3");
  executed = await command3;
  ok(executed.textContent.includes("ReferenceError: foobar is not defined"),
     "command3 executed successfully");

  let onceResumed = gTarget.once("thread-resumed");
  EventUtils.sendMouseEvent({ type: "mousedown" }, gResumeButton, gDebugger);
  await onceResumed;

  terminateWorkerInTab(tab, WORKER_URL);
  await waitForWorkerClose(workerClient);
  await gDevTools.closeToolbox(TargetFactory.forWorker(workerClient));
  await close(client);
  await removeTab(tab);
});
