/**
 * These tests test the ability for the PermissionUI module to open
 * permission prompts to the user. It also tests to ensure that
 * add-ons can introduce their own permission prompts.
 */

"use strict";

ChromeUtils.import("resource://gre/modules/Integration.jsm", this);
ChromeUtils.import("resource:///modules/PermissionUI.jsm", this);
ChromeUtils.import("resource:///modules/SitePermissions.jsm", this);

// Tests that GeolocationPermissionPrompt works as expected
add_task(async function test_geo_permission_prompt() {
  await testPrompt(PermissionUI.GeolocationPermissionPrompt);
});

// Tests that DesktopNotificationPermissionPrompt works as expected
add_task(async function test_desktop_notification_permission_prompt() {
  Services.prefs.setBoolPref(
    "dom.webnotifications.requireuserinteraction",
    false
  );
  await testPrompt(PermissionUI.DesktopNotificationPermissionPrompt);
  Services.prefs.clearUserPref("dom.webnotifications.requireuserinteraction");
});

// Tests that PersistentStoragePermissionPrompt works as expected
add_task(async function test_persistent_storage_permission_prompt() {
  await testPrompt(PermissionUI.PersistentStoragePermissionPrompt);
});

// Tests that MidiPrompt works as expected
add_task(async function test_midi_permission_prompt() {
  await testPrompt(PermissionUI.MIDIPermissionPrompt);
});

// Tests that StoragePermissionPrompt works as expected
add_task(async function test_storage_access_permission_prompt() {
  Services.prefs.setBoolPref("dom.storage_access.auto_grants", false);
  await testPrompt(PermissionUI.StorageAccessPermissionPrompt);
  Services.prefs.clearUserPref("dom.storage_access.auto_grants");
});

async function testPrompt(Prompt) {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "http://example.com",
    },
    async function(browser) {
      let mockRequest = makeMockPermissionRequest(browser);
      let principal = mockRequest.principal;
      let TestPrompt = new Prompt(mockRequest);
      let permissionKey =
        TestPrompt.usePermissionManager && TestPrompt.permissionKey;

      registerCleanupFunction(function() {
        if (permissionKey) {
          SitePermissions.remove(principal.URI, permissionKey);
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
        curPerm = SitePermissions.get(principal.URI, permissionKey, browser);
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
      let isPersistentStoragePrompt =
        Prompt == PermissionUI.PersistentStoragePermissionPrompt;
      let isStorageAccessPrompt =
        Prompt == PermissionUI.StorageAccessPermissionPrompt;

      let expectedSecondaryActionsCount =
        isNotificationPrompt ||
        isPersistentStoragePrompt ||
        isStorageAccessPrompt
          ? 2
          : 1;
      Assert.equal(
        notification.secondaryActions.length,
        expectedSecondaryActionsCount,
        "There should only be " +
          expectedSecondaryActionsCount +
          " secondary action(s)"
      );
      await clickSecondaryAction();
      if (permissionKey) {
        curPerm = SitePermissions.get(principal.URI, permissionKey, browser);
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

        SitePermissions.remove(principal.URI, permissionKey, browser);
        mockRequest._cancelled = false;
      }

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
      if (isNotificationPrompt || isPersistentStoragePrompt) {
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
        curPerm = SitePermissions.get(principal.URI, permissionKey);
        Assert.deepEqual(
          curPerm,
          {
            state: SitePermissions.BLOCK,
            scope: SitePermissions.SCOPE_PERSISTENT,
          },
          "Should have denied the action permanently"
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

      if (permissionKey) {
        SitePermissions.remove(principal.URI, permissionKey);
        mockRequest._cancelled = false;
      }

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
      if (permissionKey) {
        curPerm = SitePermissions.get(principal.URI, permissionKey);
        Assert.deepEqual(
          curPerm,
          {
            state: SitePermissions.ALLOW,
            scope: SitePermissions.SCOPE_PERSISTENT,
          },
          "Should have allowed the action permanently"
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
