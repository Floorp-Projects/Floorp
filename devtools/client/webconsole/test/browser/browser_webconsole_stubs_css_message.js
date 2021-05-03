/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {
  STUBS_UPDATE_ENV,
  createCommandsForTab,
  getCleanedPacket,
  getStubFile,
  writeStubsToFile,
} = require(`${CHROME_URL_ROOT}stub-generator-helpers`);

const TEST_URI =
  "http://example.com/browser/devtools/client/webconsole/test/browser/stub-generators/test-css-message.html";
const STUB_FILE = "cssMessage.js";

add_task(async function() {
  const isStubsUpdate = env.get(STUBS_UPDATE_ENV) == "true";
  info(`${isStubsUpdate ? "Update" : "Check"} ${STUB_FILE}`);

  const generatedStubs = await generateCssMessageStubs();

  if (isStubsUpdate) {
    await writeStubsToFile(env, STUB_FILE, generatedStubs);
    ok(true, `${STUB_FILE} was updated`);
    return;
  }

  const existingStubs = getStubFile(STUB_FILE);
  const FAILURE_MSG =
    "The cssMessage stubs file needs to be updated by running `" +
    `mach test ${getCurrentTestFilePath()} --headless --setenv WEBCONSOLE_STUBS_UPDATE=true` +
    "`";

  if (generatedStubs.size !== existingStubs.stubPackets.size) {
    ok(false, FAILURE_MSG);
    return;
  }

  let failed = false;
  for (const [key, packet] of generatedStubs) {
    const packetStr = JSON.stringify(packet, null, 2);
    const existingPacketStr = JSON.stringify(
      existingStubs.stubPackets.get(key),
      null,
      2
    );
    is(packetStr, existingPacketStr, `"${key}" packet has expected value`);
    failed = failed || packetStr !== existingPacketStr;
  }

  if (failed) {
    ok(false, FAILURE_MSG);
  } else {
    ok(true, "Stubs are up to date");
  }
});

async function generateCssMessageStubs() {
  const stubs = new Map();

  const tab = await addTab(TEST_URI);
  const commands = await createCommandsForTab(tab);
  await commands.targetCommand.startListening();
  const resourceWatcher = commands.resourceCommand;

  // The resource-watcher only supports a single call to watch/unwatch per
  // instance, so we attach a unique watch callback, which will forward the
  // resource to `handleErrorMessage`, dynamically updated for each command.
  let handleCSSMessage = function() {};

  const onCSSMessageAvailable = resources => {
    for (const resource of resources) {
      handleCSSMessage(resource);
    }
  };

  await resourceWatcher.watchResources([resourceWatcher.TYPES.CSS_MESSAGE], {
    onAvailable: onCSSMessageAvailable,
  });

  for (const code of getCommands()) {
    const received = new Promise(resolve => {
      handleCSSMessage = function(packet) {
        const key = packet.pageError.errorMessage;
        stubs.set(key, getCleanedPacket(key, packet));
        resolve();
      };
    });

    await SpecialPowers.spawn(gBrowser.selectedBrowser, [code], function(
      subCode
    ) {
      content.docShell.cssErrorReportingEnabled = true;
      const style = content.document.createElement("style");
      style.append(content.document.createTextNode(subCode));
      content.document.body.append(style);
    });

    await received;
  }

  resourceWatcher.unwatchResources([resourceWatcher.TYPES.CSS_MESSAGE], {
    onAvailable: onCSSMessageAvailable,
  });

  await commands.destroy();
  return stubs;
}

function getCommands() {
  return [
    `
  p {
    such-unknown-property: wow;
  }
  `,
    `
  p {
    padding-top: invalid value;
  }
  `,
  ];
}
