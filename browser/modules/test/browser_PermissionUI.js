/**
 * These tests test the ability for the PermissionUI module to open
 * permission prompts to the user. It also tests to ensure that
 * add-ons can introduce their own permission prompts.
 */

"use strict";

Cu.import("resource://gre/modules/Integration.jsm", this);
Cu.import("resource:///modules/PermissionUI.jsm", this);

/**
 * Given a <xul:browser> at some non-internal web page,
 * return something that resembles an nsIContentPermissionRequest,
 * using the browsers currently loaded document to get a principal.
 *
 * @param browser (<xul:browser>)
 *        The browser that we'll create a nsIContentPermissionRequest
 *        for.
 * @returns A nsIContentPermissionRequest-ish object.
 */
function makeMockPermissionRequest(browser) {
  let result = {
    types: null,
    principal: browser.contentPrincipal,
    requester: null,
    _cancelled: false,
    cancel() {
      this._cancelled = true;
    },
    _allowed: false,
    allow() {
      this._allowed = true;
    },
    QueryInterface: XPCOMUtils.generateQI([Ci.nsIContentPermissionRequest]),
  };

  // In the e10s-case, nsIContentPermissionRequest will have
  // element defined. window is defined otherwise.
  if (browser.isRemoteBrowser) {
    result.element = browser;
  } else {
    result.window = browser.contentWindow;
  }

  return result;
}

/**
 * For an opened PopupNotification, clicks on the main action,
 * and waits for the panel to fully close.
 *
 * @return {Promise}
 *         Resolves once the panel has fired the "popuphidden"
 *         event.
 */
function clickMainAction() {
  let removePromise =
    BrowserTestUtils.waitForEvent(PopupNotifications.panel, "popuphidden");
  let popupNotification = getPopupNotificationNode();
  popupNotification.button.click();
  return removePromise;
}

/**
 * For an opened PopupNotification, clicks on the secondary action,
 * and waits for the panel to fully close.
 *
 * @return {Promise}
 *         Resolves once the panel has fired the "popuphidden"
 *         event.
 */
function clickSecondaryAction() {
  let removePromise =
    BrowserTestUtils.waitForEvent(PopupNotifications.panel, "popuphidden");
  let popupNotification = getPopupNotificationNode();
  popupNotification.secondaryButton.click();
  return removePromise;
}

/**
 * Makes sure that 1 (and only 1) <xul:popupnotification> is being displayed
 * by PopupNotification, and then returns that <xul:popupnotification>.
 *
 * @return {<xul:popupnotification>}
 */
function getPopupNotificationNode() {
  // PopupNotification is a bit overloaded here, so to be
  // clear, popupNotifications is a list of <xul:popupnotification>
  // nodes.
  let popupNotifications = PopupNotifications.panel.childNodes;
  Assert.equal(popupNotifications.length, 1,
               "Should be showing a <xul:popupnotification>");
  return popupNotifications[0];
}

/**
 * Tests the PermissionPromptForRequest prototype to ensure that a prompt
 * can be displayed. Does not test permission handling.
 */
add_task(function* test_permission_prompt_for_request() {
  yield BrowserTestUtils.withNewTab({
    gBrowser,
    url: "http://example.com/",
  }, function*(browser) {
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

    let shownPromise =
      BrowserTestUtils.waitForEvent(PopupNotifications.panel, "popupshown");
    TestPrompt.prompt();
    yield shownPromise;
    let notification =
      PopupNotifications.getNotification(kTestNotificationID, browser);
    Assert.ok(notification, "Should have gotten the notification");

    Assert.equal(notification.message, kTestMessage,
                 "Should be showing the right message");
    Assert.equal(notification.mainAction.label, mainAction.label,
                 "The main action should have the right label");
    Assert.equal(notification.mainAction.accessKey, mainAction.accessKey,
                 "The main action should have the right access key");
    Assert.equal(notification.secondaryActions.length, 1,
                 "There should only be 1 secondary action");
    Assert.equal(notification.secondaryActions[0].label, secondaryAction.label,
                 "The secondary action should have the right label");
    Assert.equal(notification.secondaryActions[0].accessKey,
                 secondaryAction.accessKey,
                 "The secondary action should have the right access key");
    Assert.ok(notification.options.displayURI.equals(mockRequest.principal.URI),
              "Should be showing the URI of the requesting page");

    let removePromise =
      BrowserTestUtils.waitForEvent(PopupNotifications.panel, "popuphidden");
    notification.remove();
    yield removePromise;
  });
});

/**
 * Tests that if the PermissionPrompt sets displayURI to false in popupOptions,
 * then there is no URI shown on the popupnotification.
 */
add_task(function* test_permission_prompt_for_popupOptions() {
  yield BrowserTestUtils.withNewTab({
    gBrowser,
    url: "http://example.com/",
  }, function*(browser) {
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

    let shownPromise =
      BrowserTestUtils.waitForEvent(PopupNotifications.panel, "popupshown");
    TestPrompt.prompt();
    yield shownPromise;
    let notification =
      PopupNotifications.getNotification(kTestNotificationID, browser);

    Assert.ok(!notification.options.displayURI,
              "Should not show the URI of the requesting page");

    let removePromise =
      BrowserTestUtils.waitForEvent(PopupNotifications.panel, "popuphidden");
    notification.remove();
    yield removePromise;
  });
});

/**
 * Tests that if the PermissionPrompt has the permissionKey
 * set that permissions can be set properly by the user. Also
 * ensures that callbacks for promptActions are properly fired.
 */
add_task(function* test_with_permission_key() {
  yield BrowserTestUtils.withNewTab({
    gBrowser,
    url: "http://example.com",
  }, function*(browser) {
    const kTestNotificationID = "test-notification";
    const kTestMessage = "Test message";
    const kTestPermissionKey = "test-permission-key";

    let allowed = false;
    let mainAction = {
      label: "Allow",
      accessKey: "M",
      action: Ci.nsIPermissionManager.ALLOW_ACTION,
      expiryType: Ci.nsIPermissionManager.EXPIRE_SESSION,
      callback: function() {
        allowed = true;
      }
    };

    let denied = false;
    let secondaryAction = {
      label: "Deny",
      accessKey: "D",
      action: Ci.nsIPermissionManager.DENY_ACTION,
      expiryType: Ci.nsIPermissionManager.EXPIRE_SESSION,
      callback: function() {
        denied = true;
      }
    };

    let mockRequest = makeMockPermissionRequest(browser);
    let principal = mockRequest.principal;
    registerCleanupFunction(function() {
      Services.perms.removeFromPrincipal(principal, kTestPermissionKey);
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
          checked: true
        }
      }
    };

    let shownPromise =
      BrowserTestUtils.waitForEvent(PopupNotifications.panel, "popupshown");
    TestPrompt.prompt();
    yield shownPromise;
    let notification =
      PopupNotifications.getNotification(kTestNotificationID, browser);
    Assert.ok(notification, "Should have gotten the notification");

    let curPerm =
      Services.perms.testPermissionFromPrincipal(principal,
                                                 kTestPermissionKey);
    Assert.equal(curPerm, Ci.nsIPermissionManager.UNKNOWN_ACTION,
                 "Should be no permission set to begin with.");

    // First test denying the permission request.
    Assert.equal(notification.secondaryActions.length, 1,
                 "There should only be 1 secondary action");
    yield clickSecondaryAction();
    curPerm = Services.perms.testPermissionFromPrincipal(principal,
                                                         kTestPermissionKey);
    Assert.equal(curPerm, Ci.nsIPermissionManager.DENY_ACTION,
                 "Should have denied the action");
    Assert.ok(denied, "The secondaryAction callback should have fired");
    Assert.ok(!allowed, "The mainAction callback should not have fired");
    Assert.ok(mockRequest._cancelled,
              "The request should have been cancelled");
    Assert.ok(!mockRequest._allowed,
              "The request should not have been allowed");

    // Clear the permission and pretend we never denied
    Services.perms.removeFromPrincipal(principal, kTestPermissionKey);
    denied = false;
    mockRequest._cancelled = false;

    // Bring the PopupNotification back up now...
    shownPromise =
      BrowserTestUtils.waitForEvent(PopupNotifications.panel, "popupshown");
    TestPrompt.prompt();
    yield shownPromise;

    // Next test allowing the permission request.
    yield clickMainAction();
    curPerm = Services.perms.testPermissionFromPrincipal(principal,
                                                         kTestPermissionKey);
    Assert.equal(curPerm, Ci.nsIPermissionManager.ALLOW_ACTION,
                 "Should have allowed the action");
    Assert.ok(!denied, "The secondaryAction callback should not have fired");
    Assert.ok(allowed, "The mainAction callback should have fired");
    Assert.ok(!mockRequest._cancelled,
              "The request should not have been cancelled");
    Assert.ok(mockRequest._allowed,
              "The request should have been allowed");
  });
});

/**
 * Tests that the onBeforeShow method will be called before
 * the popup appears.
 */
add_task(function* test_on_before_show() {
  yield BrowserTestUtils.withNewTab({
    gBrowser,
    url: "http://example.com",
  }, function*(browser) {
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
      }
    };

    let shownPromise =
      BrowserTestUtils.waitForEvent(PopupNotifications.panel, "popupshown");
    TestPrompt.prompt();
    Assert.ok(beforeShown, "Should have called onBeforeShown");
    yield shownPromise;
    let notification =
      PopupNotifications.getNotification(kTestNotificationID, browser);

    let removePromise =
      BrowserTestUtils.waitForEvent(PopupNotifications.panel, "popuphidden");
    notification.remove();
    yield removePromise;
  });
});

/**
 * Tests that we can open a PermissionPrompt without wrapping a
 * nsIContentPermissionRequest.
 */
add_task(function* test_no_request() {
  yield BrowserTestUtils.withNewTab({
    gBrowser,
    url: "http://example.com",
  }, function*(browser) {
    const kTestNotificationID = "test-notification";
    let allowed = false;
    let mainAction = {
      label: "Allow",
      accessKey: "M",
      callback: function() {
        allowed = true;
      }
    };

    let denied = false;
    let secondaryAction = {
      label: "Deny",
      accessKey: "D",
      callback: function() {
        denied = true;
      }
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
      }
    };

    let shownPromise =
      BrowserTestUtils.waitForEvent(PopupNotifications.panel, "popupshown");
    TestPrompt.prompt();
    Assert.ok(beforeShown, "Should have called onBeforeShown");
    yield shownPromise;
    let notification =
      PopupNotifications.getNotification(kTestNotificationID, browser);

    Assert.equal(notification.message, kTestMessage,
                 "Should be showing the right message");
    Assert.equal(notification.mainAction.label, mainAction.label,
                 "The main action should have the right label");
    Assert.equal(notification.mainAction.accessKey, mainAction.accessKey,
                 "The main action should have the right access key");
    Assert.equal(notification.secondaryActions.length, 1,
                 "There should only be 1 secondary action");
    Assert.equal(notification.secondaryActions[0].label, secondaryAction.label,
                 "The secondary action should have the right label");
    Assert.equal(notification.secondaryActions[0].accessKey,
                 secondaryAction.accessKey,
                 "The secondary action should have the right access key");
    Assert.ok(notification.options.displayURI.equals(principal.URI),
              "Should be showing the URI of the requesting page");

    // First test denying the permission request.
    Assert.equal(notification.secondaryActions.length, 1,
                 "There should only be 1 secondary action");
    yield clickSecondaryAction();
    Assert.ok(denied, "The secondaryAction callback should have fired");
    Assert.ok(!allowed, "The mainAction callback should not have fired");

    shownPromise =
      BrowserTestUtils.waitForEvent(PopupNotifications.panel, "popupshown");
    TestPrompt.prompt();
    yield shownPromise;

    // Next test allowing the permission request.
    yield clickMainAction();
    Assert.ok(allowed, "The mainAction callback should have fired");
  });
});
