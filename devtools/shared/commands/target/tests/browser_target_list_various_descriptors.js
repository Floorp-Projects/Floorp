/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the TargetCommand API with all possible descriptors

const TEST_URL = "https://example.org/document-builder.sjs?html=org";
const CHROME_WORKER_URL = CHROME_URL_ROOT + "test_worker.js";

add_task(async function() {
  // Enabled fission prefs
  await pushPref("devtools.browsertoolbox.fission", true);
  // Disable the preloaded process as it gets created lazily and may interfere
  // with process count assertions
  await pushPref("dom.ipc.processPrelaunch.enabled", false);
  // This preference helps destroying the content process when we close the tab
  await pushPref("dom.ipc.keepProcessesAlive.web", 1);

  const client = await createLocalClient();
  const mainRoot = client.mainRoot;

  await testTab(mainRoot);
  await testParentProcess(mainRoot);
  await testContentProcess(mainRoot);
  await testWorker(mainRoot);
  await testWebExtension(mainRoot);

  await client.close();
});

async function testParentProcess(rootFront) {
  info("Test TargetCommand against parent process descriptor");

  const descriptor = await rootFront.getMainProcess();
  const commands = await descriptor.getCommands();
  const targetList = commands.targetCommand;
  await targetList.startListening();

  const targets = await targetList.getAllTargets(targetList.ALL_TYPES);
  ok(
    targets.length > 1,
    "We get many targets when debugging the parent process"
  );
  const targetFront = targets[0];
  is(targetFront, targetList.targetFront, "The first is the top level one");
  is(
    targetFront.targetType,
    targetList.TYPES.FRAME,
    "the parent process target is of frame type, because it inherits from BrowsingContextTargetActor"
  );
  is(targetFront.isTopLevel, true, "This is flagged as top level");

  targetList.destroy();
  await waitForAllTargetsToBeAttached(targetList);
}

async function testTab(rootFront) {
  info("Test TargetCommand against tab descriptor");

  const tab = await addTab(TEST_URL);
  const descriptor = await rootFront.getTab({ tab });
  const commands = await descriptor.getCommands();
  const targetList = commands.targetCommand;
  await targetList.startListening();
  // Avoid the target to close the client when we destroy the target list and the target
  targetList.targetFront.shouldCloseClient = false;

  const targets = await targetList.getAllTargets(targetList.ALL_TYPES);
  is(targets.length, 1, "Got a unique target");
  const targetFront = targets[0];
  is(targetFront, targetList.targetFront, "The first is the top level one");
  is(
    targetFront.targetType,
    targetList.TYPES.FRAME,
    "the tab target is of frame type"
  );
  is(targetFront.isTopLevel, true, "This is flagged as top level");

  targetList.destroy();

  BrowserTestUtils.removeTab(tab);
}

async function testWebExtension(rootFront) {
  info("Test TargetCommand against webextension descriptor");

  const extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "temporary",
    manifest: {
      name: "Sample extension",
    },
  });

  await extension.startup();

  const descriptor = await rootFront.getAddon({ id: extension.id });
  const commands = await descriptor.getCommands();
  const targetList = commands.targetCommand;
  await targetList.startListening();

  const targets = await targetList.getAllTargets(targetList.ALL_TYPES);
  is(targets.length, 1, "Got a unique target");
  const targetFront = targets[0];
  is(targetFront, targetList.targetFront, "The first is the top level one");
  is(
    targetFront.targetType,
    targetList.TYPES.FRAME,
    "the web extension target is of frame type, because it inherits from BrowsingContextTargetActor"
  );
  is(targetFront.isTopLevel, true, "This is flagged as top level");

  targetList.destroy();

  await extension.unload();
}

async function testContentProcess(rootFront) {
  info("Test TargetCommand against content process descriptor");

  const tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: "data:text/html,foo",
    forceNewProcess: true,
  });

  const { osPid } = tab.linkedBrowser.browsingContext.currentWindowGlobal;

  const descriptor = await rootFront.getProcess(osPid);
  const commands = await descriptor.getCommands();
  const targetList = commands.targetCommand;
  await targetList.startListening();

  const targets = await targetList.getAllTargets(targetList.ALL_TYPES);
  is(targets.length, 1, "Got a unique target");
  const targetFront = targets[0];
  is(targetFront, targetList.targetFront, "The first is the top level one");
  is(
    targetFront.targetType,
    targetList.TYPES.PROCESS,
    "the content process target is of process type"
  );
  is(targetFront.isTopLevel, true, "This is flagged as top level");

  targetList.destroy();

  BrowserTestUtils.removeTab(tab);
}

async function testWorker(rootFront) {
  info("Test TargetCommand against worker descriptor");

  const workerUrl = CHROME_WORKER_URL + "#descriptor";
  new Worker(workerUrl);

  const { workers } = await rootFront.listWorkers();
  const descriptor = workers.find(w => w.url == workerUrl);
  const commands = await descriptor.getCommands();
  const targetList = commands.targetCommand;
  await targetList.startListening();

  const targets = await targetList.getAllTargets(targetList.ALL_TYPES);
  is(targets.length, 1, "Got a unique target");
  const targetFront = targets[0];
  is(targetFront, targetList.targetFront, "The first is the top level one");
  is(
    targetFront.targetType,
    targetList.TYPES.WORKER,
    "the worker target is of worker type"
  );
  is(targetFront.isTopLevel, true, "This is flagged as top level");

  targetList.destroy();
}
