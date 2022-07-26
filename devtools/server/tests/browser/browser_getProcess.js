/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test `RootActor.getProcess` method
 */

"use strict";

add_task(async () => {
  let client, tab;

  function connect() {
    // Fake a first connection to the content process
    const transport = DevToolsServer.connectPipe();
    client = new DevToolsClient(transport);
    return client.connect();
  }

  async function listProcess() {
    const onNewProcess = new Promise(resolve => {
      // Call listProcesses in order to start receiving new process notifications
      client.mainRoot.on("processListChanged", function listener() {
        client.off("processListChanged", listener);
        ok(true, "Received processListChanged event");
        resolve();
      });
    });
    await client.mainRoot.listProcesses();
    await createNewProcess();
    return onNewProcess;
  }

  async function createNewProcess() {
    tab = await BrowserTestUtils.openNewForegroundTab({
      gBrowser,
      url: "data:text/html,new-process",
      forceNewProcess: true,
    });
  }

  async function getProcess() {
    // Note that we can't assert process count as the number of processes
    // is affected by previous tests.
    const processes = await client.mainRoot.listProcesses();
    const { osPid } = tab.linkedBrowser.browsingContext.currentWindowGlobal;
    const descriptor = processes.find(process => process.id == osPid);
    ok(descriptor, "Got the new process descriptor");

    // Connect to the first content process available
    const content = processes.filter(p => !p.isParentProcessDescriptor)[0];

    const processDescriptor = await client.mainRoot.getProcess(content.id);
    const front = await processDescriptor.getTarget();
    const targetForm = front.targetForm;
    ok(targetForm.consoleActor, "Got the console actor");
    ok(targetForm.threadActor, "Got the thread actor");

    // Ensure sending at least one request to an actor
    const commands = await CommandsFactory.forProcess(osPid);
    await commands.targetCommand.startListening();
    const { result } = await commands.scriptCommand.execute("var a = 42; a");

    is(result, 42, "console.eval worked");

    return [front, content.id];
  }

  // Assert that calling client.getProcess against the same process id is
  // returning the same actor.
  async function getProcessAgain(firstTargetFront, id) {
    const processDescriptor = await client.mainRoot.getProcess(id);
    const front = await processDescriptor.getTarget();
    is(
      front,
      firstTargetFront,
      "Second call to getProcess with the same id returns the same form"
    );
  }

  function processScript() {
    /* eslint-env mozilla/process-script */
    const listener = function() {
      Services.obs.removeObserver(listener, "devtools:loader:destroy");
      sendAsyncMessage("test:getProcess-destroy", null);
    };
    Services.obs.addObserver(listener, "devtools:loader:destroy");
  }

  async function closeClient() {
    const onLoaderDestroyed = new Promise(done => {
      const processListener = function() {
        Services.ppmm.removeMessageListener(
          "test:getProcess-destroy",
          processListener
        );
        done();
      };
      Services.ppmm.addMessageListener(
        "test:getProcess-destroy",
        processListener
      );
    });
    const script = `data:,(${encodeURI(processScript)})()`;
    Services.ppmm.loadProcessScript(script, true);
    await client.close();

    await onLoaderDestroyed;
    Services.ppmm.removeDelayedProcessScript(script);
    info("Loader destroyed in the content process");
  }

  // Instantiate a minimal server
  DevToolsServer.init();
  DevToolsServer.allowChromeProcess = true;
  if (!DevToolsServer.createRootActor) {
    DevToolsServer.registerAllActors();
  }

  await connect();
  await listProcess();

  const [front, contentId] = await getProcess();

  await getProcessAgain(front, contentId);

  await closeClient();

  BrowserTestUtils.removeTab(tab);
  DevToolsServer.destroy();
});
