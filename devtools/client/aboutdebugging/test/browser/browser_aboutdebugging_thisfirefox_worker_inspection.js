/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function() {
  const thisFirefoxClient = createThisFirefoxClientMock();
  // Prepare a worker mock.
  const testWorker = {
    id: "test-worker-id",
    name: "Test Worker",
  };
  // Add a worker mock as other worker.
  thisFirefoxClient.listWorkers = () => ({
    otherWorkers: [testWorker],
    serviceWorkers: [],
    sharedWorkers: [],
  });
  thisFirefoxClient.client.mainRoot = {
    getWorker: id => {
      return id === testWorker.id ? testWorker : null;
    },
  };

  const runtimeClientFactoryMock = createRuntimeClientFactoryMock();
  runtimeClientFactoryMock.createClientForRuntime = runtime => {
    const {
      RUNTIMES,
    } = require("devtools/client/aboutdebugging/src/constants");
    if (runtime.id === RUNTIMES.THIS_FIREFOX) {
      return thisFirefoxClient;
    }
    throw new Error("Unexpected runtime id " + runtime.id);
  };

  info("Enable mocks");
  enableRuntimeClientFactoryMock(runtimeClientFactoryMock);
  registerCleanupFunction(() => {
    disableRuntimeClientFactoryMock();
  });

  const { document, tab, window } = await openAboutDebugging();
  await selectThisFirefoxPage(document, window.AboutDebugging.store);

  info("Open a toolbox to debug the worker");
  const { devtoolsTab, devtoolsWindow } = await openAboutDevtoolsToolbox(
    document,
    tab,
    window,
    testWorker.name,
    false
  );

  info(
    "Check whether the correct actor front will be opened in worker toolbox"
  );
  const url = new window.URL(devtoolsWindow.location.href);
  const workerID = url.searchParams.get("id");
  is(
    workerID,
    testWorker.id,
    "Correct actor front will be opened in worker toolbox"
  );

  await removeTab(devtoolsTab);
  await waitUntil(() => !findDebugTargetByText("Toolbox - ", document));
  await removeTab(tab);
});
