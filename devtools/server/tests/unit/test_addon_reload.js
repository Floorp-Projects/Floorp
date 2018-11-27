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
      },
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

async function reloadAddon(addonTargetFront) {
  // The add-on will be re-installed after a successful reload.
  const onInstalled = promiseAddonEvent("onInstalled");
  await addonTargetFront.reload();
  await onInstalled;
}

function getSupportFile(path) {
  const allowMissing = false;
  return do_get_file(path, allowMissing);
}

add_task(async function testReloadExitedAddon() {
  DebuggerServer.init();
  DebuggerServer.registerAllActors();

  const client = new DebuggerClient(DebuggerServer.connectPipe());
  await client.connect();

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

  const addonTargetFront = await client.mainRoot.getAddon({ id: installedAddon.id });

  await Promise.all([
    reloadAddon(addonTargetFront),
    promiseWebExtensionStartup(),
  ]);

  // Uninstall the decoy add-on, which should cause its actor to exit.
  const onUninstalled = promiseAddonEvent("onUninstalled");
  installedAddon2.uninstall();
  await onUninstalled;

  // Try to re-list all add-ons after a reload.
  // This was throwing an exception because of the exited actor.
  const newAddonFront = await client.mainRoot.getAddon({ id: installedAddon.id });
  equal(newAddonFront.id, addonTargetFront.id);

  // The fronts should be the same after the reload
  equal(newAddonFront, addonTargetFront);

  const onAddonListChanged = client.mainRoot.once("addonListChanged");

  // Install an upgrade version of the first add-on.
  const addonUpgradeFile = getSupportFile("addons/web-extension-upgrade");
  const [upgradedAddon] = await Promise.all([
    AddonManager.installTemporaryAddon(addonUpgradeFile),
    promiseWebExtensionStartup(),
  ]);

  // Waiting for addonListChanged unsolicited event
  await onAddonListChanged;

  // re-list all add-ons after an upgrade.
  const upgradedAddonFront = await client.mainRoot.getAddon({ id: upgradedAddon.id });
  equal(upgradedAddonFront.id, addonTargetFront.id);
  // The fronts should be the same after the upgrade.
  equal(upgradedAddonFront, addonTargetFront);

  // The addon metadata has been updated.
  equal(upgradedAddonFront.name, "Test Addons Actor Upgrade");

  await close(client);
});
