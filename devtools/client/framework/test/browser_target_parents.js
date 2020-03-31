/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test a given Target's parentFront attribute returns the correct parent front.

const { DevToolsClient } = require("devtools/client/devtools-client");
const { DevToolsServer } = require("devtools/server/devtools-server");

const TEST_URL = `data:text/html;charset=utf-8,<div id="test"></div>`;

// Test against Tab targets
add_task(async function() {
  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);

  const tab = await addTab(TEST_URL);

  const client = await setupDebuggerClient();
  const mainRoot = client.mainRoot;

  const tabs = await mainRoot.listTabs();
  const tabDescriptors = tabs.map(tabTarget => tabTarget.descriptorFront);

  await testGetTargetWithConcurrentCalls(tabDescriptors, tabTarget => {
    // Tab Target is attached when it has a console front.
    return !!tabTarget.getCachedFront("console");
  });

  await client.close();
  await removeTab(tab);
});

// Test against Process targets
add_task(async function() {
  const client = await setupDebuggerClient();
  const mainRoot = client.mainRoot;

  const processes = await mainRoot.listProcesses();

  // Assert that concurrent calls to getTarget resolves the same target and that it is already attached
  // With that, we were chasing a precise race, where a second call to ProcessDescriptor.getTarget()
  // happens between the instantiation of ContentProcessTarget and its call to attach() from getTarget
  // function.
  await testGetTargetWithConcurrentCalls(processes, processTarget => {
    // Content Process Target is attached when it has a console front.
    return !!processTarget.getCachedFront("console");
  });

  await client.close();
});

// Test against Frame targets
add_task(async function() {
  const client = await setupDebuggerClient();
  const mainRoot = client.mainRoot;

  const mainProcessDescriptor = await mainRoot.getMainProcess();
  const mainProcess = await mainProcessDescriptor.getTarget();
  const { frames } = await mainProcess.listRemoteFrames();

  await testGetTargetWithConcurrentCalls(frames, frameTarget => {
    // traits is one attribute to assert that a Frame Target is attached
    return !!frameTarget.traits;
  });

  await client.close();
});

// Test against Webextension targets
add_task(async function() {
  const client = await setupDebuggerClient();

  const mainRoot = client.mainRoot;

  const addons = await mainRoot.listAddons();
  await Promise.all(
    // some extensions, such as themes, are not debuggable. Filter those out
    // before trying to connect.
    addons
      .filter(a => a.debuggable)
      .map(async addonDescriptorFront => {
        const addonFront = await addonDescriptorFront.getTarget();
        is(
          addonFront.descriptorFront,
          addonDescriptorFront,
          "Got the correct descriptorFront from the addon target."
        );
      })
  );

  await client.close();
});

// Test against worker targets on parent process
add_task(async function() {
  const client = await setupDebuggerClient();

  const mainRoot = client.mainRoot;

  const { workers } = await mainRoot.listWorkers();
  await Promise.all(
    workers.map(workerTargetFront => {
      is(
        workerTargetFront.descriptorFront,
        mainRoot,
        "Got the Main Root as the descriptor for main root worker target."
      );
    })
  );

  await client.close();
});

async function setupDebuggerClient() {
  // Instantiate a minimal server
  DevToolsServer.init();
  DevToolsServer.allowChromeProcess = true;
  if (!DevToolsServer.createRootActor) {
    DevToolsServer.registerAllActors();
  }
  const transport = DevToolsServer.connectPipe();
  const client = new DevToolsClient(transport);
  await client.connect();
  return client;
}

async function testGetTargetWithConcurrentCalls(descriptors, isTargetAttached) {
  // Assert that concurrent calls to getTarget resolves the same target and that it is already attached
  await Promise.all(
    descriptors.map(async descriptor => {
      const promises = [];
      const concurrentCalls = 10;
      for (let i = 0; i < concurrentCalls; i++) {
        const targetPromise = descriptor.getTarget();
        // Every odd runs, wait for a tick to introduce some more randomness
        if (i % 2 == 0) {
          await wait(0);
        }
        promises.push(
          targetPromise.then(target => {
            is(
              target.descriptorFront,
              descriptor,
              "Got the correct descriptorFront from the frame target."
            );
            ok(isTargetAttached(target), "The target is attached");
            return target;
          })
        );
      }
      const targets = await Promise.all(promises);
      for (let i = 1; i < concurrentCalls; i++) {
        is(
          targets[0],
          targets[i],
          "All the targets returned by concurrent calls to getTarget are the same"
        );
      }
    })
  );
}
