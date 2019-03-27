/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { gDevToolsBrowser } = require("devtools/client/framework/devtools-browser");

add_task(async function() {
  const thisFirefoxClient = createThisFirefoxClientMock();
  // Prepare a worker mock.
  const testWorkerTargetFront = {
    actorID: "test-worker-id",
  };
  const testWorker = {
    name: "Test Worker",
    workerTargetFront: testWorkerTargetFront,
  };
  // Add a worker mock as other worker.
  thisFirefoxClient.listWorkers = () => ({
    otherWorkers: [testWorker],
    serviceWorkers: [],
    sharedWorkers: [],
  });
  // Override getActor function of client which is used in inspect action for worker.
  thisFirefoxClient.client.getActor = id => {
    return id === testWorkerTargetFront.actorID ? testWorkerTargetFront : null;
  };

  const runtimeClientFactoryMock = createRuntimeClientFactoryMock();
  runtimeClientFactoryMock.createClientForRuntime = runtime => {
    const { RUNTIMES } = require("devtools/client/aboutdebugging-new/src/constants");
    if (runtime.id === RUNTIMES.THIS_FIREFOX) {
      return thisFirefoxClient;
    }
    throw new Error("Unexpected runtime id " + runtime.id);
  };

  info("Enable mocks");
  enableRuntimeClientFactoryMock(runtimeClientFactoryMock);
  const originalOpenWorkerForToolbox =  gDevToolsBrowser.openWorkerToolbox;
  registerCleanupFunction(() => {
    disableRuntimeClientFactoryMock();
    gDevToolsBrowser.openWorkerToolbox = originalOpenWorkerForToolbox;
  });

  const { document, tab, window } = await openAboutDebugging();
  await selectThisFirefoxPage(document, window.AboutDebugging.store);

  info("Open a toolbox to debug the worker");
  const { devtoolsTab, devtoolsWindow } =
    await openAboutDevtoolsToolbox(document, tab, window, testWorker.name, false);

  info("Check whether the correct actor front will be opened in worker toolbox");
  const url = new window.URL(devtoolsWindow.location.href);
  const workderID = url.searchParams.get("id");
  is(workderID, testWorkerTargetFront.actorID,
     "Correct actor front will be opened in worker toolbox");

  await removeTab(devtoolsTab);
  await waitUntil(() => !findDebugTargetByText("about:devtools-toolbox?", document));
  await removeTab(tab);
});
