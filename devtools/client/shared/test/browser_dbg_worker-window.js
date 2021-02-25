// Check to make sure that a worker can be attached to a toolbox
// directly, and that the toolbox has expected properties.

"use strict";

// Import helpers for the workers
/* import-globals-from helper_workers.js */
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/shared/test/helper_workers.js",
  this
);

const TAB_URL = EXAMPLE_URL + "doc_WorkerTargetActor.attachThread-tab.html";
const WORKER_URL = "code_WorkerTargetActor.attachThread-worker.js";

add_task(async function() {
  const tab = await addTab(TAB_URL);
  const target = await createAndAttachTargetForTab(tab);

  await listWorkers(target);
  await createWorkerInTab(tab, WORKER_URL);

  const { workers } = await listWorkers(target);
  const workerTarget = findWorker(workers, WORKER_URL);

  const toolbox = await gDevTools.showToolbox(
    workerTarget,
    "jsdebugger",
    Toolbox.HostType.WINDOW
  );

  is(toolbox.hostType, "window", "correct host");

  await new Promise(done => {
    toolbox.win.parent.addEventListener("message", function onmessage(event) {
      if (event.data.name == "set-host-title") {
        toolbox.win.parent.removeEventListener("message", onmessage);
        done();
      }
    });
  });
  ok(
    toolbox.win.parent.document.title.includes(WORKER_URL),
    "worker URL in host title"
  );

  const toolTabs = toolbox.doc.querySelectorAll(".devtools-tab");
  const activeTools = [...toolTabs].map(toolTab =>
    toolTab.getAttribute("data-id")
  );

  is(
    activeTools.join(","),
    "webconsole,jsdebugger",
    "Correct set of tools supported by worker"
  );

  terminateWorkerInTab(tab, WORKER_URL);
  await waitForWorkerClose(workerTarget);
  await target.destroy();

  await toolbox.destroy();
});
