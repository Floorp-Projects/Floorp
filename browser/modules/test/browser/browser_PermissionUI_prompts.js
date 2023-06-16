/**
 * These tests test the ability for the PermissionUI module to open
 * permission prompts to the user. It also tests to ensure that
 * add-ons can introduce their own permission prompts.
 */

"use strict";

const { PermissionUI } = ChromeUtils.importESModule(
  "resource:///modules/PermissionUI.sys.mjs"
);
const { SITEPERMS_ADDON_PROVIDER_PREF } = ChromeUtils.importESModule(
  "resource://gre/modules/addons/siteperms-addon-utils.sys.mjs"
);

// Tests that GeolocationPermissionPrompt works as expected
add_task(async function test_geo_permission_prompt() {
  await testPrompt(PermissionUI.GeolocationPermissionPrompt);
});

// Tests that GeolocationPermissionPrompt works as expected with local files
add_task(async function test_geo_permission_prompt_local_file() {
  await testPrompt(PermissionUI.GeolocationPermissionPrompt, true);
});

// Tests that XRPermissionPrompt works as expected
add_task(async function test_xr_permission_prompt() {
  await testPrompt(PermissionUI.XRPermissionPrompt);
});

// Tests that XRPermissionPrompt works as expected with local files
add_task(async function test_xr_permission_prompt_local_file() {
  await testPrompt(PermissionUI.XRPermissionPrompt, true);
});

// Tests that DesktopNotificationPermissionPrompt works as expected
add_task(async function test_desktop_notification_permission_prompt() {
  Services.prefs.setBoolPref(
    "dom.webnotifications.requireuserinteraction",
    false
  );
  Services.prefs.setBoolPref(
    "permissions.desktop-notification.notNow.enabled",
    true
  );
  await testPrompt(PermissionUI.DesktopNotificationPermissionPrompt);
  Services.prefs.clearUserPref("dom.webnotifications.requireuserinteraction");
  Services.prefs.clearUserPref(
    "permissions.desktop-notification.notNow.enabled"
  );
});

// Tests that PersistentStoragePermissionPrompt works as expected
add_task(async function test_persistent_storage_permission_prompt() {
  await testPrompt(PermissionUI.PersistentStoragePermissionPrompt);
});

// Tests that MidiPrompt works as expected
add_task(async function test_midi_permission_prompt() {
  if (Services.prefs.getBoolPref(SITEPERMS_ADDON_PROVIDER_PREF, false)) {
    ok(
      true,
      "PermissionUI.MIDIPermissionPrompt uses SitePermsAddon install flow"
    );
    return;
  }
  await testPrompt(PermissionUI.MIDIPermissionPrompt);
});

// Tests that MidiPrompt works as expected with local files
add_task(async function test_midi_permission_prompt_local_file() {
  if (Services.prefs.getBoolPref(SITEPERMS_ADDON_PROVIDER_PREF, false)) {
    ok(
      true,
      "PermissionUI.MIDIPermissionPrompt uses SitePermsAddon install flow"
    );
    return;
  }
  await testPrompt(PermissionUI.MIDIPermissionPrompt, true);
});

// Tests that StoragePermissionPrompt works as expected
add_task(async function test_storage_access_permission_prompt() {
  Services.prefs.setBoolPref("dom.storage_access.auto_grants", false);
  await testPrompt(PermissionUI.StorageAccessPermissionPrompt);
  Services.prefs.clearUserPref("dom.storage_access.auto_grants");
});

async function testPrompt(Prompt, useLocalFile = false) {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: useLocalFile ? `file://${PathUtils.tempDir}` : "http://example.com",
    },
    async function (browser) {
      let mockRequest = makeMockPermissionRequest(browser);
      let principal = mockRequest.principal;
      let TestPrompt = new Prompt(mockRequest);
      let { usePermissionManager, permissionKey } = TestPrompt;

      registerCleanupFunction(function () {
        if (permissionKey) {
          SitePermissions.removeFromPrincipal(
            principal,
            permissionKey,
            browser
          );
        }
      });

      let shownPromise = BrowserTestUtils.waitForEvent(
        PopupNotifications.panel,
        "popupshown"
      );
      TestPrompt.prompt();
      await shownPromise;
      let notification = PopupNotifications.getNotification(
        TestPrompt.notificationID,
        browser
      );
      Assert.ok(notification, "Should have gotten the notification");

      let curPerm;
      if (permissionKey) {
        curPerm = SitePermissions.getForPrincipal(
          principal,
          permissionKey,
          browser
        );
        Assert.equal(
          curPerm.state,
          SitePermissions.UNKNOWN,
          "Should be no permission set to begin with."
        );
      }

      // First test denying the permission request without the checkbox checked.
      let popupNotification = getPopupNotificationNode();
      popupNotification.checkbox.checked = false;

      let isNotificationPrompt =
        Prompt == PermissionUI.DesktopNotificationPermissionPrompt;

      let expectedSecondaryActionsCount = isNotificationPrompt ? 2 : 1;
      Assert.equal(
        notification.secondaryActions.length,
        expectedSecondaryActionsCount,
        "There should only be " +
          expectedSecondaryActionsCount +
          " secondary action(s)"
      );
      await clickSecondaryAction();
      if (permissionKey) {
        curPerm = SitePermissions.getForPrincipal(
          principal,
          permissionKey,
          browser
        );
        Assert.deepEqual(
          curPerm,
          {
            state: SitePermissions.BLOCK,
            scope: SitePermissions.SCOPE_TEMPORARY,
          },
          "Should have denied the action temporarily"
        );

        Assert.ok(
          mockRequest._cancelled,
          "The request should have been cancelled"
        );
        Assert.ok(
          !mockRequest._allowed,
          "The request should not have been allowed"
        );
      }

      SitePermissions.removeFromPrincipal(
        principal,
        TestPrompt.permissionKey,
        browser
      );
      mockRequest._cancelled = false;

      // Bring the PopupNotification back up now...
      shownPromise = BrowserTestUtils.waitForEvent(
        PopupNotifications.panel,
        "popupshown"
      );
      TestPrompt.prompt();
      await shownPromise;

      // Test denying the permission request with the checkbox checked (for geolocation)
      // or by clicking the "never" option from the dropdown (for notifications and persistent-storage).
      popupNotification = getPopupNotificationNode();
      let secondaryActionToClickIndex = 0;
      if (isNotificationPrompt) {
        secondaryActionToClickIndex = 1;
      } else {
        popupNotification.checkbox.checked = true;
      }

      Assert.equal(
        notification.secondaryActions.length,
        expectedSecondaryActionsCount,
        "There should only be " +
          expectedSecondaryActionsCount +
          " secondary action(s)"
      );
      await clickSecondaryAction(secondaryActionToClickIndex);
      if (permissionKey) {
        curPerm = SitePermissions.getForPrincipal(
          principal,
          permissionKey,
          browser
        );
        Assert.equal(
          curPerm.state,
          SitePermissions.BLOCK,
          "Should have denied the action"
        );

        let expectedScope = usePermissionManager
          ? SitePermissions.SCOPE_PERSISTENT
          : SitePermissions.SCOPE_TEMPORARY;
        Assert.equal(
          curPerm.scope,
          expectedScope,
          `Deny should be ${usePermissionManager ? "persistent" : "temporary"}`
        );

        Assert.ok(
          mockRequest._cancelled,
          "The request should have been cancelled"
        );
        Assert.ok(
          !mockRequest._allowed,
          "The request should not have been allowed"
        );
      }

      SitePermissions.removeFromPrincipal(principal, permissionKey, browser);
      mockRequest._cancelled = false;

      // Bring the PopupNotification back up now...
      shownPromise = BrowserTestUtils.waitForEvent(
        PopupNotifications.panel,
        "popupshown"
      );
      TestPrompt.prompt();
      await shownPromise;

      // Test allowing the permission request with the checkbox checked.
      popupNotification = getPopupNotificationNode();
      popupNotification.checkbox.checked = true;

      await clickMainAction();
      // If the prompt does not use the permission manager, it can not set a
      // persistent allow. Temporary allow is not supported.
      if (usePermissionManager && permissionKey) {
        curPerm = SitePermissions.getForPrincipal(
          principal,
          permissionKey,
          browser
        );
        Assert.equal(
          curPerm.state,
          SitePermissions.ALLOW,
          "Should have allowed the action"
        );
        Assert.equal(
          curPerm.scope,
          SitePermissions.SCOPE_PERSISTENT,
          "Allow should be permanent"
        );
        Assert.ok(
          !mockRequest._cancelled,
          "The request should not have been cancelled"
        );
        Assert.ok(mockRequest._allowed, "The request should have been allowed");
      }
    }
  );
}
