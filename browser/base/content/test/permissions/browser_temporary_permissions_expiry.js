/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable mozilla/no-arbitrary-setTimeout */

"use strict";

const ORIGIN = "https://example.com";
const PERMISSIONS_PAGE =
  getRootDirectory(gTestPath).replace("chrome://mochitests/content", ORIGIN) +
  "permissions.html";

// Ignore promise rejection caused by clicking Deny button.
const { PromiseTestUtils } = ChromeUtils.import(
  "resource://testing-common/PromiseTestUtils.jsm"
);
PromiseTestUtils.whitelistRejectionsGlobally(/The request is not allowed/);

const EXPIRE_TIME_MS = 100;
const TIMEOUT_MS = 500;

// Test that temporary permissions can be re-requested after they expired
// and that the identity block is updated accordingly.
add_task(async function testTempPermissionRequestAfterExpiry() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["privacy.temporary_permission_expire_time_ms", EXPIRE_TIME_MS],
      ["media.navigator.permission.fake", true],
    ],
  });

  let uri = NetUtil.newURI(ORIGIN);
  let ids = ["geo", "camera"];

  for (let id of ids) {
    await BrowserTestUtils.withNewTab(PERMISSIONS_PAGE, async function(
      browser
    ) {
      let blockedIcon = gIdentityHandler._identityBox.querySelector(
        `.blocked-permission-icon[data-permission-id='${id}']`
      );

      SitePermissions.set(
        uri,
        id,
        SitePermissions.BLOCK,
        SitePermissions.SCOPE_TEMPORARY,
        browser
      );

      Assert.deepEqual(SitePermissions.get(uri, id, browser), {
        state: SitePermissions.BLOCK,
        scope: SitePermissions.SCOPE_TEMPORARY,
      });

      ok(
        blockedIcon.hasAttribute("showing"),
        "blocked permission icon is shown"
      );

      await new Promise(c => setTimeout(c, TIMEOUT_MS));

      Assert.deepEqual(SitePermissions.get(uri, id, browser), {
        state: SitePermissions.UNKNOWN,
        scope: SitePermissions.SCOPE_PERSISTENT,
      });

      let popupshown = BrowserTestUtils.waitForEvent(
        PopupNotifications.panel,
        "popupshown"
      );

      // Request a permission;
      await BrowserTestUtils.synthesizeMouseAtCenter(`#${id}`, {}, browser);

      await popupshown;

      ok(
        !blockedIcon.hasAttribute("showing"),
        "blocked permission icon is not shown"
      );

      let popuphidden = BrowserTestUtils.waitForEvent(
        PopupNotifications.panel,
        "popuphidden"
      );

      let notification = PopupNotifications.panel.firstElementChild;
      EventUtils.synthesizeMouseAtCenter(notification.secondaryButton, {});

      await popuphidden;

      SitePermissions.remove(uri, id, browser);
    });
  }
});
