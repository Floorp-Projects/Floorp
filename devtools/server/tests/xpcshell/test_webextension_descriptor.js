/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { ExtensionTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/ExtensionXPCShellUtils.sys.mjs"
);

const DistinctDevToolsServer = getDistinctDevToolsServer();
ExtensionTestUtils.init(this);

add_setup(async () => {
  Services.prefs.setBoolPref("extensions.blocklist.enabled", false);
  await startupAddonsManager();

  // We intentionally generate install-time manifest warnings, so don't trigger
  // the special test-only mode of converting them to errors.
  Services.prefs.setBoolPref(
    "extensions.webextensions.warnings-as-errors",
    false
  );

  DistinctDevToolsServer.init();
  DistinctDevToolsServer.registerAllActors();
});

// Verifies:
// - listAddons
// - WebExtensionDescriptorActor output
// Also a regression test for bug 1837185, that AddonManager.sys.mjs and
// ExtensionParent.sys.mjs are imported from the correct loader.
add_task(async function test_listAddons_and_WebExtensionDescriptor() {
  const transport = DistinctDevToolsServer.connectPipe();
  const client = new DevToolsClient(transport);
  await client.connect();

  const getRootResponse = await client.mainRoot.getRoot();

  ok(getRootResponse, "received a response after calling RootActor::getRoot");
  ok(getRootResponse.addonsActor, "getRoot returned an addonsActor id");

  const ADDON_ID = "with@warning";
  const extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "permanent",
    manifest: {
      name: "DummyExtensionWithUnknownManifestKey",
      unknown_manifest_key: "this is an unknown manifest key",
      browser_specific_settings: { gecko: { id: ADDON_ID } },
    },
    background: `browser.test.sendMessage("background_started");`,
  });
  await extension.startup();
  await extension.awaitMessage("background_started");

  // listAddons: addon after new install.
  {
    const listAddonsResponse = await client.mainRoot.listAddons();
    const addon = listAddonsResponse.find(a => a.id === ADDON_ID);
    ok(addon, "listAddons() returns a list of add-ons including with@warning");

    // Inspect all raw properties of the message, to make sure that we always
    // have full coverage for all current and future properties.
    const { actor, url, warnings, ...addonMinusSomeKeys } = addon._form;
    const actorPattern = /^server\d+\.conn\d+\.webExtensionDescriptor\d+$/;
    ok(actorPattern.test(actor), `actor is webExtensionDescriptor: ${actor}`);
    // We don't care about the exact path, just a dummy check:
    ok(url.endsWith(".xpi"), `url is path to the xpi file`);

    deepEqual(
      warnings,
      [
        "Reading manifest: Warning processing unknown_manifest_key: An unexpected property was found in the WebExtension manifest.",
      ],
      "Can retrieve warnings."
    );

    // Verify that the other remaining keys have a meaningful value.
    // This is mainly to have some form of verification on the value of the
    // properties. If this check ever fails, double-check whether the proposed
    // change makes sense and if it does just update the test expectation here.
    deepEqual(
      addonMinusSomeKeys,
      {
        backgroundScriptStatus: undefined,
        debuggable: true,
        hidden: false,
        iconDataURL: undefined,
        iconURL: null,
        id: ADDON_ID,
        isSystem: false,
        isWebExtension: true,
        manifestURL: `moz-extension://${extension.uuid}/manifest.json`,
        name: "DummyExtensionWithUnknownManifestKey",
        persistentBackgroundScript: true,
        temporarilyInstalled: false,
        traits: {
          supportsReloadDescriptor: true,
          watcher: true,
        },
      },
      "WebExtensionDescriptorActor content matches the add-on"
    );
  }

  await extension.upgrade({
    manifest: {
      name: "Updated_extension",
      new_unknown_manifest_key: "different warning than before",
      browser_specific_settings: { gecko: { id: ADDON_ID } },
    },
    background: `browser.test.sendMessage("updated_done");`,
  });
  await extension.awaitMessage("updated_done");

  // listAddons: addon after update.
  {
    const listAddonsResponse = await client.mainRoot.listAddons();
    const addon = listAddonsResponse.find(a => a.id === ADDON_ID);
    ok(addon, "listAddons() should still list the add-on after update");
    equal(addon.name, "Updated_extension", "Got updated name");
    deepEqual(
      addon.warnings,
      [
        "Reading manifest: Warning processing new_unknown_manifest_key: An unexpected property was found in the WebExtension manifest.",
      ],
      "Can retrieve new warnings for updated add-on."
    );
  }

  await extension.unload();

  // listAddons: addon after removal - gone.
  {
    const listAddonsResponse = await client.mainRoot.listAddons();
    const addon = listAddonsResponse.find(a => a.id === ADDON_ID);
    deepEqual(addon, null, "Add-on should be gone after removal");
  }

  await client.close();
});
