/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test a given Target's parentFront attribute returns the correct parent front.

const { DebuggerClient } = require("devtools/shared/client/debugger-client");
const { DebuggerServer } = require("devtools/server/debugger-server");

const TEST_URL = `data:text/html;charset=utf-8,<div id="test"></div>`;

// Test against Tab targets
add_task(async function() {
  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);

  const tab = await addTab(TEST_URL);
  const target = await TargetFactory.forTab(tab);
  const mainRoot = target.client.mainRoot;

  is(
    target.descriptorFront,
    mainRoot,
    "Got the correct descriptorFront from the target."
  );
  await removeTab(tab);
});

// Test against Process targets
add_task(async function() {
  // Instantiate a minimal server
  DebuggerServer.init();
  DebuggerServer.allowChromeProcess = true;
  if (!DebuggerServer.createRootActor) {
    DebuggerServer.registerAllActors();
  }
  const transport = DebuggerServer.connectPipe();
  const client = new DebuggerClient(transport);
  await client.connect();

  const mainRoot = client.mainRoot;

  const { processes } = await mainRoot.listProcesses();

  // Assert that concurrent calls to getTarget resolves the same target and that it is already attached
  // We that, we were chasing a precise race, where a second call to ProcessDescriptor.getTarget()
  // happens between the instantiation of ContentProcessTarget and its call to attach() from getTarget
  // function.
  await Promise.all(
    processes.map(async processDescriptor => {
      const promises = [];
      const concurrentCalls = 10;
      for (let i = 0; i < concurrentCalls; i++) {
        const targetPromise = processDescriptor.getTarget();
        // Every odd runs, wait for a tick to introduce some more randomness
        if (i % 2 == 0) {
          await wait(0);
        }
        promises.push(
          targetPromise.then(target => {
            is(
              target.descriptorFront,
              processDescriptor,
              "Got the correct descriptorFront from the process target."
            );
            // Content Process Target is attached when it has a console front.
            ok(target.getCachedFront("console"), "The target is attached");
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

  await client.close();
});

// Test against Frame targets
add_task(async function() {
  // Instantiate a minimal server
  DebuggerServer.init();
  DebuggerServer.allowChromeProcess = true;
  if (!DebuggerServer.createRootActor) {
    DebuggerServer.registerAllActors();
  }
  const transport = DebuggerServer.connectPipe();
  const client = new DebuggerClient(transport);
  await client.connect();

  const mainRoot = client.mainRoot;

  const mainProcess = await mainRoot.getMainProcess();
  const { frames } = await mainProcess.listRemoteFrames();

  // Assert that concurrent calls to getTarget resolves the same target and that it is already attached
  await Promise.all(
    frames.map(async frameDescriptor => {
      const promises = [];
      const concurrentCalls = 10;
      for (let i = 0; i < concurrentCalls; i++) {
        const targetPromise = frameDescriptor.getTarget();
        // Every odd runs, wait for a tick to introduce some more randomness
        if (i % 2 == 0) {
          await wait(0);
        }
        promises.push(
          targetPromise.then(target => {
            is(
              target.descriptorFront,
              frameDescriptor,
              "Got the correct descriptorFront from the frame target."
            );
            // traits is one attribute to assert that a Frame Target is attached
            ok(target.traits, "The target is attached");
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

  await client.close();
});

// Test against Webextension targets
add_task(async function() {
  // Instantiate a minimal server
  DebuggerServer.init();
  DebuggerServer.allowChromeProcess = true;
  if (!DebuggerServer.createRootActor) {
    DebuggerServer.registerAllActors();
  }
  const transport = DebuggerServer.connectPipe();
  const client = new DebuggerClient(transport);
  await client.connect();

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
  // Instantiate a minimal server
  DebuggerServer.init();
  DebuggerServer.allowChromeProcess = true;
  if (!DebuggerServer.createRootActor) {
    DebuggerServer.registerAllActors();
  }
  const transport = DebuggerServer.connectPipe();
  const client = new DebuggerClient(transport);
  await client.connect();

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
