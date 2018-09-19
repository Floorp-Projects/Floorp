
// Set some prefs that apply to all the tests in this file
add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({set: [
    // We don't have pre-pinned certificates for the local mochitest server
    ["extensions.install.requireBuiltInCerts", false],
    ["extensions.update.requireBuiltInCerts", false],

    // Don't require the extensions to be signed
    ["xpinstall.signatures.required", false],

    // Point updates to the local mochitest server
    ["extensions.update.url", `${BASE}/browser_webext_update.json`],
  ]});
});

// Helper to test that an update of a given extension does not
// generate any permission prompts.
async function testUpdateNoPrompt(filename, id,
                                  initialVersion = "1.0", updateVersion = "2.0") {
  // Navigate away to ensure that BrowserOpenAddonMgr() opens a new tab
  gBrowser.selectedBrowser.loadURI("about:robots");
  await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);

  // Install initial version of the test extension
  let addon = await promiseInstallAddon(`${BASE}/${filename}`);
  ok(addon, "Addon was installed");
  is(addon.version, initialVersion, "Version 1 of the addon is installed");

  // Go to Extensions in about:addons
  let win = await BrowserOpenAddonsMgr("addons://list/extension");

  let sawPopup = false;
  function popupListener() {
    sawPopup = true;
  }
  PopupNotifications.panel.addEventListener("popupshown", popupListener);

  // Trigger an update check, we should see the update get applied
  let updatePromise = waitForUpdate(addon);
  win.gViewController.doCommand("cmd_findAllUpdates");
  await updatePromise;

  addon = await AddonManager.getAddonByID(id);
  is(addon.version, updateVersion, "Should have upgraded");

  ok(!sawPopup, "Should not have seen a permission notification");
  PopupNotifications.panel.removeEventListener("popupshown", popupListener);

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
  await addon.uninstall();
}

// Test that we don't see a prompt when updating from a legacy
// extension to a webextension.
add_task(() => testUpdateNoPrompt("browser_legacy.xpi",
                                  "legacy_update@tests.mozilla.org", "1.1"));

// Test that we don't see a prompt when no new promptable permissions
// are added.
add_task(() => testUpdateNoPrompt("browser_webext_update_perms1.xpi",
                                  "update_perms@tests.mozilla.org"));

// Test that an update that narrows origin permissions is just applied without
// showing a notification promt
add_task(() => testUpdateNoPrompt("browser_webext_update_origins1.xpi",
                                  "update_origins@tests.mozilla.org"));
