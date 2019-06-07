/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const ADDON_ID = "policytest@mozilla.com";
const BASE_URL = "http://mochi.test:8888/browser/browser/components/enterprisepolicies/tests/browser";

async function isExtensionLocked(win, addonID) {
  if (win.useHtmlViews) {
    // Test on HTML about:addons page.
    let addonCard = await BrowserTestUtils.waitForCondition(async () => {
      let doc = win.getHtmlBrowser().contentDocument;
      await win.htmlBrowserLoaded;
      return doc.querySelector(`addon-card[addon-id="${addonID}"]`);
    }, `Get addon-card for "${addonID}"`);
    let disableBtn = addonCard.querySelector('panel-item[action="toggle-disabled"]');
    let removeBtn = addonCard.querySelector('panel-item[action="remove"]');
    ok(removeBtn.hidden, "Remove button should be hidden");
    ok(disableBtn.hidden, "Disable button should be hidden");
  } else {
    // Test on XUL about:addons page.
    const doc = win.document;
    let list = doc.getElementById("addon-list");
    let addonEntry = await BrowserTestUtils.waitForCondition(
      () => list.getElementsByAttribute("value", addonID)[0],
      `Get addon entry for "${addonID}"`);
    let disableBtn = doc.getAnonymousElementByAttribute(addonEntry, "anonid", "disable-btn");
    let removeBtn = doc.getAnonymousElementByAttribute(addonEntry, "anonid", "remove-btn");
    ok(removeBtn.hidden, "Remove button should be hidden");
    ok(disableBtn.hidden, "Disable button should be hidden");
  }
}

// This test case will run on both the XUL and HTML about:addons views.
async function test_addon_locked() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);
  const win = await BrowserOpenAddonsMgr("addons://list/extension");

  await isExtensionLocked(win, ADDON_ID);

  BrowserTestUtils.removeTab(tab);
}

add_task(async function test_addon_install() {
  let installPromise = wait_for_addon_install();
  await setupPolicyEngineWithJson({
    "policies": {
      "Extensions": {
        "Install": [
          `${BASE_URL}/policytest_v0.1.xpi`,
        ],
        "Locked": [
          ADDON_ID,
        ],
      },
    },
  });
  await installPromise;
  let addon = await AddonManager.getAddonByID(ADDON_ID);
  isnot(addon, null, "Addon not installed.");
  is(addon.version, "0.1", "Addon version is correct");

  Assert.deepEqual(addon.installTelemetryInfo, {source: "enterprise-policy"},
                   "Got the expected addon.installTelemetryInfo");
});

add_task(async function test_XUL_aboutaddons_addon_locked() {
  await testOnAboutAddonsType("XUL", test_addon_locked);
});

add_task(async function test_HTML_aboutaddons_addon_locked() {
  await testOnAboutAddonsType("HTML", test_addon_locked);
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
          ADDON_ID,
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

  let addon = await AddonManager.getAddonByID(ADDON_ID);
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
          ADDON_ID,
        ],
      },
    },
  });
  await uninstallPromise;
  let addon = await AddonManager.getAddonByID(ADDON_ID);
  is(addon, null, "Addon should be uninstalled.");
});

add_task(async function test_addon_download_failure() {
  // Test that if the download fails, the runOnce pref
  // is cleared so that the download will happen again.

  let installPromise = wait_for_addon_install();
  await setupPolicyEngineWithJson({
    "policies": {
      "Extensions": {
        "Install": [
          `${BASE_URL}/policytest_invalid.xpi`,
        ],
      },
    },
  });

  await installPromise;
  is(Services.prefs.prefHasUserValue("browser.policies.runOncePerModification.extensionsInstall"), false, "runOnce pref should be unset");
});

function wait_for_addon_install() {
  return new Promise(resolve => {
    let listener = {
      onInstallEnded(install, addon) {
        if (addon.id == ADDON_ID) {
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

function wait_for_addon_uninstall() {
 return new Promise(resolve => {
    let listener = {};
    listener.onUninstalled = addon => {
      if (addon.id == ADDON_ID) {
        AddonManager.removeAddonListener(listener);
        resolve();
      }
    };
    AddonManager.addAddonListener(listener);
  });
}
