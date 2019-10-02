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

  await Promise.all(
    processes.map(async processDescriptor => {
      const process = await processDescriptor.getTarget();

      is(
        process.descriptorFront,
        processDescriptor,
        "Got the correct descriptorFront from the process target."
      );
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
  await Promise.all(
    frames.map(async frameDescriptorFront => {
      const frameTargetFront = await frameDescriptorFront.getTarget();

      is(
        frameTargetFront.descriptorFront,
        frameDescriptorFront,
        "Got the correct descriptorFront from the frame target."
      );
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
