/**
 * These tests test the ability for the PermissionUI module to open
 * permission prompts to the user. It also tests to ensure that
 * add-ons can introduce their own permission prompts.
 */

"use strict";

Cu.import("resource://gre/modules/Integration.jsm", this);
Cu.import("resource:///modules/PermissionUI.jsm", this);
Cu.import("resource:///modules/SitePermissions.jsm", this);

// Tests that GeolocationPermissionPrompt works as expected
add_task(function* test_geo_permission_prompt() {
  yield testPrompt(PermissionUI.GeolocationPermissionPrompt);
});

// Tests that DesktopNotificationPermissionPrompt works as expected
add_task(function* test_desktop_notification_permission_prompt() {
  yield testPrompt(PermissionUI.DesktopNotificationPermissionPrompt);
});

function* testPrompt(Prompt) {
  yield BrowserTestUtils.withNewTab({
    gBrowser,
    url: "http://example.com",
  }, function*(browser) {
    let mockRequest = makeMockPermissionRequest(browser);
    let principal = mockRequest.principal;
    let TestPrompt = new Prompt(mockRequest);
    let permissionKey = TestPrompt.permissionKey;

    registerCleanupFunction(function() {
      SitePermissions.remove(principal.URI, permissionKey);
    });

    let shownPromise =
      BrowserTestUtils.waitForEvent(PopupNotifications.panel, "popupshown");
    TestPrompt.prompt();
    yield shownPromise;
    let notification =
      PopupNotifications.getNotification(TestPrompt.notificationID, browser);
    Assert.ok(notification, "Should have gotten the notification");

    let curPerm = SitePermissions.get(principal.URI, permissionKey, browser);
    Assert.equal(curPerm.state, SitePermissions.UNKNOWN,
                 "Should be no permission set to begin with.");

    // First test denying the permission request without the checkbox checked.
    let popupNotification = getPopupNotificationNode();
    popupNotification.checkbox.checked = false;

    Assert.equal(notification.secondaryActions.length, 1,
                 "There should only be 1 secondary action");
    yield clickSecondaryAction();
    curPerm = SitePermissions.get(principal.URI, permissionKey, browser);
    Assert.deepEqual(curPerm, {
                       state: SitePermissions.BLOCK,
                       scope: SitePermissions.SCOPE_TEMPORARY,
                     }, "Should have denied the action temporarily");

    Assert.ok(mockRequest._cancelled,
              "The request should have been cancelled");
    Assert.ok(!mockRequest._allowed,
              "The request should not have been allowed");

    SitePermissions.remove(principal.URI, permissionKey, browser);
    mockRequest._cancelled = false;

    // Bring the PopupNotification back up now...
    shownPromise =
      BrowserTestUtils.waitForEvent(PopupNotifications.panel, "popupshown");
    TestPrompt.prompt();
    yield shownPromise;

    // Test denying the permission request with the checkbox checked.
    popupNotification = getPopupNotificationNode();
    popupNotification.checkbox.checked = true;

    Assert.equal(notification.secondaryActions.length, 1,
                 "There should only be 1 secondary action");
    yield clickSecondaryAction();
    curPerm = SitePermissions.get(principal.URI, permissionKey);
    Assert.deepEqual(curPerm, {
                       state: SitePermissions.BLOCK,
                       scope: SitePermissions.SCOPE_PERSISTENT
                     }, "Should have denied the action permanently");
    Assert.ok(mockRequest._cancelled,
              "The request should have been cancelled");
    Assert.ok(!mockRequest._allowed,
              "The request should not have been allowed");

    SitePermissions.remove(principal.URI, permissionKey);
    mockRequest._cancelled = false;

    // Bring the PopupNotification back up now...
    shownPromise =
      BrowserTestUtils.waitForEvent(PopupNotifications.panel, "popupshown");
    TestPrompt.prompt();
    yield shownPromise;

    // Test allowing the permission request with the checkbox checked.
    popupNotification = getPopupNotificationNode();
    popupNotification.checkbox.checked = true;

    yield clickMainAction();
    curPerm = SitePermissions.get(principal.URI, permissionKey);
    Assert.deepEqual(curPerm, {
                       state: SitePermissions.ALLOW,
                       scope: SitePermissions.SCOPE_PERSISTENT
                     }, "Should have allowed the action permanently");
    Assert.ok(!mockRequest._cancelled,
              "The request should not have been cancelled");
    Assert.ok(mockRequest._allowed,
              "The request should have been allowed");
  });
}

