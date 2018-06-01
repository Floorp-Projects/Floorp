/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow */

"use strict";

const {AddonManager} = require("resource://gre/modules/AddonManager.jsm");

startupAddonsManager();

function promiseAddonEvent(event) {
  return new Promise(resolve => {
    const listener = {
      [event]: function(...args) {
        AddonManager.removeAddonListener(listener);
        resolve(args);
      }
    };

    AddonManager.addAddonListener(listener);
  });
}

function promiseWebExtensionStartup() {
  const {Management} = ChromeUtils.import("resource://gre/modules/Extension.jsm", {});

  return new Promise(resolve => {
    const listener = (evt, extension) => {
      Management.off("ready", listener);
      resolve(extension);
    };

    Management.on("ready", listener);
  });
}

async function findAddonInRootList(client, addonId) {
  const result = await client.listAddons();
  const addonActor = result.addons.filter(addon => addon.id === addonId)[0];
  ok(addonActor, `Found add-on actor for ${addonId}`);
  return addonActor;
}

async function reloadAddon(client, addonActor) {
  // The add-on will be re-installed after a successful reload.
  const onInstalled = promiseAddonEvent("onInstalled");
  await client.request({to: addonActor.actor, type: "reload"});
  await onInstalled;
}

function getSupportFile(path) {
  const allowMissing = false;
  return do_get_file(path, allowMissing);
}

add_task(async function testReloadExitedAddon() {
  const client = await new Promise(resolve => {
    get_chrome_actors(client => resolve(client));
  });

  // Install our main add-on to trigger reloads on.
  const addonFile = getSupportFile("addons/web-extension");
  const [installedAddon] = await Promise.all([
    AddonManager.installTemporaryAddon(addonFile),
    promiseWebExtensionStartup(),
  ]);

  // Install a decoy add-on.
  const addonFile2 = getSupportFile("addons/web-extension2");
  const [installedAddon2] = await Promise.all([
    AddonManager.installTemporaryAddon(addonFile2),
    promiseWebExtensionStartup(),
  ]);

  const addonActor = await findAddonInRootList(client, installedAddon.id);

  await Promise.all([
    reloadAddon(client, addonActor),
    promiseWebExtensionStartup(),
  ]);

  // Uninstall the decoy add-on, which should cause its actor to exit.
  const onUninstalled = promiseAddonEvent("onUninstalled");
  installedAddon2.uninstall();
  await onUninstalled;

  // Try to re-list all add-ons after a reload.
  // This was throwing an exception because of the exited actor.
  const newAddonActor = await findAddonInRootList(client, installedAddon.id);
  equal(newAddonActor.id, addonActor.id);

  // The actor id should be the same after the reload
  equal(newAddonActor.actor, addonActor.actor);

  const onAddonListChanged = new Promise((resolve) => {
    client.addListener("addonListChanged", function listener() {
      client.removeListener("addonListChanged", listener);
      resolve();
    });
  });

  // Install an upgrade version of the first add-on.
  const addonUpgradeFile = getSupportFile("addons/web-extension-upgrade");
  const [upgradedAddon] = await Promise.all([
    AddonManager.installTemporaryAddon(addonUpgradeFile),
    promiseWebExtensionStartup(),
  ]);

  // Waiting for addonListChanged unsolicited event
  await onAddonListChanged;

  // re-list all add-ons after an upgrade.
  const upgradedAddonActor = await findAddonInRootList(client, upgradedAddon.id);
  equal(upgradedAddonActor.id, addonActor.id);
  // The actor id should be the same after the upgrade.
  equal(upgradedAddonActor.actor, addonActor.actor);

  // The addon metadata has been updated.
  equal(upgradedAddonActor.name, "Test Addons Actor Upgrade");

  await close(client);
});
