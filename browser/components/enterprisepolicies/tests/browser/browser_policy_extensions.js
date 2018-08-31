/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const addonID = "policytest@mozilla.com";

add_task(async function test_addon_install() {
  let installPromise = wait_for_addon_install();
  await setupPolicyEngineWithJson({
    "policies": {
      "Extensions": {
        "Install": [
          "http://mochi.test:8888/browser/browser/components/enterprisepolicies/tests/browser/policytest.xpi",
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

add_task(async function test_addon_uninstall() {
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
