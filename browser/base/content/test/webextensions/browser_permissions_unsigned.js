"use strict";

const ID = "permissions@test.mozilla.org";
const WARNING_ICON = "chrome://browser/skin/warning.svg";

add_task(async function test_unsigned() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["extensions.webapi.testing", true],
      ["extensions.install.requireBuiltInCerts", false],
    ],
  });

  let testURI = makeURI("https://example.com/");
  Services.perms.add(testURI, "install", Services.perms.ALLOW_ACTION);
  registerCleanupFunction(() => Services.perms.remove(testURI, "install"));

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);

  BrowserTestUtils.loadURI(
    gBrowser.selectedBrowser,
    `${BASE}/file_install_extensions.html`
  );
  await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);

  ContentTask.spawn(
    gBrowser.selectedBrowser,
    `${BASE}/browser_webext_unsigned.xpi`,
    async function(url) {
      content.wrappedJSObject.installTrigger(url);
    }
  );

  let panel = await promisePopupNotificationShown("addon-webext-permissions");

  is(panel.getAttribute("icon"), WARNING_ICON);
  checkPermissionString(
    document.getElementById("addon-webext-perm-text").textContent,
    "webextPerms.unsignedWarning",
    null,
    "Install notification includes unsigned warning"
  );

  // cancel the install
  let promise = promiseInstallEvent({ id: ID }, "onInstallCancelled");
  panel.secondaryButton.click();
  await promise;

  let addon = await AddonManager.getAddonByID(ID);
  is(addon, null, "Extension is not installed");

  BrowserTestUtils.removeTab(tab);
});
