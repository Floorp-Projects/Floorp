const {AddonManagerPrivate} = Cu.import("resource://gre/modules/AddonManager.jsm", {});

const ID_PERMS = "update_perms@tests.mozilla.org";
const ID_LEGACY = "legacy_update@tests.mozilla.org";

function getBadgeStatus() {
  let menuButton = document.getElementById("PanelUI-menu-button");
  return menuButton.getAttribute("badge-status");
}

// Set some prefs that apply to all the tests in this file
add_task(function* setup() {
  yield SpecialPowers.pushPrefEnv({set: [
    // We don't have pre-pinned certificates for the local mochitest server
    ["extensions.install.requireBuiltInCerts", false],
    ["extensions.update.requireBuiltInCerts", false],
  ]});

  // Navigate away from the initial page so that about:addons always
  // opens in a new tab during tests
  gBrowser.selectedBrowser.loadURI("about:robots");
  yield BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);

  registerCleanupFunction(function*() {
    // Return to about:blank when we're done
    gBrowser.selectedBrowser.loadURI("about:blank");
    yield BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
  });
});

hookExtensionsTelemetry();

// Helper function to test an upgrade that should not show a prompt
async function testNoPrompt(origUrl, id) {
  await SpecialPowers.pushPrefEnv({set: [
    // Turn on background updates
    ["extensions.update.enabled", true],

    // Point updates to the local mochitest server
    ["extensions.update.background.url", `${BASE}/browser_webext_update.json`],
  ]});

  // Install version 1.0 of the test extension
  let addon = await promiseInstallAddon(origUrl);

  ok(addon, "Addon was installed");

  let sawPopup = false;
  PopupNotifications.panel.addEventListener("popupshown",
                                            () => sawPopup = true,
                                            {once: true});

  // Trigger an update check and wait for the update to be applied.
  let updatePromise = waitForUpdate(addon);
  AddonManagerPrivate.backgroundUpdateCheck();
  await updatePromise;

  // There should be no notifications about the update
  is(getBadgeStatus(), "", "Should not have addon alert badge");

  await PanelUI.show();
  let addons = document.getElementById("PanelUI-footer-addons");
  is(addons.children.length, 0, "Have 0 updates in the PanelUI menu");
  await PanelUI.hide();

  ok(!sawPopup, "Should not have seen permissions notification");

  addon = await AddonManager.getAddonByID(id);
  is(addon.version, "2.0", "Update should have applied");

  addon.uninstall();
  await SpecialPowers.popPrefEnv();
}

// Test that an update that adds new non-promptable permissions is just
// applied without showing a notification dialog.
add_task(() => testNoPrompt(`${BASE}/browser_webext_update_perms1.xpi`,
                            ID_PERMS));

// Test that an update from a legacy extension to a webextension
// doesn't show a prompt even when the webextension uses
// promptable required permissions.
add_task(() => testNoPrompt(`${BASE}/browser_legacy.xpi`, ID_LEGACY));
