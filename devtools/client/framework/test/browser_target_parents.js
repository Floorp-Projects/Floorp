/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test a given Target's parentFront attribute returns the correct parent front.

const {
  DevToolsClient,
} = require("resource://devtools/client/devtools-client.js");
const {
  DevToolsServer,
} = require("resource://devtools/server/devtools-server.js");
const {
  createCommandsDictionary,
} = require("resource://devtools/shared/commands/index.js");

const TEST_URL = `data:text/html;charset=utf-8,<div id="test"></div>`;

// Test against Tab targets
add_task(async function () {
  const tab = await addTab(TEST_URL);

  const client = await setupDebuggerClient();
  const mainRoot = client.mainRoot;

  const tabDescriptors = await mainRoot.listTabs();

  const concurrentCommands = [];
  for (const descriptor of tabDescriptors) {
    concurrentCommands.push(
      (async () => {
        const commands = await createCommandsDictionary(descriptor);
        // Descriptor's getTarget will only work if the TargetCommand watches for the first top target
        await commands.targetCommand.startListening();
      })()
    );
  }
  info("Instantiate all tab's commands and initialize their TargetCommand");
  await Promise.all(concurrentCommands);

  await testGetTargetWithConcurrentCalls(tabDescriptors, tabTarget => {
    // We only call BrowsingContextTargetFront.attach and not TargetMixin.attachAndInitThread.
    // So very few things are done.
    return !!tabTarget.targetForm?.traits;
  });

  await client.close();
  await removeTab(tab);
});

// Test against Process targets
add_task(async function () {
  const client = await setupDebuggerClient();
  const mainRoot = client.mainRoot;

  const processes = await mainRoot.listProcesses();

  // Assert that concurrent calls to getTarget resolves the same target and that it is already attached
  // With that, we were chasing a precise race, where a second call to ProcessDescriptor.getTarget()
  // happens between the instantiation of ContentProcessTarget and its call to attach() from getTarget
  // function.
  await testGetTargetWithConcurrentCalls(processes, processTarget => {
    // We only call ContentProcessTargetFront.attach and not TargetMixin.attachAndInitThread.
    // So nothing is done for content process targets.
    return true;
  });

  await client.close();
});

// Test against Webextension targets
add_task(async function () {
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
        ok(addonFront, "Got the addon target");
      })
  );

  await client.close();
});

// Test against worker targets on parent process
add_task(async function () {
  const client = await setupDebuggerClient();

  const mainRoot = client.mainRoot;

  const { workers } = await mainRoot.listWorkers();

  ok(!!workers.length, "list workers returned a non-empty list of workers");

  for (const workerDescriptorFront of workers) {
    let targetFront;
    try {
      targetFront = await workerDescriptorFront.getTarget();
    } catch (e) {
      // Ignore race condition where we are trying to connect to a worker
      // related to a previous test which is being destroyed.
      if (
        e.message.includes("nsIWorkerDebugger.initialize") ||
        workerDescriptorFront.isDestroyed() ||
        !workerDescriptorFront.name
      ) {
        info("Failed to connect to " + workerDescriptorFront.url);
        continue;
      }
      throw e;
    }
    // Bug 1767760: name might be null on some worker which are probably initializing or destroying.
    if (!workerDescriptorFront.name) {
      info("Failed to connect to " + workerDescriptorFront.url);
      continue;
    }

    is(
      workerDescriptorFront,
      targetFront,
      "For now, worker descriptors and targets are the same object (see bug 1667404)"
    );
    // Check that accessing descriptor#name getter doesn't throw (See Bug 1714974).
    ok(
      workerDescriptorFront.name.includes(".js") ||
        workerDescriptorFront.name.includes(".mjs"),
      `worker descriptor front holds the worker file name (${workerDescriptorFront.name})`
    );
    is(
      workerDescriptorFront.isWorkerDescriptor,
      true,
      "isWorkerDescriptor is true"
    );
  }

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
