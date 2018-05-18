/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* global sinon */

"use strict";

ChromeUtils.defineModuleGetter(this, "ExtensionSettingsStore",
                               "resource://gre/modules/ExtensionSettingsStore.jsm");
ChromeUtils.defineModuleGetter(this, "ExtensionControlledPopup",
                               "resource:///modules/ExtensionControlledPopup.jsm");

/*
 * This function is a unit test for distributions disabling the ExtensionControlledPopup.
 */
add_task(async function testDistributionPopup() {
  let distExtId = "ext-distribution@mochi.test";
  Services.prefs.setCharPref(`extensions.installedDistroAddon.${distExtId}`, true);
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      applications: {gecko: {id: distExtId}},
      name: "Ext Distribution",
    },
  });

  let userExtId = "ext-user@mochi.test";
  let userExtension = ExtensionTestUtils.loadExtension({
    manifest: {
      applications: {gecko: {id: userExtId}},
      name: "Ext User Installed",
    },
  });

  await extension.startup();
  await userExtension.startup();
  await ExtensionSettingsStore.initialize();

  let confirmedType = "extension-controlled-confirmed";
  equal(new ExtensionControlledPopup({confirmedType}).userHasConfirmed(distExtId),
        true, "The popup has been disabled.");

  equal(new ExtensionControlledPopup({confirmedType}).userHasConfirmed(userExtId),
        false, "The popup has not been disabled.");

  await extension.unload();
  await userExtension.unload();
});
