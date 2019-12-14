/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {
  STUBS_UPDATE_ENV,
  getStubFilePath,
  getCleanedPacket,
  writeStubsToFile,
} = require("devtools/client/webconsole/test/browser/stub-generator-helpers");

const TEST_URI =
  "http://example.com/browser/devtools/client/webconsole/test/browser/stub-generators/test-network-event.html";
const STUB_FILE = "networkEvent.js";

add_task(async function() {
  const isStubsUpdate = env.get(STUBS_UPDATE_ENV) == "true";
  info(`${isStubsUpdate ? "Update" : "Check"} ${STUB_FILE}`);

  const generatedStubs = await generateNetworkEventStubs();

  if (isStubsUpdate) {
    await writeStubsToFile(
      getStubFilePath(STUB_FILE, env, true),
      generatedStubs,
      true
    );
    ok(true, `${STUB_FILE} was updated`);
    return;
  }

  const existingStubs = require(getStubFilePath(STUB_FILE));
  const FAILURE_MSG =
    "The network event stubs file needs to be updated by running " +
    "`mach test devtools/client/webconsole/test/browser/" +
    "browser_webconsole_stubs_network_event.js --headless " +
    "--setenv WEBCONSOLE_STUBS_UPDATE=true`";

  if (generatedStubs.size !== existingStubs.stubPackets.size) {
    ok(false, FAILURE_MSG);
    return;
  }

  let failed = false;
  for (const [key, packet] of generatedStubs) {
    // packet.updates are handle by the webconsole front, and can be updated after
    // we cleaned the packet, so the order isn't guaranteed. Let's sort the array
    // here so the test doesn't fail.
    const existingPacket = existingStubs.stubPackets.get(key);
    if (packet.updates && existingPacket.updates) {
      packet.updates.sort();
      existingPacket.updates.sort();
    }

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
  const packets = new Map();
  const toolbox = await openNewTabAndToolbox(TEST_URI, "webconsole");
  const { ui } = toolbox.getCurrentPanel().hud;

  for (const [key, code] of getCommands()) {
    const consoleFront = await toolbox.target.getFront("console");
    const onNetwork = consoleFront.once("networkEvent", packet => {
      packets.set(key, getCleanedPacket(key, packet));
    });

    const onNetworkUpdate = ui.once("network-message-updated", res => {
      const updateKey = `${key} update`;
      // We cannot ensure the form of the network update packet, some properties
      // might be in another order than in the original packet.
      // Hand-picking only what we need should prevent this.
      const packet = {
        networkInfo: {
          _type: res.networkInfo._type,
          actor: res.networkInfo.actor,
          request: res.networkInfo.request,
          response: res.networkInfo.response,
          totalTime: res.networkInfo.totalTime,
        },
      };
      packets.set(updateKey, getCleanedPacket(updateKey, packet));
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

    await Promise.all([onNetwork, onNetworkUpdate]);
  }

  await closeTabAndToolbox();
  return packets;
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
