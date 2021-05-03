/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {
  STUBS_UPDATE_ENV,
  createCommandsForMainProcess,
  getCleanedPacket,
  getSerializedPacket,
  getStubFile,
  writeStubsToFile,
} = require(`${CHROME_URL_ROOT}stub-generator-helpers`);

const STUB_FILE = "platformMessage.js";

add_task(async function() {
  const isStubsUpdate = env.get(STUBS_UPDATE_ENV) == "true";
  info(`${isStubsUpdate ? "Update" : "Check"} ${STUB_FILE}`);

  const generatedStubs = await generatePlatformMessagesStubs();

  if (isStubsUpdate) {
    await writeStubsToFile(env, STUB_FILE, generatedStubs);
    ok(true, `${STUB_FILE} was updated`);
    return;
  }

  const existingStubs = getStubFile(STUB_FILE);

  const FAILURE_MSG =
    "The platformMessage stubs file needs to be updated by running `" +
    `mach test ${getCurrentTestFilePath()} --headless --setenv WEBCONSOLE_STUBS_UPDATE=true` +
    "`";

  if (generatedStubs.size !== existingStubs.rawPackets.size) {
    ok(false, FAILURE_MSG);
    return;
  }

  let failed = false;
  for (const [key, packet] of generatedStubs) {
    const packetStr = getSerializedPacket(packet, { sortKeys: true });
    const existingPacketStr = getSerializedPacket(
      existingStubs.rawPackets.get(key),
      { sortKeys: true }
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

async function generatePlatformMessagesStubs() {
  const stubs = new Map();

  const commands = await createCommandsForMainProcess();
  await commands.targetCommand.startListening();
  const resourceWatcher = commands.resourceCommand;

  // The resource-watcher only supports a single call to watch/unwatch per
  // instance, so we attach a unique watch callback, which will forward the
  // resource to `handlePlatformMessage`, dynamically updated for each command.
  let handlePlatformMessage = function() {};

  const onPlatformMessageAvailable = resources => {
    for (const resource of resources) {
      handlePlatformMessage(resource);
    }
  };
  await resourceWatcher.watchResources(
    [resourceWatcher.TYPES.PLATFORM_MESSAGE],
    {
      onAvailable: onPlatformMessageAvailable,
    }
  );

  for (const [key, string] of getPlatformMessages()) {
    const onPlatformMessage = new Promise(resolve => {
      handlePlatformMessage = resolve;
    });

    Services.console.logStringMessage(string);

    const packet = await onPlatformMessage;
    stubs.set(key, getCleanedPacket(key, packet));
  }

  await commands.destroy();

  return stubs;
}

function getPlatformMessages() {
  return new Map([
    ["platform-simple-message", "foobar test"],
    ["platform-longString-message", `a\n${"a".repeat(20000)}`],
  ]);
}
