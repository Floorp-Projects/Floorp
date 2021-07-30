/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the TargetCommand API with all possible descriptors

const TEST_URL = "https://example.org/document-builder.sjs?html=org";
const SECOND_TEST_URL = "https://example.com/document-builder.sjs?html=org";
const CHROME_WORKER_URL = CHROME_URL_ROOT + "test_worker.js";

add_task(async function() {
  // Enabled fission prefs
  await pushPref("devtools.browsertoolbox.fission", true);
  // Disable the preloaded process as it gets created lazily and may interfere
  // with process count assertions
  await pushPref("dom.ipc.processPrelaunch.enabled", false);
  // This preference helps destroying the content process when we close the tab
  await pushPref("dom.ipc.keepProcessesAlive.web", 1);

  await testLocalTab();
  await testRemoteTab();
  await testParentProcess();
  await testContentProcess();
  await testWorker();
  await testWebExtension();
});

async function testParentProcess() {
  info("Test TargetCommand against parent process descriptor");

  const commands = await CommandsFactory.forMainProcess();
  const { descriptorFront } = commands;
  is(
    descriptorFront.isParentProcessDescriptor,
    true,
    "Descriptor front isParentProcessDescriptor is correct"
  );
  is(
    descriptorFront.isProcessDescriptor,
    true,
    "Descriptor front isProcessDescriptor is correct"
  );

  const targetCommand = commands.targetCommand;
  await targetCommand.startListening();

  const targets = await targetCommand.getAllTargets(targetCommand.ALL_TYPES);
  ok(
    targets.length > 1,
    "We get many targets when debugging the parent process"
  );
  const targetFront = targets[0];
  is(targetFront, targetCommand.targetFront, "The first is the top level one");
  is(
    targetFront.targetType,
    targetCommand.TYPES.FRAME,
    "the parent process target is of frame type, because it inherits from BrowsingContextTargetActor"
  );
  is(targetFront.isTopLevel, true, "This is flagged as top level");

  targetCommand.destroy();
  await waitForAllTargetsToBeAttached(targetCommand);

  await commands.destroy();
}

async function testLocalTab() {
  info("Test TargetCommand against local tab descriptor (via getTab({ tab }))");

  const tab = await addTab(TEST_URL);
  const commands = await CommandsFactory.forTab(tab);
  const { descriptorFront } = commands;
  is(
    descriptorFront.isTabDescriptor,
    true,
    "Descriptor front isTabDescriptor is correct"
  );

  // By default, tab descriptor will close the client when destroying the client
  // Disable this behavior via this boolean
  // Bug 1698890: The test should probably stop assuming this.
  descriptorFront.shouldCloseClient = false;

  const targetCommand = commands.targetCommand;
  await targetCommand.startListening();

  const targets = await targetCommand.getAllTargets(targetCommand.ALL_TYPES);
  is(targets.length, 1, "Got a unique target");
  const targetFront = targets[0];
  is(targetFront, targetCommand.targetFront, "The first is the top level one");
  is(
    targetFront.targetType,
    targetCommand.TYPES.FRAME,
    "the tab target is of frame type"
  );
  is(targetFront.isTopLevel, true, "This is flagged as top level");

  targetCommand.destroy();

  BrowserTestUtils.removeTab(tab);

  await commands.destroy();
}

async function testRemoteTab() {
  info(
    "Test TargetCommand against remote tab descriptor (via getTab({ outerWindowID }))"
  );

  const tab = await addTab(TEST_URL);
  const commands = await CommandsFactory.forRemoteTabInTest({
    outerWindowID: tab.linkedBrowser.outerWindowID,
  });
  const { descriptorFront } = commands;
  is(
    descriptorFront.isTabDescriptor,
    true,
    "Descriptor front isTabDescriptor is correct"
  );

  const targetCommand = commands.targetCommand;
  await targetCommand.startListening();

  const targets = await targetCommand.getAllTargets(targetCommand.ALL_TYPES);
  is(targets.length, 1, "Got a unique target");
  const targetFront = targets[0];
  is(
    targetFront,
    targetCommand.targetFront,
    "TargetCommand top target is the same as the first target"
  );
  is(
    targetFront.targetType,
    targetCommand.TYPES.FRAME,
    "the tab target is of frame type"
  );
  is(targetFront.isTopLevel, true, "This is flagged as top level");

  const browser = tab.linkedBrowser;
  const onLoaded = BrowserTestUtils.browserLoaded(browser);
  await BrowserTestUtils.loadURI(browser, SECOND_TEST_URL);
  await onLoaded;

  if (isFissionEnabled()) {
    info("With fission, cross process switching destroy everything");
    ok(targetFront.isDestroyed(), "Top level target is destroyed");
    ok(commands.descriptorFront.isDestroyed(), "Descriptor is also destroyed");
  } else {
    is(
      targetCommand.targetFront,
      targetFront,
      "Without fission, the top target stays the same"
    );
  }

  targetCommand.destroy();

  BrowserTestUtils.removeTab(tab);

  await commands.destroy();
}

async function testWebExtension() {
  info("Test TargetCommand against webextension descriptor");

  const extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "temporary",
    manifest: {
      name: "Sample extension",
    },
  });

  await extension.startup();

  const commands = await CommandsFactory.forAddon(extension.id);
  const { descriptorFront } = commands;
  is(
    descriptorFront.isWebExtensionDescriptor,
    true,
    "Descriptor front isWebExtensionDescriptor is correct"
  );

  const targetCommand = commands.targetCommand;
  await targetCommand.startListening();

  const targets = await targetCommand.getAllTargets(targetCommand.ALL_TYPES);
  is(targets.length, 1, "Got a unique target");
  const targetFront = targets[0];
  is(targetFront, targetCommand.targetFront, "The first is the top level one");
  is(
    targetFront.targetType,
    targetCommand.TYPES.FRAME,
    "the web extension target is of frame type, because it inherits from BrowsingContextTargetActor"
  );
  is(targetFront.isTopLevel, true, "This is flagged as top level");

  targetCommand.destroy();

  await extension.unload();

  await commands.destroy();
}

async function testContentProcess() {
  info("Test TargetCommand against content process descriptor");

  const tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: "data:text/html,foo",
    forceNewProcess: true,
  });

  const { osPid } = tab.linkedBrowser.browsingContext.currentWindowGlobal;

  const commands = await CommandsFactory.forProcess(osPid);
  const { descriptorFront } = commands;
  is(
    descriptorFront.isProcessDescriptor,
    true,
    "Descriptor front isProcessDescriptor is correct"
  );
  is(
    descriptorFront.isParentProcessDescriptor,
    false,
    "Descriptor front isParentProcessDescriptor is false for content processes"
  );

  const targetCommand = commands.targetCommand;
  await targetCommand.startListening();

  const targets = await targetCommand.getAllTargets(targetCommand.ALL_TYPES);
  is(targets.length, 1, "Got a unique target");
  const targetFront = targets[0];
  is(targetFront, targetCommand.targetFront, "The first is the top level one");
  is(
    targetFront.targetType,
    targetCommand.TYPES.PROCESS,
    "the content process target is of process type"
  );
  is(targetFront.isTopLevel, true, "This is flagged as top level");

  targetCommand.destroy();

  BrowserTestUtils.removeTab(tab);

  await commands.destroy();
}

// CommandsFactory expect the worker id, which is computed from the nsIWorkerDebugger.id attribute
function getWorkerDebuggerId(url) {
  const wdm = Cc[
    "@mozilla.org/dom/workers/workerdebuggermanager;1"
  ].createInstance(Ci.nsIWorkerDebuggerManager);
  const workers = wdm.getWorkerDebuggerEnumerator();
  while (workers.hasMoreElements()) {
    const worker = workers.getNext();
    worker.QueryInterface(Ci.nsIWorkerDebugger);
    if (worker.url == url) {
      return worker.id;
    }
  }
  return null;
}

async function testWorker() {
  info("Test TargetCommand against worker descriptor");

  const workerUrl = CHROME_WORKER_URL + "#descriptor";
  new Worker(workerUrl);

  const workerId = getWorkerDebuggerId(workerUrl);
  ok(workerId, "Found the worker Debugger ID");
  const commands = await CommandsFactory.forWorker(workerId);
  const { descriptorFront } = commands;
  is(
    descriptorFront.isWorkerDescriptor,
    true,
    "Descriptor front isWorkerDescriptor is correct"
  );

  const targetCommand = commands.targetCommand;
  await targetCommand.startListening();

  const targets = await targetCommand.getAllTargets(targetCommand.ALL_TYPES);
  is(targets.length, 1, "Got a unique target");
  const targetFront = targets[0];
  is(targetFront, targetCommand.targetFront, "The first is the top level one");
  is(
    targetFront.targetType,
    targetCommand.TYPES.WORKER,
    "the worker target is of worker type"
  );
  is(targetFront.isTopLevel, true, "This is flagged as top level");

  targetCommand.destroy();

  // Calling CommandsFactory.forWorker, will call RootFront.getWorker
  // which will spawn lots of worker legacy code, firing lots of requests,
  // which may still be pending
  await commands.waitForRequestsToSettle();

  await commands.destroy();
}
