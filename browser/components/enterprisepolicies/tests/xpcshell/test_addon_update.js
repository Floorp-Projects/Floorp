/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { AddonTestUtils } = ChromeUtils.import(
  "resource://testing-common/AddonTestUtils.jsm"
);
const { AddonManager } = ChromeUtils.import(
  "resource://gre/modules/AddonManager.jsm"
);

AddonTestUtils.init(this);
AddonTestUtils.overrideCertDB();
AddonTestUtils.appInfo = getAppInfo();

const server = AddonTestUtils.createHttpServer({ hosts: ["example.com"] });
const BASE_URL = `http://example.com/data`;

let TEST_NAME = "updatable.xpi";

/* Test that when a local file addon is updated,
   the new version gets installed. */
add_task(async function test_local_addon_update() {
  await AddonTestUtils.promiseStartupManager();

  let tmpDir = Services.dirsvc.get("TmpD", Ci.nsIFile);
  let id = "updatable1@test";
  let xpi1 = AddonTestUtils.createTempWebExtensionFile({
    manifest: {
      version: "1.0",
      applications: {
        gecko: { id },
      },
    },
  });
  xpi1.copyTo(tmpDir, TEST_NAME);

  await Promise.all([
    AddonTestUtils.promiseInstallEvent("onInstallEnded"),
    setupPolicyEngineWithJson({
      policies: {
        ExtensionSettings: {
          "updatable@test": {
            installation_mode: "force_installed",
            install_url: Services.io.newFileURI(tmpDir).spec + "/" + TEST_NAME,
          },
        },
      },
    }),
  ]);
  let addon = await AddonManager.getAddonByID(id);
  notEqual(addon, null, "Addon should not be null");
  equal(addon.version, "1.0", "Addon 1.0 installed");

  let xpi2 = AddonTestUtils.createTempWebExtensionFile({
    manifest: {
      version: "2.0",
      applications: {
        gecko: { id },
      },
    },
  });
  // overwrite the test file
  xpi2.copyTo(tmpDir, TEST_NAME);

  await Promise.all([
    AddonTestUtils.promiseInstallEvent("onInstallEnded"),
    setupPolicyEngineWithJson({
      policies: {
        ExtensionSettings: {
          "updatable@test": {
            installation_mode: "force_installed",
            install_url: Services.io.newFileURI(tmpDir).spec + "/" + TEST_NAME,
          },
        },
      },
    }),
  ]);

  addon = await AddonManager.getAddonByID(id);
  equal(addon.version, "2.0", "Addon 2.0 installed");

  let xpifile = tmpDir.clone();
  xpifile.append(TEST_NAME);
  xpifile.remove(false);
});

/* Test that when the url changes,
   the new version gets installed. */
add_task(async function test_newurl_addon_update() {
  let id = "updatable2@test";

  let xpi1 = AddonTestUtils.createTempWebExtensionFile({
    manifest: {
      version: "1.0",
      applications: {
        gecko: { id },
      },
    },
  });
  server.registerFile("/data/policy_test1.xpi", xpi1);

  let xpi2 = AddonTestUtils.createTempWebExtensionFile({
    manifest: {
      version: "2.0",
      applications: {
        gecko: { id },
      },
    },
  });
  server.registerFile("/data/policy_test2.xpi", xpi2);

  await Promise.all([
    AddonTestUtils.promiseInstallEvent("onInstallEnded"),
    setupPolicyEngineWithJson({
      policies: {
        ExtensionSettings: {
          "updatable2@test": {
            installation_mode: "force_installed",
            install_url: `${BASE_URL}/policy_test1.xpi`,
          },
        },
      },
    }),
  ]);
  let addon = await AddonManager.getAddonByID(id);
  notEqual(addon, null, "Addon should not be null");
  equal(addon.version, "1.0", "Addon 1.0 installed");

  await Promise.all([
    AddonTestUtils.promiseInstallEvent("onInstallEnded"),
    setupPolicyEngineWithJson({
      policies: {
        ExtensionSettings: {
          "updatable2@test": {
            installation_mode: "force_installed",
            install_url: `${BASE_URL}/policy_test2.xpi`,
          },
        },
      },
    }),
  ]);

  addon = await AddonManager.getAddonByID(id);
  equal(addon.version, "2.0", "Addon 2.0 installed");
});
