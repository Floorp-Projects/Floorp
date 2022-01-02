/**
 * These tests test the ability for the PermissionUI module to open
 * permission prompts to the user. It also tests to ensure that
 * add-ons can introduce their own permission prompts.
 */

"use strict";

const { Integration } = ChromeUtils.import(
  "resource://gre/modules/Integration.jsm"
);
const { PermissionUI } = ChromeUtils.import(
  "resource:///modules/PermissionUI.jsm"
);
const { SitePermissions } = ChromeUtils.import(
  "resource:///modules/SitePermissions.jsm"
);
const { PermissionTestUtils } = ChromeUtils.import(
  "resource://testing-common/PermissionTestUtils.jsm"
);

/**
 * Tests the PermissionPromptForRequest prototype to ensure that a prompt
 * can be displayed. Does not test permission handling.
 */
add_task(async function test_permission_prompt_for_request() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "http://example.com/",
    },
    async function(browser) {
      const kTestNotificationID = "test-notification";
      const kTestMessage = "Test message";
      let mainAction = {
        label: "Main",
        accessKey: "M",
      };
      let secondaryAction = {
        label: "Secondary",
        accessKey: "S",
      };

      let mockRequest = makeMockPermissionRequest(browser);
      let TestPrompt = {
        __proto__: PermissionUI.PermissionPromptForRequestPrototype,
        request: mockRequest,
        notificationID: kTestNotificationID,
        message: kTestMessage,
        promptActions: [mainAction, secondaryAction],
      };

      let shownPromise = BrowserTestUtils.waitForEvent(
        PopupNotifications.panel,
        "popupshown"
      );
      TestPrompt.prompt();
      await shownPromise;
      let notification = PopupNotifications.getNotification(
        kTestNotificationID,
        browser
      );
      Assert.ok(notification, "Should have gotten the notification");

      Assert.equal(
        notification.message,
        kTestMessage,
        "Should be showing the right message"
      );
      Assert.equal(
        notification.mainAction.label,
        mainAction.label,
        "The main action should have the right label"
      );
      Assert.equal(
        notification.mainAction.accessKey,
        mainAction.accessKey,
        "The main action should have the right access key"
      );
      Assert.equal(
        notification.secondaryActions.length,
        1,
        "There should only be 1 secondary action"
      );
      Assert.equal(
        notification.secondaryActions[0].label,
        secondaryAction.label,
        "The secondary action should have the right label"
      );
      Assert.equal(
        notification.secondaryActions[0].accessKey,
        secondaryAction.accessKey,
        "The secondary action should have the right access key"
      );
      Assert.ok(
        notification.options.displayURI.equals(mockRequest.principal.URI),
        "Should be showing the URI of the requesting page"
      );

      let removePromise = BrowserTestUtils.waitForEvent(
        PopupNotifications.panel,
        "popuphidden"
      );
      notification.remove();
      await removePromise;
    }
  );
});

/**
 * Tests that if the PermissionPrompt sets displayURI to false in popupOptions,
 * then there is no URI shown on the popupnotification.
 */
add_task(async function test_permission_prompt_for_popupOptions() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "http://example.com/",
    },
    async function(browser) {
      const kTestNotificationID = "test-notification";
      const kTestMessage = "Test message";
      let mainAction = {
        label: "Main",
        accessKey: "M",
      };
      let secondaryAction = {
        label: "Secondary",
        accessKey: "S",
      };

      let mockRequest = makeMockPermissionRequest(browser);
      let TestPrompt = {
        __proto__: PermissionUI.PermissionPromptForRequestPrototype,
        request: mockRequest,
        notificationID: kTestNotificationID,
        message: kTestMessage,
        promptActions: [mainAction, secondaryAction],
        popupOptions: {
          displayURI: false,
        },
      };

      let shownPromise = BrowserTestUtils.waitForEvent(
        PopupNotifications.panel,
        "popupshown"
      );
      TestPrompt.prompt();
      await shownPromise;
      let notification = PopupNotifications.getNotification(
        kTestNotificationID,
        browser
      );

      Assert.ok(
        !notification.options.displayURI,
        "Should not show the URI of the requesting page"
      );

      let removePromise = BrowserTestUtils.waitForEvent(
        PopupNotifications.panel,
        "popuphidden"
      );
      notification.remove();
      await removePromise;
    }
  );
});

/**
 * Tests that if the PermissionPrompt has the permissionKey
 * set that permissions can be set properly by the user. Also
 * ensures that callbacks for promptActions are properly fired.
 */
add_task(async function test_with_permission_key() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "http://example.com",
    },
    async function(browser) {
      const kTestNotificationID = "test-notification";
      const kTestMessage = "Test message";
      const kTestPermissionKey = "test-permission-key";

      let allowed = false;
      let mainAction = {
        label: "Allow",
        accessKey: "M",
        action: SitePermissions.ALLOW,
        callback() {
          allowed = true;
        },
      };

      let denied = false;
      let secondaryAction = {
        label: "Deny",
        accessKey: "D",
        action: SitePermissions.BLOCK,
        callback() {
          denied = true;
        },
      };

      let mockRequest = makeMockPermissionRequest(browser);
      let principal = mockRequest.principal;
      registerCleanupFunction(function() {
        PermissionTestUtils.remove(principal.URI, kTestPermissionKey);
      });

      let TestPrompt = {
        __proto__: PermissionUI.PermissionPromptForRequestPrototype,
        request: mockRequest,
        notificationID: kTestNotificationID,
        permissionKey: kTestPermissionKey,
        message: kTestMessage,
        promptActions: [mainAction, secondaryAction],
        popupOptions: {
          checkbox: {
            label: "Remember this decision",
            show: true,
            checked: true,
          },
        },
      };

      let shownPromise = BrowserTestUtils.waitForEvent(
        PopupNotifications.panel,
        "popupshown"
      );
      TestPrompt.prompt();
      await shownPromise;
      let notification = PopupNotifications.getNotification(
        kTestNotificationID,
        browser
      );
      Assert.ok(notification, "Should have gotten the notification");

      let curPerm = SitePermissions.getForPrincipal(
        principal,
        kTestPermissionKey,
        browser
      );
      Assert.equal(
        curPerm.state,
        SitePermissions.UNKNOWN,
        "Should be no permission set to begin with."
      );

      // First test denying the permission request without the checkbox checked.
      let popupNotification = getPopupNotificationNode();
      popupNotification.checkbox.checked = false;

      Assert.equal(
        notification.secondaryActions.length,
        1,
        "There should only be 1 secondary action"
      );
      await clickSecondaryAction();
      curPerm = SitePermissions.getForPrincipal(
        principal,
        kTestPermissionKey,
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
      // Try getting the permission without passing the browser object (should fail).
      curPerm = PermissionTestUtils.getPermissionObject(
        principal.URI,
        kTestPermissionKey
      );
      Assert.equal(
        curPerm,
        null,
        "Should have made no permanent permission entry"
      );
      Assert.ok(denied, "The secondaryAction callback should have fired");
      Assert.ok(!allowed, "The mainAction callback should not have fired");
      Assert.ok(
        mockRequest._cancelled,
        "The request should have been cancelled"
      );
      Assert.ok(
        !mockRequest._allowed,
        "The request should not have been allowed"
      );

      // Clear the permission and pretend we never denied
      SitePermissions.removeFromPrincipal(
        principal,
        kTestPermissionKey,
        browser
      );
      denied = false;
      mockRequest._cancelled = false;

      // Bring the PopupNotification back up now...
      shownPromise = BrowserTestUtils.waitForEvent(
        PopupNotifications.panel,
        "popupshown"
      );
      TestPrompt.prompt();
      await shownPromise;

      // Test denying the permission request.
      Assert.equal(
        notification.secondaryActions.length,
        1,
        "There should only be 1 secondary action"
      );
      await clickSecondaryAction();
      curPerm = PermissionTestUtils.getPermissionObject(
        principal.URI,
        kTestPermissionKey
      );
      Assert.equal(
        curPerm.capability,
        Services.perms.DENY_ACTION,
        "Should have denied the action"
      );
      Assert.equal(curPerm.expireTime, 0, "Deny should be permanent");
      Assert.ok(denied, "The secondaryAction callback should have fired");
      Assert.ok(!allowed, "The mainAction callback should not have fired");
      Assert.ok(
        mockRequest._cancelled,
        "The request should have been cancelled"
      );
      Assert.ok(
        !mockRequest._allowed,
        "The request should not have been allowed"
      );

      // Clear the permission and pretend we never denied
      PermissionTestUtils.remove(principal.URI, kTestPermissionKey);
      denied = false;
      mockRequest._cancelled = false;

      // Bring the PopupNotification back up now...
      shownPromise = BrowserTestUtils.waitForEvent(
        PopupNotifications.panel,
        "popupshown"
      );
      TestPrompt.prompt();
      await shownPromise;

      // Test allowing the permission request.
      await clickMainAction();
      curPerm = PermissionTestUtils.getPermissionObject(
        principal.URI,
        kTestPermissionKey
      );
      Assert.equal(
        curPerm.capability,
        Services.perms.ALLOW_ACTION,
        "Should have allowed the action"
      );
      Assert.equal(curPerm.expireTime, 0, "Allow should be permanent");
      Assert.ok(!denied, "The secondaryAction callback should not have fired");
      Assert.ok(allowed, "The mainAction callback should have fired");
      Assert.ok(
        !mockRequest._cancelled,
        "The request should not have been cancelled"
      );
      Assert.ok(mockRequest._allowed, "The request should have been allowed");
    }
  );
});

/**
 * Tests that the onBeforeShow method will be called before
 * the popup appears.
 */
add_task(async function test_on_before_show() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "http://example.com",
    },
    async function(browser) {
      const kTestNotificationID = "test-notification";
      const kTestMessage = "Test message";

      let mainAction = {
        label: "Test action",
        accessKey: "T",
      };

      let mockRequest = makeMockPermissionRequest(browser);
      let beforeShown = false;

      let TestPrompt = {
        __proto__: PermissionUI.PermissionPromptForRequestPrototype,
        request: mockRequest,
        notificationID: kTestNotificationID,
        message: kTestMessage,
        promptActions: [mainAction],
        onBeforeShow() {
          beforeShown = true;
          return true;
        },
      };

      let shownPromise = BrowserTestUtils.waitForEvent(
        PopupNotifications.panel,
        "popupshown"
      );
      TestPrompt.prompt();
      Assert.ok(beforeShown, "Should have called onBeforeShown");
      await shownPromise;
      let notification = PopupNotifications.getNotification(
        kTestNotificationID,
        browser
      );

      let removePromise = BrowserTestUtils.waitForEvent(
        PopupNotifications.panel,
        "popuphidden"
      );
      notification.remove();
      await removePromise;
    }
  );
});

/**
 * Tests that we can open a PermissionPrompt without wrapping a
 * nsIContentPermissionRequest.
 */
add_task(async function test_no_request() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "http://example.com",
    },
    async function(browser) {
      const kTestNotificationID = "test-notification";
      let allowed = false;
      let mainAction = {
        label: "Allow",
        accessKey: "M",
        callback() {
          allowed = true;
        },
      };

      let denied = false;
      let secondaryAction = {
        label: "Deny",
        accessKey: "D",
        callback() {
          denied = true;
        },
      };

      const kTestMessage = "Test message with no request";
      let principal = browser.contentPrincipal;
      let beforeShown = false;

      let TestPrompt = {
        __proto__: PermissionUI.PermissionPromptPrototype,
        notificationID: kTestNotificationID,
        principal,
        browser,
        message: kTestMessage,
        promptActions: [mainAction, secondaryAction],
        onBeforeShow() {
          beforeShown = true;
          return true;
        },
      };

      let shownPromise = BrowserTestUtils.waitForEvent(
        PopupNotifications.panel,
        "popupshown"
      );
      TestPrompt.prompt();
      Assert.ok(beforeShown, "Should have called onBeforeShown");
      await shownPromise;
      let notification = PopupNotifications.getNotification(
        kTestNotificationID,
        browser
      );

      Assert.equal(
        notification.message,
        kTestMessage,
        "Should be showing the right message"
      );
      Assert.equal(
        notification.mainAction.label,
        mainAction.label,
        "The main action should have the right label"
      );
      Assert.equal(
        notification.mainAction.accessKey,
        mainAction.accessKey,
        "The main action should have the right access key"
      );
      Assert.equal(
        notification.secondaryActions.length,
        1,
        "There should only be 1 secondary action"
      );
      Assert.equal(
        notification.secondaryActions[0].label,
        secondaryAction.label,
        "The secondary action should have the right label"
      );
      Assert.equal(
        notification.secondaryActions[0].accessKey,
        secondaryAction.accessKey,
        "The secondary action should have the right access key"
      );
      Assert.ok(
        notification.options.displayURI.equals(principal.URI),
        "Should be showing the URI of the requesting page"
      );

      // First test denying the permission request.
      Assert.equal(
        notification.secondaryActions.length,
        1,
        "There should only be 1 secondary action"
      );
      await clickSecondaryAction();
      Assert.ok(denied, "The secondaryAction callback should have fired");
      Assert.ok(!allowed, "The mainAction callback should not have fired");

      shownPromise = BrowserTestUtils.waitForEvent(
        PopupNotifications.panel,
        "popupshown"
      );
      TestPrompt.prompt();
      await shownPromise;

      // Next test allowing the permission request.
      await clickMainAction();
      Assert.ok(allowed, "The mainAction callback should have fired");
    }
  );
});

/**
 * Tests that when the tab is moved to a different window, the notification
 * is transferred to the new window.
 */
add_task(async function test_window_swap() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "http://example.com",
    },
    async function(browser) {
      const kTestNotificationID = "test-notification";
      const kTestMessage = "Test message";

      let mainAction = {
        label: "Test action",
        accessKey: "T",
      };
      let secondaryAction = {
        label: "Secondary",
        accessKey: "S",
      };

      let mockRequest = makeMockPermissionRequest(browser);

      let TestPrompt = {
        __proto__: PermissionUI.PermissionPromptForRequestPrototype,
        request: mockRequest,
        notificationID: kTestNotificationID,
        message: kTestMessage,
        promptActions: [mainAction, secondaryAction],
      };

      let shownPromise = BrowserTestUtils.waitForEvent(
        PopupNotifications.panel,
        "popupshown"
      );
      TestPrompt.prompt();
      await shownPromise;

      let newWindowOpened = BrowserTestUtils.waitForNewWindow();
      gBrowser.replaceTabWithWindow(gBrowser.selectedTab);
      let newWindow = await newWindowOpened;
      // We may have already opened the panel, because it was open before we moved the tab.
      if (newWindow.PopupNotifications.panel.state != "open") {
        shownPromise = BrowserTestUtils.waitForEvent(
          newWindow.PopupNotifications.panel,
          "popupshown"
        );
        TestPrompt.prompt();
        await shownPromise;
      }

      let notification = newWindow.PopupNotifications.getNotification(
        kTestNotificationID,
        newWindow.gBrowser.selectedBrowser
      );
      Assert.ok(notification, "Should have gotten the notification");

      Assert.equal(
        notification.message,
        kTestMessage,
        "Should be showing the right message"
      );
      Assert.equal(
        notification.mainAction.label,
        mainAction.label,
        "The main action should have the right label"
      );
      Assert.equal(
        notification.mainAction.accessKey,
        mainAction.accessKey,
        "The main action should have the right access key"
      );
      Assert.equal(
        notification.secondaryActions.length,
        1,
        "There should only be 1 secondary action"
      );
      Assert.equal(
        notification.secondaryActions[0].label,
        secondaryAction.label,
        "The secondary action should have the right label"
      );
      Assert.equal(
        notification.secondaryActions[0].accessKey,
        secondaryAction.accessKey,
        "The secondary action should have the right access key"
      );
      Assert.ok(
        notification.options.displayURI.equals(mockRequest.principal.URI),
        "Should be showing the URI of the requesting page"
      );

      await BrowserTestUtils.closeWindow(newWindow);
    }
  );
});
