"use strict";

const INSTALL_PAGE = `${BASE}/file_install_extensions.html`;
const INSTALL_XPI = `${BASE}/browser_webext_permissions.xpi`;

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({set: [
    ["extensions.webapi.testing", true],
    ["extensions.install.requireBuiltInCerts", false],
  ]});

  registerCleanupFunction(async () => {
    await SpecialPowers.popPrefEnv();
  });
});

add_task(async function test_tab_switch_dismiss() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, INSTALL_PAGE);

  let installCanceled = new Promise(resolve => {
    let listener = {
      onInstallCancelled() {
        AddonManager.removeInstallListener(listener);
        resolve();
      },
    };
    AddonManager.addInstallListener(listener);
  });

  ContentTask.spawn(gBrowser.selectedBrowser, INSTALL_XPI, function(url) {
    content.wrappedJSObject.installMozAM(url);
  });

  await promisePopupNotificationShown("addon-webext-permissions");
  let permsUL = document.getElementById("addon-webext-perm-list");
  is(permsUL.childElementCount, 5, `Permissions list has 5 entries`);

  // Switching tabs dismisses the notification and cancels the install.
  let switchTo = await BrowserTestUtils.openNewForegroundTab(gBrowser);
  BrowserTestUtils.removeTab(switchTo);
  await installCanceled;

  let addon = await AddonManager.getAddonByID("permissions@test.mozilla.org");
  is(addon, null, "Extension is not installed");

  BrowserTestUtils.removeTab(tab);
});
