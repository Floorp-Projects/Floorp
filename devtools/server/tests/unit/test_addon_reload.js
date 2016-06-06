/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const protocol = require("devtools/shared/protocol");
const {AddonManager} = require("resource://gre/modules/AddonManager.jsm");

startupAddonsManager();

function promiseAddonEvent(event) {
  return new Promise(resolve => {
    let listener = {
      [event]: function (...args) {
        AddonManager.removeAddonListener(listener);
        resolve(args);
      }
    };

    AddonManager.addAddonListener(listener);
  });
}

function* findAddonInRootList(client, addonId) {
  const result = yield client.listAddons();
  const addonActor = result.addons.filter(addon => addon.id === addonId)[0];
  ok(addonActor, `Found add-on actor for ${addonId}`);
  return addonActor;
}

function* reloadAddon(client, addonActor) {
  // The add-on will be re-installed after a successful reload.
  const onInstalled = promiseAddonEvent("onInstalled");
  yield client.request({to: addonActor.actor, type: "reload"});
  yield onInstalled;
}

function getSupportFile(path) {
  const allowMissing = false;
  return do_get_file(path, allowMissing);
}

add_task(function* testReloadExitedAddon() {
  const client = yield new Promise(resolve => {
    get_chrome_actors(client => resolve(client));
  });

  // Install our main add-on to trigger reloads on.
  const addonFile = getSupportFile("addons/web-extension");
  const installedAddon = yield AddonManager.installTemporaryAddon(
    addonFile);

  // Install a decoy add-on.
  const addonFile2 = getSupportFile("addons/web-extension2");
  const installedAddon2 = yield AddonManager.installTemporaryAddon(
    addonFile2);

  let addonActor = yield findAddonInRootList(client, installedAddon.id);

  yield reloadAddon(client, addonActor);

  // Uninstall the decoy add-on, which should cause its actor to exit.
  const onUninstalled = promiseAddonEvent("onUninstalled");
  installedAddon2.uninstall();
  const [uninstalledAddon] = yield onUninstalled;

  // Try to re-list all add-ons after a reload.
  // This was throwing an exception because of the exited actor.
  const newAddonActor = yield findAddonInRootList(client, installedAddon.id);
  equal(newAddonActor.id, addonActor.id);

  yield close(client);
});
