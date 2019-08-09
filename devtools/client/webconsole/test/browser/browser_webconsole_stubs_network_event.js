/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {
  STUBS_UPDATE_ENV,
  formatPacket,
  formatNetworkEventStub,
  formatFile,
  getStubFilePath,
} = require("devtools/client/webconsole/test/browser/stub-generator-helpers");

const TEST_URI =
  "http://example.com/browser/devtools/client/webconsole/test/browser/stub-generators/test-network-event.html";

add_task(async function() {
  const isStubsUpdate = env.get(STUBS_UPDATE_ENV) == "true";
  const filePath = getStubFilePath("networkEvent.js", env);
  info(`${isStubsUpdate ? "Update" : "Check"} stubs at ${filePath}`);

  const generatedStubs = await generateNetworkEventStubs();

  if (isStubsUpdate) {
    await OS.File.writeAtomic(filePath, generatedStubs);
    ok(true, `${filePath} was successfully updated`);
    return;
  }

  const repoStubFileContent = await OS.File.read(filePath, {
    encoding: "utf-8",
  });
  is(generatedStubs, repoStubFileContent, "stubs file is up to date");

  if (generatedStubs != repoStubFileContent) {
    ok(
      false,
      "The network event stubs file needs to be updated by running " +
        "`mach test devtools/client/webconsole/test/browser/" +
        "browser_webconsole_stubs_network_event.js --headless " +
        "--setenv WEBCONSOLE_STUBS_UPDATE=true`"
    );
  }
});

async function generateNetworkEventStubs() {
  const stubs = {
    preparedMessages: [],
    packets: [],
  };

  const toolbox = await openNewTabAndToolbox(TEST_URI, "webconsole");
  const { ui } = toolbox.getCurrentPanel().hud;

  for (const [key, code] of getCommands()) {
    const onNetwork = new Promise(resolve => {
      toolbox.target.activeConsole.once("networkEvent", function onNetworkEvent(
        res
      ) {
        stubs.packets.push(formatPacket(key, res));
        stubs.preparedMessages.push(formatNetworkEventStub(key, res));
        toolbox.target.activeConsole.off("networkEvent", onNetworkEvent);
        resolve();
      });
    });

    const onNetworkUpdate = new Promise(resolve => {
      ui.once("network-message-updated", function onNetworkUpdated(res) {
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

        stubs.packets.push(formatPacket(updateKey, packet));
        stubs.preparedMessages.push(formatNetworkEventStub(updateKey, res));
        resolve();
      });
    });

    await ContentTask.spawn(gBrowser.selectedBrowser, code, function(subCode) {
      console.log("subCode", subCode);
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
  return formatFile(stubs, "NetworkEventMessage");
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
