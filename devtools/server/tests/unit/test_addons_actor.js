/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow */

"use strict";

const {AddonsFront} = require("devtools/shared/fronts/addon/addons");

startupAddonsManager();

async function connect() {
  const client = await new Promise(resolve => {
    get_parent_process_actors(client => resolve(client));
  });
  const root = await listTabs(client);
  const addonsActor = root.addonsActor;
  ok(addonsActor, "Got AddonsActor instance");

  const addons = AddonsFront(client, {addonsActor});
  return [client, addons];
}

add_task(async function testSuccessfulInstall() {
  const [client, addons] = await connect();

  const allowMissing = false;
  const usePlatformSeparator = true;
  const addonPath = getFilePath("addons/web-extension",
                                allowMissing, usePlatformSeparator);
  const installedAddon = await addons.installTemporaryAddon(addonPath);
  equal(installedAddon.id, "test-addons-actor@mozilla.org");
  // The returned object is currently not a proper actor.
  equal(installedAddon.actor, false);

  const addonList = await client.listAddons();
  ok(addonList && addonList.addons && addonList.addons.map(a => a.name),
     "Received list of add-ons");
  const addon = addonList.addons.filter(a => a.id === installedAddon.id)[0];
  ok(addon, "Test add-on appeared in root install list");

  await close(client);
});

add_task(async function testNonExistantPath() {
  const [client, addons] = await connect();

  await Assert.rejects(
    addons.installTemporaryAddon("some-non-existant-path"),
    /Could not install add-on.*Component returned failure/);

  await close(client);
});
