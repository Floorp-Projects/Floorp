/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const addonID = "policytest@mozilla.com";
const BASE_URL = "http://mochi.test:8888/browser/browser/components/enterprisepolicies/tests/browser";

add_task(async function test_addon_install() {
  let installPromise = wait_for_addon_install();
  await setupPolicyEngineWithJson({
    "policies": {
      "Extensions": {
        "Install": [
          `${BASE_URL}/policytest_v0.1.xpi`,
        ],
        "Locked": [
          addonID,
        ],
      },
    },
  });
  await installPromise;
  let addon = await AddonManager.getAddonByID(addonID);
  isnot(addon, null, "Addon not installed.");
  is(addon.version, "0.1", "Addon version is correct");

  Assert.deepEqual(addon.installTelemetryInfo, {source: "enterprise-policy"},
                   "Got the expected addon.installTelemetryInfo");
});

add_task(async function test_addon_locked() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);
  await BrowserOpenAddonsMgr("addons://list/extension");
  // eslint-disable-next-line no-shadow
  await ContentTask.spawn(tab.linkedBrowser, {addonID}, async function({addonID}) {
    let list = content.document.getElementById("addon-list");
    let flashEntry = list.getElementsByAttribute("value", addonID)[0];
    let disableBtn = content.document.getAnonymousElementByAttribute(flashEntry, "anonid", "disable-btn");
    let removeBtn = content.document.getAnonymousElementByAttribute(flashEntry, "anonid", "remove-btn");
    is(removeBtn.hidden, true, "Remove button should be hidden");
    is(disableBtn.hidden, true, "Disable button should be hidden");
  });
  BrowserTestUtils.removeTab(tab);
});

add_task(async function test_addon_reinstall() {
  // Test that uninstalling and reinstalling the same addon ID works as expected.
  // This can be used to update an addon.

  let uninstallPromise = wait_for_addon_uninstall();
  let installPromise = wait_for_addon_install();
  await setupPolicyEngineWithJson({
    "policies": {
      "Extensions": {
        "Uninstall": [
          addonID,
        ],
        "Install": [
          `${BASE_URL}/policytest_v0.2.xpi`,
        ],
      },
    },
  });

  // Older version was uninstalled
  await uninstallPromise;

  // New version was installed
  await installPromise;

  let addon = await AddonManager.getAddonByID(addonID);
  isnot(addon, null, "Addon still exists because the policy was used to update it.");
  is(addon.version, "0.2", "New version is correct");
});


add_task(async function test_addon_uninstall() {
  EnterprisePolicyTesting.resetRunOnceState();

  let uninstallPromise = wait_for_addon_uninstall();
  await setupPolicyEngineWithJson({
    "policies": {
      "Extensions": {
        "Uninstall": [
          addonID,
        ],
      },
    },
  });
  await uninstallPromise;
  let addon = await AddonManager.getAddonByID(addonID);
  is(addon, null, "Addon should be uninstalled.");
});

function wait_for_addon_install() {
  return new Promise((resolve, reject) => {
      AddonManager.addInstallListener({
        onInstallEnded(install, addon) {
          if (addon.id == addonID) {
            resolve();
          }
        },
        onDownloadFailed: (install) => {
          reject();
        },
        onInstallFailed: (install) => {
          reject();
        },
      });
  });
}

function wait_for_addon_uninstall() {
 return new Promise(resolve => {
    let listener = {};
    listener.onUninstalled = addon => {
      if (addon.id == addonID) {
        AddonManager.removeAddonListener(listener);
        resolve();
      }
    };
    AddonManager.addAddonListener(listener);
  });
}
