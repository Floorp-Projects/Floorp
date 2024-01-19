"use strict";

const INSTALL_PAGE = `${BASE}/file_install_extensions.html`;
const INSTALL_XPI = `${BASE}/browser_webext_permissions.xpi`;

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["extensions.webapi.testing", true],
      ["extensions.install.requireBuiltInCerts", false],
    ],
  });

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

  SpecialPowers.spawn(gBrowser.selectedBrowser, [INSTALL_XPI], function (url) {
    content.wrappedJSObject.installMozAM(url);
  });

  await promisePopupNotificationShown("addon-webext-permissions");
  let permsUL = document.getElementById("addon-webext-perm-list");
  is(permsUL.childElementCount, 5, `Permissions list has 5 entries`);

  let permsLearnMore = document.getElementById("addon-webext-perm-info");
  ok(
    BrowserTestUtils.isVisible(permsLearnMore),
    "Learn more link is shown on Permission popup"
  );
  is(
    permsLearnMore.href,
    Services.urlFormatter.formatURLPref("app.support.baseURL") +
      "extension-permissions",
    "Learn more link has desired URL"
  );

  // Switching tabs dismisses the notification and cancels the install.
  let switchTo = await BrowserTestUtils.openNewForegroundTab(gBrowser);
  BrowserTestUtils.removeTab(switchTo);
  await installCanceled;

  let addon = await AddonManager.getAddonByID("permissions@test.mozilla.org");
  is(addon, null, "Extension is not installed");

  BrowserTestUtils.removeTab(tab);
});

add_task(async function test_add_tab_by_user_and_switch() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, INSTALL_PAGE);

  let listener = {
    onInstallCancelled() {
      this.canceledPromise = Promise.resolve();
    },
  };
  AddonManager.addInstallListener(listener);

  SpecialPowers.spawn(gBrowser.selectedBrowser, [INSTALL_XPI], function (url) {
    content.wrappedJSObject.installMozAM(url);
  });

  // Show addon permission notification.
  await promisePopupNotificationShown("addon-webext-permissions");
  is(
    document.getElementById("addon-webext-perm-list").childElementCount,
    5,
    "Permissions list has 5 entries"
  );

  // Open about:newtab page in a new tab.
  let newTab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:newtab",
    false
  );

  // Switch to tab that is opening addon permission notification.
  gBrowser.selectedTab = tab;
  is(
    document.getElementById("addon-webext-perm-list").childElementCount,
    5,
    "Permission notification is shown again"
  );
  ok(!listener.canceledPromise, "Extension installation is not canceled");

  // Cancel installation.
  document.querySelector(".popup-notification-secondary-button").click();
  await listener.canceledPromise;
  info("Extension installation is canceled");

  let addon = await AddonManager.getAddonByID("permissions@test.mozilla.org");
  is(addon, null, "Extension is not installed");

  AddonManager.removeInstallListener(listener);
  BrowserTestUtils.removeTab(tab);
  BrowserTestUtils.removeTab(newTab);
});
