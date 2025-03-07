/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const { ExtensionPermissions } = ChromeUtils.importESModule(
  "resource://gre/modules/ExtensionPermissions.sys.mjs"
);

const ADDON_ID = "policytest@mozilla.com";
const BASE_URL =
  "http://mochi.test:8888/browser/browser/components/enterprisepolicies/tests/browser";

async function isExtensionLockedAndUpdateDisabled(win, addonID) {
  let addonCard = await BrowserTestUtils.waitForCondition(() => {
    return win.document.querySelector(`addon-card[addon-id="${addonID}"]`);
  }, `Get addon-card for "${addonID}"`);
  let disableBtn = addonCard.querySelector('[action="toggle-disabled"]');
  let removeBtn = addonCard.querySelector('panel-item[action="remove"]');
  ok(removeBtn.disabled, "Remove button should be disabled");
  ok(disableBtn.hidden, "Disable button should be hidden");
  let updateRow = addonCard.querySelector(".addon-detail-row-updates");
  is(updateRow.hidden, true, "Update row should be hidden");
}

add_task(async function test_addon_private_browser_access_locked() {
  async function installWithExtensionSettings(extensionSettings = {}) {
    let installPromise = waitForAddonInstall(ADDON_ID);
    await setupPolicyEngineWithJson({
      policies: {
        ExtensionSettings: {
          "policytest@mozilla.com": {
            install_url: `${BASE_URL}/policytest_v0.1.xpi`,
            installation_mode: "force_installed",
            updates_disabled: true,
            ...extensionSettings,
          },
        },
      },
    });
    await installPromise;
    let addon = await AddonManager.getAddonByID(ADDON_ID);
    isnot(addon, null, "Addon not installed.");
    is(addon.version, "0.1", "Addon version is correct");
    return addon;
  }

  let addon = await installWithExtensionSettings();
  is(
    Boolean(
      addon.permissions & AddonManager.PERM_CAN_CHANGE_PRIVATEBROWSING_ACCESS
    ),
    true,
    "Addon should be able to change private browsing setting (not set in policy)."
  );
  await addon.uninstall();

  addon = await installWithExtensionSettings({ private_browsing: true });
  is(
    Boolean(
      addon.permissions & AddonManager.PERM_CAN_CHANGE_PRIVATEBROWSING_ACCESS
    ),
    false,
    "Addon should NOT be able to change private browsing setting (set to true in policy)."
  );
  await addon.uninstall();

  addon = await installWithExtensionSettings({ private_browsing: false });
  is(
    Boolean(
      addon.permissions & AddonManager.PERM_CAN_CHANGE_PRIVATEBROWSING_ACCESS
    ),
    false,
    "Addon should NOT be able to change private browsing setting (set to false in policy)."
  );
  await addon.uninstall();
});

add_task(async function test_addon_install() {
  let installPromise = waitForAddonInstall(ADDON_ID);
  await setupPolicyEngineWithJson({
    policies: {
      ExtensionSettings: {
        "policytest@mozilla.com": {
          install_url: `${BASE_URL}/policytest_v0.1.xpi`,
          installation_mode: "force_installed",
          updates_disabled: true,
        },
      },
    },
  });
  await installPromise;
  let addon = await AddonManager.getAddonByID(ADDON_ID);
  isnot(addon, null, "Addon not installed.");
  is(addon.version, "0.1", "Addon version is correct");

  Assert.deepEqual(
    addon.installTelemetryInfo,
    { source: "enterprise-policy" },
    "Got the expected addon.installTelemetryInfo"
  );
});

add_task(async function test_addon_locked_update_disabled() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);
  const win = await BrowserAddonUI.openAddonsMgr(
    "addons://detail/" + encodeURIComponent(ADDON_ID)
  );

  await isExtensionLockedAndUpdateDisabled(win, ADDON_ID);

  BrowserTestUtils.removeTab(tab);
});

add_task(async function test_addon_uninstall() {
  let uninstallPromise = waitForAddonUninstall(ADDON_ID);
  await setupPolicyEngineWithJson({
    policies: {
      ExtensionSettings: {
        "policytest@mozilla.com": {
          installation_mode: "blocked",
        },
      },
    },
  });
  await uninstallPromise;
  let addon = await AddonManager.getAddonByID(ADDON_ID);
  is(addon, null, "Addon should be uninstalled.");
});
