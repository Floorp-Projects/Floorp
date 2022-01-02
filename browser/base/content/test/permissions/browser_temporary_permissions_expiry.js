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
PromiseTestUtils.allowMatchingRejectionsGlobally(/The request is not allowed/);

const EXPIRE_TIME_MS = 100;
const TIMEOUT_MS = 500;

const EXPIRE_TIME_CUSTOM_MS = 1000;
const TIMEOUT_CUSTOM_MS = 1500;

const kVREnabled = SpecialPowers.getBoolPref("dom.vr.enabled");

// Test that temporary permissions can be re-requested after they expired
// and that the identity block is updated accordingly.
add_task(async function testTempPermissionRequestAfterExpiry() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["privacy.temporary_permission_expire_time_ms", EXPIRE_TIME_MS],
      ["media.navigator.permission.fake", true],
      ["dom.vr.always_support_vr", true],
    ],
  });

  let principal = Services.scriptSecurityManager.createContentPrincipalFromOrigin(
    ORIGIN
  );
  let ids = ["geo", "camera"];

  if (kVREnabled) {
    ids.push("xr");
  }

  for (let id of ids) {
    await BrowserTestUtils.withNewTab(PERMISSIONS_PAGE, async function(
      browser
    ) {
      let blockedIcon = gPermissionPanel._identityPermissionBox.querySelector(
        `.blocked-permission-icon[data-permission-id='${id}']`
      );

      SitePermissions.setForPrincipal(
        principal,
        id,
        SitePermissions.BLOCK,
        SitePermissions.SCOPE_TEMPORARY,
        browser
      );

      Assert.deepEqual(
        SitePermissions.getForPrincipal(principal, id, browser),
        {
          state: SitePermissions.BLOCK,
          scope: SitePermissions.SCOPE_TEMPORARY,
        }
      );

      ok(
        blockedIcon.hasAttribute("showing"),
        "blocked permission icon is shown"
      );

      await new Promise(c => setTimeout(c, TIMEOUT_MS));

      Assert.deepEqual(
        SitePermissions.getForPrincipal(principal, id, browser),
        {
          state: SitePermissions.UNKNOWN,
          scope: SitePermissions.SCOPE_PERSISTENT,
        }
      );

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

      SitePermissions.removeFromPrincipal(principal, id, browser);
    });
  }
});

/**
 * Test whether the identity UI shows the permission granted state.
 * @param {boolean} state - true = Shows permission granted, false otherwise.
 */
async function testIdentityPermissionGrantedState(state) {
  let hasAttribute;
  let msg = `Identity permission box ${
    state ? "shows" : "does not show"
  } granted permissions.`;
  await TestUtils.waitForCondition(() => {
    hasAttribute = gPermissionPanel._identityPermissionBox.hasAttribute(
      "hasPermissions"
    );
    return hasAttribute == state;
  }, msg);
  is(hasAttribute, state, msg);
}

// Test that temporary permissions can have custom expiry time and the identity
// block is updated correctly on expiry.
add_task(async function testTempPermissionCustomExpiry() {
  const TEST_ID = "geo";
  // Set a default expiry time which is lower than the custom one we'll set.
  await SpecialPowers.pushPrefEnv({
    set: [["privacy.temporary_permission_expire_time_ms", EXPIRE_TIME_MS]],
  });

  await BrowserTestUtils.withNewTab(PERMISSIONS_PAGE, async browser => {
    Assert.deepEqual(
      SitePermissions.getForPrincipal(null, TEST_ID, browser),
      {
        state: SitePermissions.UNKNOWN,
        scope: SitePermissions.SCOPE_PERSISTENT,
      },
      "Permission not set initially"
    );

    await testIdentityPermissionGrantedState(false);

    // Set permission with custom expiry time.
    SitePermissions.setForPrincipal(
      null,
      "geo",
      SitePermissions.ALLOW,
      SitePermissions.SCOPE_TEMPORARY,
      browser,
      EXPIRE_TIME_CUSTOM_MS
    );

    await testIdentityPermissionGrantedState(true);

    // We've set the permission, start the timer promise.
    let timeout = new Promise(resolve =>
      setTimeout(resolve, TIMEOUT_CUSTOM_MS)
    );

    Assert.deepEqual(
      SitePermissions.getForPrincipal(null, TEST_ID, browser),
      {
        state: SitePermissions.ALLOW,
        scope: SitePermissions.SCOPE_TEMPORARY,
      },
      "We should see the temporary permission we just set."
    );

    // Wait for half of the expiry time.
    await new Promise(resolve =>
      setTimeout(resolve, EXPIRE_TIME_CUSTOM_MS / 2)
    );
    Assert.deepEqual(
      SitePermissions.getForPrincipal(null, TEST_ID, browser),
      {
        state: SitePermissions.ALLOW,
        scope: SitePermissions.SCOPE_TEMPORARY,
      },
      "Temporary permission should not have expired yet."
    );

    // Wait until permission expiry.
    await timeout;

    // Identity permission section should have updated by now. It should do this
    // without relying on side-effects of the SitePermissions getter.
    await testIdentityPermissionGrantedState(false);

    Assert.deepEqual(
      SitePermissions.getForPrincipal(null, TEST_ID, browser),
      {
        state: SitePermissions.UNKNOWN,
        scope: SitePermissions.SCOPE_PERSISTENT,
      },
      "Permission should have expired"
    );
  });
});
