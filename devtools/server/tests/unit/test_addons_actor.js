/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow */

"use strict";

startupAddonsManager();

async function connect() {
  DebuggerServer.init();
  DebuggerServer.registerAllActors();

  const client = new DebuggerClient(DebuggerServer.connectPipe());
  await client.connect();

  const addons = await client.mainRoot.getFront("addons");
  return [client, addons];
}

add_task(async function testSuccessfulInstall() {
  const [client, addons] = await connect();

  const allowMissing = false;
  const usePlatformSeparator = true;
  const addonPath = getFilePath(
    "addons/web-extension",
    allowMissing,
    usePlatformSeparator
  );
  const installedAddon = await addons.installTemporaryAddon(addonPath);
  equal(installedAddon.id, "test-addons-actor@mozilla.org");
  // The returned object is currently not a proper actor.
  equal(installedAddon.actor, false);

  const addonList = await client.mainRoot.listAddons();
  ok(addonList && addonList.map(a => a.name), "Received list of add-ons");
  const addon = addonList.find(a => a.id === installedAddon.id);
  ok(addon, "Test add-on appeared in root install list");

  await close(client);
});

add_task(async function testNonExistantPath() {
  const [client, addons] = await connect();

  await Assert.rejects(
    addons.installTemporaryAddon("some-non-existant-path"),
    /Could not install add-on.*Component returned failure/
  );

  await close(client);
});
