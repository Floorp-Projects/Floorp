/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow */

"use strict";

const { AddonManager } = require("resource://gre/modules/AddonManager.jsm");

startupAddonsManager();

add_task(async function testReloadExitedAddon() {
  DebuggerServer.init();
  DebuggerServer.registerAllActors();

  const client = new DebuggerClient(DebuggerServer.connectPipe());
  await client.connect();

  // Retrieve the current list of addons to be notified of the next list update.
  // We will also call listAddons every time we receive the event "addonListChanged" for
  // the same reason.
  await client.mainRoot.listAddons();

  info("Install the addon");
  const addonFile = do_get_file("addons/web-extension", false);

  let installedAddon;
  await expectAddonListChanged(client, async () => {
    installedAddon = await AddonManager.installTemporaryAddon(addonFile);
  });
  ok(true, "Received onAddonListChanged when installing addon");

  info("Disable the addon");
  await expectAddonListChanged(client, () => installedAddon.disable());
  ok(true, "Received onAddonListChanged when disabling addon");

  info("Enable the addon");
  await expectAddonListChanged(client, () => installedAddon.enable());
  ok(true, "Received onAddonListChanged when enabling addon");

  info("Put the addon in pending uninstall mode");
  await expectAddonListChanged(client, () => installedAddon.uninstall(true));
  ok(true, "Received onAddonListChanged when addon moves to pending uninstall");

  info("Cancel uninstall for addon");
  await expectAddonListChanged(client, () => installedAddon.cancelUninstall());
  ok(true, "Received onAddonListChanged when addon uninstall is canceled");

  info("Completely uninstall the addon");
  await expectAddonListChanged(client, () => installedAddon.uninstall());
  ok(true, "Received onAddonListChanged when addon is uninstalled");

  await close(client);
});

async function expectAddonListChanged(client, predicate) {
  const onAddonListChanged = client.mainRoot.once("addonListChanged");
  await predicate();
  await onAddonListChanged;
  await client.mainRoot.listAddons();
}
