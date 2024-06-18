/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const { AddonTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/AddonTestUtils.sys.mjs"
);
const { AddonManager } = ChromeUtils.importESModule(
  "resource://gre/modules/AddonManager.sys.mjs"
);
const { ExtensionTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/ExtensionXPCShellUtils.sys.mjs"
);

AddonTestUtils.init(this);
AddonTestUtils.overrideCertDB();
AddonTestUtils.appInfo = getAppInfo();
ExtensionTestUtils.init(this);

const server = AddonTestUtils.createHttpServer({ hosts: ["example.com"] });
const BASE_URL = `http://example.com/data`;

let addonID = "policytest2@mozilla.com";
let themeID = "policytheme@mozilla.com";
let policyOnlyID = "policy_installed_only@mozilla.com";

let fileURL;

async function assertManagementAPIInstallType(addonId, expectedInstallType) {
  const addon = await AddonManager.getAddonByID(addonId);
  const expectInstalledByPolicy = expectedInstallType === "admin";
  equal(
    addon.isInstalledByEnterprisePolicy,
    expectInstalledByPolicy,
    `Addon should ${
      expectInstalledByPolicy ? "be" : "NOT be"
    } marked as installed by enterprise policy`
  );
  const policy = WebExtensionPolicy.getByID(addonId);
  const pageURL = policy.extension.baseURI.resolve(
    "_generated_background_page.html"
  );
  const page = await ExtensionTestUtils.loadContentPage(pageURL);
  const { id, installType } = await page.spawn([], async () => {
    const res = await this.content.wrappedJSObject.browser.management.getSelf();
    return { id: res.id, installType: res.installType };
  });
  await page.close();
  Assert.equal(id, addonId, "Got results for the expected addon id");
  Assert.equal(
    installType,
    expectedInstallType,
    "Got the expected installType on policy installed extension"
  );
}

function waitForAddonInstall(addonId) {
  return new Promise(resolve => {
    let listener = {
      onInstallEnded(install, addon) {
        if (addon.id == addonId) {
          AddonManager.removeInstallListener(listener);
          resolve();
        }
      },
      onDownloadFailed() {
        AddonManager.removeInstallListener(listener);
        resolve();
      },
      onInstallFailed() {
        AddonManager.removeInstallListener(listener);
        resolve();
      },
    };
    AddonManager.addInstallListener(listener);
  });
}

add_setup(async function setup() {
  await AddonTestUtils.promiseStartupManager();

  let webExtensionFile = AddonTestUtils.createTempWebExtensionFile({
    manifest: {
      browser_specific_settings: {
        gecko: {
          id: addonID,
        },
      },
    },
  });
  server.registerFile("/data/policy_test.xpi", webExtensionFile);
  fileURL = Services.io
    .newFileURI(webExtensionFile)
    .QueryInterface(Ci.nsIFileURL);

  let policyOnlyExtensionFile = AddonTestUtils.createTempWebExtensionFile({
    manifest: {
      name: "Enterprise Policy Only Test Extension",
      browser_specific_settings: {
        gecko: {
          id: policyOnlyID,
          admin_install_only: true,
        },
      },
    },
  });
  server.registerFile(
    "/data/policy_installed_only.xpi",
    policyOnlyExtensionFile
  );

  server.registerFile(
    "/data/amosigned-sha1only.xpi",
    do_get_file("amosigned-sha1only.xpi")
  );
});

add_task(async function test_extensionsettings() {
  await setupPolicyEngineWithJson({
    policies: {
      ExtensionSettings: {
        "extension1@mozilla.com": {
          blocked_install_message: "Extension1 error message.",
        },
        "*": {
          blocked_install_message: "Generic error message.",
        },
      },
    },
  });

  let extensionSettings = Services.policies.getExtensionSettings(
    "extension1@mozilla.com"
  );
  equal(
    extensionSettings.blocked_install_message,
    "Extension1 error message.",
    "Should have extension specific message."
  );
  extensionSettings = Services.policies.getExtensionSettings(
    "extension2@mozilla.com"
  );
  equal(
    extensionSettings.blocked_install_message,
    "Generic error message.",
    "Should have generic message."
  );
});

add_task(async function test_addon_blocked() {
  await setupPolicyEngineWithJson({
    policies: {
      ExtensionSettings: {
        "policytest2@mozilla.com": {
          installation_mode: "blocked",
        },
      },
    },
  });

  let install = await AddonManager.getInstallForURL(
    BASE_URL + "/policy_test.xpi"
  );
  await install.install();
  notEqual(install.addon, null, "Addon should not be null");
  equal(install.addon.appDisabled, true, "Addon should be disabled");
  await install.addon.uninstall();
});

add_task(async function test_addon_allowed() {
  await setupPolicyEngineWithJson({
    policies: {
      ExtensionSettings: {
        "policytest2@mozilla.com": {
          installation_mode: "allowed",
        },
        "*": {
          installation_mode: "blocked",
        },
      },
    },
  });

  let install = await AddonManager.getInstallForURL(
    BASE_URL + "/policy_test.xpi"
  );
  await install.install();
  notEqual(install.addon, null, "Addon should not be null");
  await assertManagementAPIInstallType(install.addon.id, "normal");
  equal(install.addon.appDisabled, false, "Addon should not be disabled");
  equal(
    install.addon.isInstalledByEnterprisePolicy,
    false,
    "Addon should NOT be marked as installed by enterprise policy"
  );

  await install.addon.uninstall();
});

add_task(async function test_addon_uninstalled() {
  let install = await AddonManager.getInstallForURL(
    BASE_URL + "/policy_test.xpi"
  );
  await install.install();
  notEqual(install.addon, null, "Addon should not be null");

  await Promise.all([
    AddonTestUtils.promiseAddonEvent("onUninstalled"),
    setupPolicyEngineWithJson({
      policies: {
        ExtensionSettings: {
          "*": {
            installation_mode: "blocked",
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
      policies: {
        ExtensionSettings: {
          "policytest2@mozilla.com": {
            installation_mode: "force_installed",
            install_url: BASE_URL + "/policy_test.xpi",
          },
        },
      },
    }),
  ]);
  let addon = await AddonManager.getAddonByID(addonID);
  notEqual(addon, null, "Addon should not be null");
  equal(addon.appDisabled, false, "Addon should not be disabled");
  equal(
    addon.permissions & AddonManager.PERM_CAN_UNINSTALL,
    0,
    "Addon should not be able to be uninstalled."
  );
  equal(
    addon.permissions & AddonManager.PERM_CAN_DISABLE,
    0,
    "Addon should not be able to be disabled."
  );
  await assertManagementAPIInstallType(addon.id, "admin");

  await addon.uninstall();
});

add_task(async function test_addon_normalinstalled() {
  await Promise.all([
    AddonTestUtils.promiseInstallEvent("onInstallEnded"),
    setupPolicyEngineWithJson({
      policies: {
        ExtensionSettings: {
          "policytest2@mozilla.com": {
            installation_mode: "normal_installed",
            install_url: BASE_URL + "/policy_test.xpi",
          },
        },
      },
    }),
  ]);
  let addon = await AddonManager.getAddonByID(addonID);
  notEqual(addon, null, "Addon should not be null");
  equal(addon.appDisabled, false, "Addon should not be disabled");
  equal(
    addon.permissions & AddonManager.PERM_CAN_UNINSTALL,
    0,
    "Addon should not be able to be uninstalled."
  );
  notEqual(
    addon.permissions & AddonManager.PERM_CAN_DISABLE,
    0,
    "Addon should be able to be disabled."
  );
  await assertManagementAPIInstallType(addon.id, "admin");

  await addon.uninstall();
});

add_task(async function test_extensionsettings_string() {
  await setupPolicyEngineWithJson({
    policies: {
      ExtensionSettings: '{"*": {"installation_mode": "blocked"}}',
    },
  });

  let extensionSettings = Services.policies.getExtensionSettings("*");
  equal(extensionSettings.installation_mode, "blocked");
});

add_task(async function test_extensionsettings_string() {
  let restrictedDomains = Services.prefs.getCharPref(
    "extensions.webextensions.restrictedDomains"
  );
  await setupPolicyEngineWithJson({
    policies: {
      ExtensionSettings:
        '{"*": {"restricted_domains": ["example.com","example.org"]}}',
    },
  });

  let newRestrictedDomains = Services.prefs.getCharPref(
    "extensions.webextensions.restrictedDomains"
  );
  equal(newRestrictedDomains, restrictedDomains + ",example.com,example.org");
});

add_task(async function test_theme() {
  let themeFile = AddonTestUtils.createTempWebExtensionFile({
    manifest: {
      browser_specific_settings: {
        gecko: {
          id: themeID,
        },
      },
      theme: {},
    },
  });

  server.registerFile("/data/policy_theme.xpi", themeFile);

  await Promise.all([
    AddonTestUtils.promiseInstallEvent("onInstallEnded"),
    setupPolicyEngineWithJson({
      policies: {
        ExtensionSettings: {
          "policytheme@mozilla.com": {
            installation_mode: "normal_installed",
            install_url: BASE_URL + "/policy_theme.xpi",
          },
        },
      },
    }),
  ]);
  let currentTheme = Services.prefs.getCharPref("extensions.activeThemeID");
  equal(currentTheme, themeID, "Theme should be active");
  let addon = await AddonManager.getAddonByID(themeID);
  await addon.uninstall();
});

add_task(async function test_addon_normalinstalled_file() {
  await Promise.all([
    AddonTestUtils.promiseInstallEvent("onInstallEnded"),
    setupPolicyEngineWithJson({
      policies: {
        ExtensionSettings: {
          "policytest2@mozilla.com": {
            installation_mode: "normal_installed",
            install_url: fileURL.spec,
          },
        },
      },
    }),
  ]);
  let addon = await AddonManager.getAddonByID(addonID);
  notEqual(addon, null, "Addon should not be null");
  equal(addon.appDisabled, false, "Addon should not be disabled");
  equal(
    addon.permissions & AddonManager.PERM_CAN_UNINSTALL,
    0,
    "Addon should not be able to be uninstalled."
  );
  notEqual(
    addon.permissions & AddonManager.PERM_CAN_DISABLE,
    0,
    "Addon should be able to be disabled."
  );
  await assertManagementAPIInstallType(addon.id, "admin");

  await addon.uninstall();
});

add_task(async function test_allow_weak_signatures() {
  // Make sure weak signatures are restricted.
  const resetWeakSignaturePref =
    AddonTestUtils.setWeakSignatureInstallAllowed(false);

  const id = "amosigned-xpi@tests.mozilla.org";
  const perAddonSettings = {
    installation_mode: "normal_installed",
    install_url: BASE_URL + "/amosigned-sha1only.xpi",
  };

  info(
    "Sanity check: expect install to fail if not allowed through enterprise policy settings"
  );
  await Promise.all([
    AddonTestUtils.promiseInstallEvent("onDownloadFailed"),
    setupPolicyEngineWithJson({
      policies: {
        ExtensionSettings: {
          [id]: { ...perAddonSettings },
        },
      },
    }),
  ]);
  let addon = await AddonManager.getAddonByID(id);
  equal(addon, null, "Add-on not installed");

  info(
    "Expect install to be allowed through per-addon enterprise policy settings"
  );
  await Promise.all([
    AddonTestUtils.promiseInstallEvent("onInstallEnded"),
    setupPolicyEngineWithJson({
      policies: {
        ExtensionSettings: {
          [id]: {
            ...perAddonSettings,
            temporarily_allow_weak_signatures: true,
          },
        },
      },
    }),
  ]);
  addon = await AddonManager.getAddonByID(id);
  notEqual(addon, null, "Add-on not installed");
  await addon.uninstall();

  info(
    "Expect install to be allowed through global enterprise policy settings"
  );
  await Promise.all([
    AddonTestUtils.promiseInstallEvent("onInstallEnded"),
    setupPolicyEngineWithJson({
      policies: {
        ExtensionSettings: {
          "*": { temporarily_allow_weak_signatures: true },
          [id]: { ...perAddonSettings },
        },
      },
    }),
  ]);
  addon = await AddonManager.getAddonByID(id);
  notEqual(addon, null, "Add-on installed");
  await addon.uninstall();

  info(
    "Expect install to fail if allowed globally but disallowed by per-addon settings"
  );
  await Promise.all([
    AddonTestUtils.promiseInstallEvent("onDownloadFailed"),
    setupPolicyEngineWithJson({
      policies: {
        ExtensionSettings: {
          "*": { temporarily_allow_weak_signatures: true },
          [id]: {
            ...perAddonSettings,
            temporarily_allow_weak_signatures: false,
          },
        },
      },
    }),
  ]);
  addon = await AddonManager.getAddonByID(id);
  equal(addon, null, "Add-on not installed");

  info(
    "Expect install to be allowed through per addon setting when globally disallowed"
  );
  await Promise.all([
    AddonTestUtils.promiseInstallEvent("onInstallEnded"),
    setupPolicyEngineWithJson({
      policies: {
        ExtensionSettings: {
          "*": { temporarily_allow_weak_signatures: false },
          [id]: {
            ...perAddonSettings,
            temporarily_allow_weak_signatures: true,
          },
        },
      },
    }),
  ]);
  addon = await AddonManager.getAddonByID(id);
  notEqual(addon, null, "Add-on installed");
  await addon.uninstall();

  resetWeakSignaturePref();
});

add_task(async function test_policy_installed_only_addon() {
  const cleanupTestAddon = async () => {
    const addon = await AddonManager.getAddonByID(policyOnlyID);
    if (addon) {
      await addon.uninstall();
    }
  };
  registerCleanupFunction(cleanupTestAddon);

  info(
    "Expect the test addon to install successfully when installed by the policy"
  );
  let installPromise = waitForAddonInstall(policyOnlyID);
  await setupPolicyEngineWithJson({
    policies: {
      ExtensionSettings: {
        [policyOnlyID]: {
          install_url: `${BASE_URL}/policy_installed_only.xpi`,
          installation_mode: "force_installed",
          updates_disabled: true,
        },
      },
    },
  });
  await installPromise;
  let addon = await AddonManager.getAddonByID(policyOnlyID);
  Assert.notEqual(addon, null, "Addon expected to be installed successfully.");
  Assert.deepEqual(
    addon.installTelemetryInfo,
    { source: "enterprise-policy" },
    "Got the expected addon.installTelemetryInfo"
  );

  info(
    "Remove the ExtensionSettings and verify the addon becomes appDisabled on XPIDatabase.verifySignatures calls"
  );
  await setupPolicyEngineWithJson({
    policies: {
      ExtensionSettings: {
        // NOTE: an empty ExtensionSettings config would not be reflected
        // by data returned by Services.policy.getExtensionSettings calls,
        // on the contrary setting new policy data with an unrelated addon-id
        // forces the ExtensionSettings data to be refreshed.
        "someother-addon@test": { updates_disabled: true },
      },
    },
  });

  {
    const { XPIExports } = ChromeUtils.importESModule(
      "resource://gre/modules/addons/XPIExports.sys.mjs"
    );
    await XPIExports.XPIDatabase.verifySignatures();
  }

  addon = await AddonManager.getAddonByID(policyOnlyID);
  Assert.notEqual(addon, null, "Addon expected to still be installed");
  Assert.equal(addon.appDisabled, true, "Expect the addon to be appDisabled");

  await cleanupTestAddon();
  addon = await AddonManager.getAddonByID(policyOnlyID);
  Assert.equal(addon, null, "Addon expected to not be installed anymore");

  info(
    "Expect the test addon to NOT install successfully when not installed by the policy"
  );
  // Setting back the extension settings with installation_mode set to force_installed
  // will install the extension again, and so we need to wait for that and uninstall
  // it first (otherwise the addon may endup being installed when the test task is
  // completed and trigger an intermittent failre).
  installPromise = waitForAddonInstall(policyOnlyID);
  await setupPolicyEngineWithJson({
    policies: {
      ExtensionSettings: {
        [policyOnlyID]: {
          install_url: `${BASE_URL}/policy_installed_only.xpi`,
          installation_mode: "force_installed",
          updates_disabled: true,
        },
      },
    },
  });
  await installPromise;
  await cleanupTestAddon();

  await setupPolicyEngineWithJson({
    policies: {
      ExtensionSettings: {
        // NOTE: an empty ExtensionSettings config would not be reflected
        // by data returned by Services.policy.getExtensionSettings calls,
        // on the contrary setting new policy data with an unrelated addon-id
        // forces the ExtensionSettings data to be refreshed.
        "someother-addon@test": { updates_disabled: true },
      },
    },
  });

  const { messages } = await AddonTestUtils.promiseConsoleOutput(async () => {
    installPromise = waitForAddonInstall(policyOnlyID);
    const install = await AddonManager.getInstallForURL(
      `${BASE_URL}/policy_installed_only.xpi`,
      { telemetryInfo: { source: "not-enterprise-policy" } }
    );
    await Assert.rejects(
      install.install(),
      /Install failed/,
      "Expect the install method to reject"
    );
    await installPromise;

    addon = await AddonManager.getAddonByID(policyOnlyID);
    Assert.equal(addon, null, "Addon expected to not be installed");
  });

  AddonTestUtils.checkMessages(
    messages,
    {
      expected: [
        {
          message:
            /This addon can only be installed through Enterprise Policies/,
        },
      ],
    },
    "Got the expect error logged on installing enterprise only extension"
  );
});
