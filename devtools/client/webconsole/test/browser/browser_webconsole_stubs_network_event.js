/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {
  createCommandsForTab,
  STUBS_UPDATE_ENV,
  getStubFile,
  getCleanedPacket,
  writeStubsToFile,
} = require(`${CHROME_URL_ROOT}stub-generator-helpers`);

const TEST_URI =
  "http://example.com/browser/devtools/client/webconsole/test/browser/stub-generators/test-network-event.html";
const STUB_FILE = "networkEvent.js";

add_task(async function() {
  const isStubsUpdate = env.get(STUBS_UPDATE_ENV) == "true";
  info(`${isStubsUpdate ? "Update" : "Check"} ${STUB_FILE}`);

  const generatedStubs = await generateNetworkEventStubs();

  if (isStubsUpdate) {
    await writeStubsToFile(env, STUB_FILE, generatedStubs, true);
    ok(true, `${STUB_FILE} was updated`);
    return;
  }

  const existingStubs = getStubFile(STUB_FILE);
  const FAILURE_MSG =
    "The network event stubs file needs to be updated by running `" +
    `mach test ${getCurrentTestFilePath()} --headless --setenv WEBCONSOLE_STUBS_UPDATE=true` +
    "`";

  if (generatedStubs.size !== existingStubs.stubPackets.size) {
    ok(false, FAILURE_MSG);
    return;
  }

  let failed = false;
  for (const [key, packet] of generatedStubs) {
    const existingPacket = existingStubs.stubPackets.get(key);
    const packetStr = JSON.stringify(packet, null, 2);
    const existingPacketStr = JSON.stringify(existingPacket, null, 2);
    is(packetStr, existingPacketStr, `"${key}" packet has expected value`);
    failed = failed || packetStr !== existingPacketStr;
  }

  if (failed) {
    ok(false, FAILURE_MSG);
  } else {
    ok(true, "Stubs are up to date");
  }
});

async function generateNetworkEventStubs() {
  const stubs = new Map();
  const tab = await addTab(TEST_URI);
  const commands = await createCommandsForTab(tab);
  await commands.targetCommand.startListening();
  const resourceWatcher = commands.resourceCommand;

  const stacktraces = new Map();
  let addNetworkStub = function() {};
  let addNetworkUpdateStub = function() {};

  const onAvailable = resources => {
    for (const resource of resources) {
      if (resource.resourceType == resourceWatcher.TYPES.NETWORK_EVENT) {
        if (stacktraces.has(resource.channelId)) {
          const { stacktraceAvailable, lastFrame } = stacktraces.get(
            resource.channelId
          );
          resource.cause.stacktraceAvailable = stacktraceAvailable;
          resource.cause.lastFrame = lastFrame;
          stacktraces.delete(resource.channelId);
        }
        addNetworkStub(resource);
        continue;
      }
      if (
        resource.resourceType == resourceWatcher.TYPES.NETWORK_EVENT_STACKTRACE
      ) {
        stacktraces.set(resource.channelId, resource);
      }
    }
  };
  const onUpdated = updates => {
    for (const { resource } of updates) {
      addNetworkUpdateStub(resource);
    }
  };

  await resourceWatcher.watchResources(
    [
      resourceWatcher.TYPES.NETWORK_EVENT_STACKTRACE,
      resourceWatcher.TYPES.NETWORK_EVENT,
    ],
    {
      onAvailable,
      onUpdated,
    }
  );

  for (const [key, code] of getCommands()) {
    const networkEventDone = new Promise(resolve => {
      addNetworkStub = resource => {
        stubs.set(key, getCleanedPacket(key, getOrderedResource(resource)));
        resolve();
      };
    });
    const networkEventUpdateDone = new Promise(resolve => {
      addNetworkUpdateStub = resource => {
        const updateKey = `${key} update`;
        stubs.set(key, getCleanedPacket(key, getOrderedResource(resource)));
        stubs.set(
          updateKey,
          // We cannot ensure the form of the resource, some properties
          // might be in another order than in the original resource.
          // Hand-picking only what we need should prevent this.
          getCleanedPacket(updateKey, getOrderedResource(resource))
        );
        resolve();
      };
    });

    await SpecialPowers.spawn(gBrowser.selectedBrowser, [code], function(
      subCode
    ) {
      const script = content.document.createElement("script");
      script.append(
        content.document.createTextNode(`function triggerPacket() {${subCode}}`)
      );
      content.document.body.append(script);
      content.wrappedJSObject.triggerPacket();
      script.remove();
    });
    await Promise.all([networkEventDone, networkEventUpdateDone]);
  }
  resourceWatcher.unwatchResources(
    [
      resourceWatcher.TYPES.NETWORK_EVENT_STACKTRACE,
      resourceWatcher.TYPES.NETWORK_EVENT,
    ],
    {
      onAvailable,
      onUpdated,
    }
  );

  await commands.destroy();

  return stubs;
}
// Ensures the order of the resource properties
function getOrderedResource(resource) {
  return {
    resourceType: resource.resourceType,
    timeStamp: resource.timeStamp,
    actor: resource.actor,
    startedDateTime: resource.startedDateTime,
    method: resource.method,
    url: resource.url,
    isXHR: resource.isXHR,
    cause: resource.cause,
    httpVersion: resource.httpVersion,
    status: resource.status,
    statusText: resource.statusText,
    headersSize: resource.headersSize,
    remoteAddress: resource.remoteAddress,
    remotePort: resource.remotePort,
    mimeType: resource.mimeType,
    waitingTime: resource.waitingTime,
    contentSize: resource.contentSize,
    transferredSize: resource.transferredSize,
    timings: resource.timings,
    private: resource.private,
    fromCache: resource.fromCache,
    fromServiceWorker: resource.fromServiceWorker,
    isThirdPartyTrackingResource: resource.isThirdPartyTrackingResource,
    referrerPolicy: resource.referrerPolicy,
    blockedReason: resource.blockedReason,
    blockingExtension: resource.blockingExtension,
    channelId: resource.channelId,
    totalTime: resource.totalTime,
    securityState: resource.securityState,
    responseCache: resource.responseCache,
    isRacing: resource.isRacing,
  };
}

function getCommands() {
  const networkEvent = new Map();

  networkEvent.set(
    "GET request",
    `
let i = document.createElement("img");
i.src = "/inexistent.html";
`
  );

  networkEvent.set(
    "XHR GET request",
    `
const xhr = new XMLHttpRequest();
xhr.open("GET", "/inexistent.html");
xhr.send();
`
  );

  networkEvent.set(
    "XHR POST request",
    `
const xhr = new XMLHttpRequest();
xhr.open("POST", "/inexistent.html");
xhr.send();
`
  );
  return networkEvent;
}
