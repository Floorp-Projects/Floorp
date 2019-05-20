/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const {AddonTestUtils} = ChromeUtils.import("resource://testing-common/AddonTestUtils.jsm");
const {AddonManager} = ChromeUtils.import("resource://gre/modules/AddonManager.jsm");

AddonTestUtils.init(this);
AddonTestUtils.overrideCertDB();
AddonTestUtils.appInfo = getAppInfo();

const server = AddonTestUtils.createHttpServer({hosts: ["example.com"]});
const BASE_URL = `http://example.com/data`;

let addonID = "policytest2@mozilla.com";

add_task(async function setup() {
  await AddonTestUtils.promiseStartupManager();

  let webExtensionFile = AddonTestUtils.createTempWebExtensionFile({
    manifest: {
      applications: {
        gecko: {
          id: addonID,
        },
      },
    },
  });

  server.registerFile("/data/policy_test.xpi", webExtensionFile);
});

add_task(async function test_extensionsettings() {
  await setupPolicyEngineWithJson({
    "policies": {
      "ExtensionSettings": {
        "extension1@mozilla.com": {
          "blocked_install_message": "Extension1 error message.",
        },
        "*": {
          "blocked_install_message": "Generic error message.",
        },
      },
    },
  });

  let extensionSettings =  Services.policies.getExtensionSettings("extension1@mozilla.com");
  equal(extensionSettings.blocked_install_message, "Extension1 error message.", "Should have extension specific message.");
  extensionSettings =  Services.policies.getExtensionSettings("extension2@mozilla.com");
  equal(extensionSettings.blocked_install_message, "Generic error message.", "Should have generic message.");
});

add_task(async function test_addon_blocked() {
  await setupPolicyEngineWithJson({
    "policies": {
      "ExtensionSettings": {
        "policytest2@mozilla.com": {
          "installation_mode": "blocked",
        },
      },
    },
  });

  let install = await AddonManager.getInstallForURL(BASE_URL + "/policy_test.xpi");
  await install.install();
  notEqual(install.addon, null, "Addon should not be null");
  equal(install.addon.appDisabled, true, "Addon should be disabled");
  await install.addon.uninstall();
});

add_task(async function test_addon_allowed() {
  await setupPolicyEngineWithJson({
    "policies": {
      "ExtensionSettings": {
        "policytest2@mozilla.com": {
          "installation_mode": "allowed",
        },
        "*": {
          "installation_mode": "blocked",
        },
      },
    },
  });

  let install = await AddonManager.getInstallForURL(BASE_URL + "/policy_test.xpi");
  await install.install();
  notEqual(install.addon, null, "Addon should not be null");
  equal(install.addon.appDisabled, false, "Addon should not be disabled");
  await install.addon.uninstall();
});

add_task(async function test_addon_uninstalled() {
  let install = await AddonManager.getInstallForURL(BASE_URL + "/policy_test.xpi");
  await install.install();
  notEqual(install.addon, null, "Addon should not be null");

  await Promise.all([
    AddonTestUtils.promiseAddonEvent("onUninstalled"),
    setupPolicyEngineWithJson({
      "policies": {
        "ExtensionSettings": {
          "*": {
            "installation_mode": "blocked",
          },
        },
      },
    }),
  ]);
  let addon = await AddonManager.getAddonByID(addonID);
  equal(addon, null, "Addon should be null");
});

add_task(async function test_addon_forceinstalled() {
  await Promise.all([
    AddonTestUtils.promiseInstallEvent("onInstallEnded"),
    setupPolicyEngineWithJson({
      "policies": {
        "ExtensionSettings": {
          "policytest2@mozilla.com": {
            "installation_mode": "force_installed",
            "install_url": BASE_URL + "/policy_test.xpi",
          },
        },
      },
    }),
  ]);
  let addon = await AddonManager.getAddonByID(addonID);
  notEqual(addon, null, "Addon should not be null");
  equal(addon.appDisabled, false, "Addon should not be disabled");
  equal(addon.permissions & AddonManager.PERM_CAN_UNINSTALL, 0, "Addon should not be able to be uninstalled.");
  equal(addon.permissions & AddonManager.PERM_CAN_DISABLE, 0, "Addon should not be able to be disabled.");
  await addon.uninstall();
});

add_task(async function test_addon_normalinstalled() {
  await Promise.all([
    AddonTestUtils.promiseInstallEvent("onInstallEnded"),
    setupPolicyEngineWithJson({
      "policies": {
        "ExtensionSettings": {
          "policytest2@mozilla.com": {
            "installation_mode": "normal_installed",
            "install_url": BASE_URL + "/policy_test.xpi",
          },
        },
      },
    }),
  ]);
  let addon = await AddonManager.getAddonByID(addonID);
  notEqual(addon, null, "Addon should not be null");
  equal(addon.appDisabled, false, "Addon should not be disabled");
  equal(addon.permissions & AddonManager.PERM_CAN_UNINSTALL, 0, "Addon should not be able to be uninstalled.");
  notEqual(addon.permissions & AddonManager.PERM_CAN_DISABLE, 0, "Addon should be able to be disabled.");
  await addon.uninstall();
});
