"use strict";

/**
 * Tests that when we click on the main call-to-action of the doorhanger, the provided
 * action is called, and the doorhanger removed.
 */
add_task(function* testMainActionCalled() {
  let options = {
    gBrowser: window.gBrowser,
    url: "about:blank"
  };

  yield BrowserTestUtils.withNewTab(options, function*(browser) {
    let doc = browser.ownerDocument;

    is(PanelUI.notificationPanel.state, "closed", "update-manual doorhanger is closed.");
    let mainActionCalled = false;
    let mainAction = {
      callback: () => { mainActionCalled = true; }
    };
    PanelUI.showNotification("update-manual", mainAction);

    isnot(PanelUI.notificationPanel.state, "closed", "update-manual doorhanger is showing.");
    let notifications = [...PanelUI.notificationPanel.children].filter(n => !n.hidden);
    is(notifications.length, 1, "PanelUI doorhanger is only displaying one notification.");
    let doorhanger = notifications[0];
    is(doorhanger.id, "appMenu-update-manual-notification", "PanelUI is displaying the update-manual notification.");

    let button = doc.getAnonymousElementByAttribute(doorhanger, "anonid", "button");
    button.click();

    ok(mainActionCalled, "Main action callback was called");
    is(PanelUI.notificationPanel.state, "closed", "update-manual doorhanger is closed.");
    is(PanelUI.menuButton.hasAttribute("badge-status"), false, "Should not have a badge status");
  });
});

/**
 * Tests that when we try to show a notification in a background window, it
 * does not display until the window comes back into the foreground. However,
 * it should display a badge.
 */
add_task(function* testDoesNotShowDoorhangerForBackgroundWindow() {
  let options = {
    gBrowser: window.gBrowser,
    url: "about:blank"
  };

  yield BrowserTestUtils.withNewTab(options, function*(browser) {
    let doc = browser.ownerDocument;

    let win = yield BrowserTestUtils.openNewBrowserWindow();
    let mainActionCalled = false;
    let mainAction = {
      callback: () => { mainActionCalled = true; }
    };
    PanelUI.showNotification("update-manual", mainAction);
    is(PanelUI.notificationPanel.state, "closed", "The background window's doorhanger is closed.");
    is(PanelUI.menuButton.hasAttribute("badge-status"), true, "The background window has a badge.");

    yield BrowserTestUtils.closeWindow(win);
    yield SimpleTest.promiseFocus(window);
    isnot(PanelUI.notificationPanel.state, "closed", "update-manual doorhanger is showing.");
    let notifications = [...PanelUI.notificationPanel.children].filter(n => !n.hidden);
    is(notifications.length, 1, "PanelUI doorhanger is only displaying one notification.");
    let doorhanger = notifications[0];
    is(doorhanger.id, "appMenu-update-manual-notification", "PanelUI is displaying the update-manual notification.");

    let button = doc.getAnonymousElementByAttribute(doorhanger, "anonid", "button");
    button.click();

    ok(mainActionCalled, "Main action callback was called");
    is(PanelUI.notificationPanel.state, "closed", "update-manual doorhanger is closed.");
    is(PanelUI.menuButton.hasAttribute("badge-status"), false, "Should not have a badge status");
  });
});

/**
 * Tests that when we try to show a notification in a background window and in
 * a foreground window, if the foreground window's main action is called, the
 * background window's doorhanger will be removed.
 */
add_task(function* testBackgroundWindowNotificationsAreRemovedByForeground() {
  let options = {
    gBrowser: window.gBrowser,
    url: "about:blank"
  };

  yield BrowserTestUtils.withNewTab(options, function*(browser) {
    let win = yield BrowserTestUtils.openNewBrowserWindow();
    PanelUI.showNotification("update-manual", {callback() {}});
    win.PanelUI.showNotification("update-manual", {callback() {}});
    let doc = win.gBrowser.ownerDocument;
    let notifications = [...win.PanelUI.notificationPanel.children].filter(n => !n.hidden);
    let doorhanger = notifications[0];
    let button = doc.getAnonymousElementByAttribute(doorhanger, "anonid", "button");
    button.click();

    yield BrowserTestUtils.closeWindow(win);
    yield SimpleTest.promiseFocus(window);

    is(PanelUI.notificationPanel.state, "closed", "update-manual doorhanger is closed.");
    is(PanelUI.menuButton.hasAttribute("badge-status"), false, "Should not have a badge status");
  });
});

/**
 * Tests that when we try to show a notification in a background window and in
 * a foreground window, if the foreground window's doorhanger is dismissed,
 * the background window's doorhanger will also be dismissed once the window
 * regains focus.
 */
add_task(function* testBackgroundWindowNotificationsAreDismissedByForeground() {
  let options = {
    gBrowser: window.gBrowser,
    url: "about:blank"
  };

  yield BrowserTestUtils.withNewTab(options, function*(browser) {
    let win = yield BrowserTestUtils.openNewBrowserWindow();
    PanelUI.showNotification("update-manual", {callback() {}});
    win.PanelUI.showNotification("update-manual", {callback() {}});
    let doc = win.gBrowser.ownerDocument;
    let notifications = [...win.PanelUI.notificationPanel.children].filter(n => !n.hidden);
    let doorhanger = notifications[0];
    let button = doc.getAnonymousElementByAttribute(doorhanger, "anonid", "secondarybutton");
    button.click();

    yield BrowserTestUtils.closeWindow(win);
    yield SimpleTest.promiseFocus(window);

    is(PanelUI.notificationPanel.state, "closed", "The background window's doorhanger is closed.");
    is(PanelUI.menuButton.hasAttribute("badge-status"), true,
       "The dismissed notification should still have a badge status");

    PanelUI.removeNotification(/.*/);
  });
});

/**
 * This tests that when we click the secondary action for a notification,
 * it will display the badge for that notification on the PanelUI menu button.
 * Once we click on this button, we should see an item in the menu which will
 * call our main action.
 */
add_task(function* testSecondaryActionWorkflow() {
  let options = {
    gBrowser: window.gBrowser,
    url: "about:blank"
  };

  yield BrowserTestUtils.withNewTab(options, function*(browser) {
    let doc = browser.ownerDocument;

    is(PanelUI.notificationPanel.state, "closed", "update-manual doorhanger is closed.");

    let mainActionCalled = false;
    let mainAction = {
      callback: () => { mainActionCalled = true; },
    };
    PanelUI.showNotification("update-manual", mainAction);

    isnot(PanelUI.notificationPanel.state, "closed", "update-manual doorhanger is showing.");
    let notifications = [...PanelUI.notificationPanel.children].filter(n => !n.hidden);
    is(notifications.length, 1, "PanelUI doorhanger is only displaying one notification.");
    let doorhanger = notifications[0];
    is(doorhanger.id, "appMenu-update-manual-notification", "PanelUI is displaying the update-manual notification.");

    let secondaryActionButton = doc.getAnonymousElementByAttribute(doorhanger, "anonid", "secondarybutton");
    secondaryActionButton.click();

    is(PanelUI.notificationPanel.state, "closed", "update-manual doorhanger is closed.");

    is(PanelUI.menuButton.getAttribute("badge-status"), "update-manual", "Badge is displaying on PanelUI button.");

    yield PanelUI.show();
    isnot(PanelUI.menuButton.getAttribute("badge-status"), "update-manual", "Badge is hidden on PanelUI button.");
    let menuItem = PanelUI.mainView.querySelector(".panel-banner-item");
    is(menuItem.label, menuItem.getAttribute("label-update-manual"), "Showing correct label");
    is(menuItem.hidden, false, "update-manual menu item is showing.");

    yield PanelUI.hide();
    is(PanelUI.menuButton.getAttribute("badge-status"), "update-manual", "Badge is shown on PanelUI button.");

    yield PanelUI.show();
    menuItem.click();
    ok(mainActionCalled, "Main action callback was called");

    PanelUI.removeNotification(/.*/);
  });
});

/**
 * We want to ensure a few things with this:
 * - Adding a doorhanger will make a badge disappear
 * - once the notification for the doorhanger is resolved (removed, not just dismissed),
 *   then we display any other badges that are remaining.
 */
add_task(function* testInteractionWithBadges() {
  yield BrowserTestUtils.withNewTab("about:blank", function*(browser) {
    let doc = browser.ownerDocument;

    PanelUI.showBadgeOnlyNotification("fxa-needs-authentication");
    is(PanelUI.menuButton.getAttribute("badge-status"), "fxa-needs-authentication", "Fxa badge is shown on PanelUI button.");
    is(PanelUI.notificationPanel.state, "closed", "update-manual doorhanger is closed.");

    let mainActionCalled = false;
    let mainAction = {
      callback: () => { mainActionCalled = true; },
    };
    PanelUI.showNotification("update-manual", mainAction);

    isnot(PanelUI.menuButton.getAttribute("badge-status"), "fxa-needs-authentication", "Fxa badge is hidden on PanelUI button.");
    isnot(PanelUI.notificationPanel.state, "closed", "update-manual doorhanger is showing.");
    let notifications = [...PanelUI.notificationPanel.children].filter(n => !n.hidden);
    is(notifications.length, 1, "PanelUI doorhanger is only displaying one notification.");
    let doorhanger = notifications[0];
    is(doorhanger.id, "appMenu-update-manual-notification", "PanelUI is displaying the update-manual notification.");

    let secondaryActionButton = doc.getAnonymousElementByAttribute(doorhanger, "anonid", "secondarybutton");
    secondaryActionButton.click();

    is(PanelUI.notificationPanel.state, "closed", "update-manual doorhanger is closed.");

    is(PanelUI.menuButton.getAttribute("badge-status"), "update-manual", "Badge is displaying on PanelUI button.");

    yield PanelUI.show();
    isnot(PanelUI.menuButton.getAttribute("badge-status"), "update-manual", "Badge is hidden on PanelUI button.");
    let menuItem = PanelUI.mainView.querySelector(".panel-banner-item");
    is(menuItem.label, menuItem.getAttribute("label-update-manual"), "Showing correct label");
    is(menuItem.hidden, false, "update-manual menu item is showing.");

    menuItem.click();
    ok(mainActionCalled, "Main action callback was called");

    is(PanelUI.menuButton.getAttribute("badge-status"), "fxa-needs-authentication", "Fxa badge is shown on PanelUI button.");
    PanelUI.removeNotification(/.*/);
    is(PanelUI.menuButton.hasAttribute("badge-status"), false, "Should not have a badge status");
  });
});

/**
 * This tests that adding a badge will not dismiss any existing doorhangers.
 */
add_task(function* testAddingBadgeWhileDoorhangerIsShowing() {
  yield BrowserTestUtils.withNewTab("about:blank", function*(browser) {
    let doc = browser.ownerDocument;

    is(PanelUI.notificationPanel.state, "closed", "update-manual doorhanger is closed.");
    let mainActionCalled = false;
    let mainAction = {
      callback: () => { mainActionCalled = true; }
    };
    PanelUI.showNotification("update-manual", mainAction);
    PanelUI.showBadgeOnlyNotification("fxa-needs-authentication");

    isnot(PanelUI.menuButton.getAttribute("badge-status"), "fxa-needs-authentication", "Fxa badge is hidden on PanelUI button.");
    isnot(PanelUI.notificationPanel.state, "closed", "update-manual doorhanger is showing.");
    let notifications = [...PanelUI.notificationPanel.children].filter(n => !n.hidden);
    is(notifications.length, 1, "PanelUI doorhanger is only displaying one notification.");
    let doorhanger = notifications[0];
    is(doorhanger.id, "appMenu-update-manual-notification", "PanelUI is displaying the update-manual notification.");

    let mainActionButton = doc.getAnonymousElementByAttribute(doorhanger, "anonid", "button");
    mainActionButton.click();

    ok(mainActionCalled, "Main action callback was called");
    is(PanelUI.notificationPanel.state, "closed", "update-manual doorhanger is closed.");
    is(PanelUI.menuButton.getAttribute("badge-status"), "fxa-needs-authentication", "Fxa badge is shown on PanelUI button.");
    PanelUI.removeNotification(/.*/);
    is(PanelUI.menuButton.hasAttribute("badge-status"), false, "Should not have a badge status");
  });
});

/**
 * Tests that badges operate like a stack.
 */
add_task(function* testMultipleBadges() {
  yield BrowserTestUtils.withNewTab("about:blank", function*(browser) {
    let doc = browser.ownerDocument;
    let menuButton = doc.getElementById("PanelUI-menu-button");

    is(menuButton.hasAttribute("badge-status"), false, "Should not have a badge status");
    is(menuButton.hasAttribute("badge"), false, "Should not have the badge attribute set");

    PanelUI.showBadgeOnlyNotification("fxa-needs-authentication");
    is(menuButton.getAttribute("badge-status"), "fxa-needs-authentication", "Should have fxa-needs-authentication badge status");

    PanelUI.showBadgeOnlyNotification("update-succeeded");
    is(menuButton.getAttribute("badge-status"), "update-succeeded", "Should have update-succeeded badge status (update > fxa)");

    PanelUI.showBadgeOnlyNotification("update-failed");
    is(menuButton.getAttribute("badge-status"), "update-failed", "Should have update-failed badge status");

    PanelUI.showBadgeOnlyNotification("download-severe");
    is(menuButton.getAttribute("badge-status"), "download-severe", "Should have download-severe badge status");

    PanelUI.showBadgeOnlyNotification("download-warning");
    is(menuButton.getAttribute("badge-status"), "download-warning", "Should have download-warning badge status");

    PanelUI.removeNotification(/^download-/);
    is(menuButton.getAttribute("badge-status"), "update-failed", "Should have update-failed badge status");

    PanelUI.removeNotification(/^update-/);
    is(menuButton.getAttribute("badge-status"), "fxa-needs-authentication", "Should have fxa-needs-authentication badge status");

    PanelUI.removeNotification(/^fxa-/);
    is(menuButton.hasAttribute("badge-status"), false, "Should not have a badge status");

    yield PanelUI.show();
    is(menuButton.hasAttribute("badge-status"), false, "Should not have a badge status (Hamburger menu opened)");
    PanelUI.hide();

    PanelUI.showBadgeOnlyNotification("fxa-needs-authentication");
    PanelUI.showBadgeOnlyNotification("update-succeeded");
    PanelUI.removeNotification(/.*/);
    is(menuButton.hasAttribute("badge-status"), false, "Should not have a badge status");
  });
});

/**
 * Tests that non-badges also operate like a stack.
 */
add_task(function* testMultipleNonBadges() {
  yield BrowserTestUtils.withNewTab("about:blank", function*(browser) {
    let doc = browser.ownerDocument;

    is(PanelUI.notificationPanel.state, "closed", "update-manual doorhanger is closed.");

    let updateManualAction = {
        called: false,
        callback: () => { updateManualAction.called = true; },
    };
    let updateRestartAction = {
        called: false,
        callback: () => { updateRestartAction.called = true; },
    };

    PanelUI.showNotification("update-manual", updateManualAction);

    let notifications;
    let doorhanger;

    isnot(PanelUI.notificationPanel.state, "closed", "Doorhanger is showing.");
    notifications = [...PanelUI.notificationPanel.children].filter(n => !n.hidden);
    is(notifications.length, 1, "PanelUI doorhanger is only displaying one notification.");
    doorhanger = notifications[0];
    is(doorhanger.id, "appMenu-update-manual-notification", "PanelUI is displaying the update-manual notification.");

    PanelUI.showNotification("update-restart", updateRestartAction);

    isnot(PanelUI.notificationPanel.state, "closed", "Doorhanger is showing.");
    notifications = [...PanelUI.notificationPanel.children].filter(n => !n.hidden);
    is(notifications.length, 1, "PanelUI doorhanger is only displaying one notification.");
    doorhanger = notifications[0];
    is(doorhanger.id, "appMenu-update-restart-notification", "PanelUI is displaying the update-restart notification.");

    let secondaryActionButton = doc.getAnonymousElementByAttribute(doorhanger, "anonid", "secondarybutton");
    secondaryActionButton.click();

    is(PanelUI.notificationPanel.state, "closed", "update-manual doorhanger is closed.");
    is(PanelUI.menuButton.getAttribute("badge-status"), "update-restart", "update-restart badge is displaying on PanelUI button.");

    yield PanelUI.show();
    isnot(PanelUI.menuButton.getAttribute("badge-status"), "update-restart", "update-restart badge is hidden on PanelUI button.");
    let menuItem = PanelUI.mainView.querySelector(".panel-banner-item");
    is(menuItem.label, menuItem.getAttribute("label-update-restart"), "Showing correct label");
    is(menuItem.hidden, false, "update-restart menu item is showing.");

    menuItem.click();
    ok(updateRestartAction.called, "update-restart main action callback was called");

    is(PanelUI.notificationPanel.state, "closed", "update-manual doorhanger is closed.");
    is(PanelUI.menuButton.getAttribute("badge-status"), "update-manual", "update-manual badge is displaying on PanelUI button.");

    yield PanelUI.show();
    isnot(PanelUI.menuButton.getAttribute("badge-status"), "update-manual", "update-manual badge is hidden on PanelUI button.");
    is(menuItem.label, menuItem.getAttribute("label-update-manual"), "Showing correct label");
    is(menuItem.hidden, false, "update-manual menu item is showing.");

    menuItem.click();
    ok(updateManualAction.called, "update-manual main action callback was called");
  });
});

add_task(function* testFullscreen() {
  let doc = document;

  is(PanelUI.notificationPanel.state, "closed", "update-manual doorhanger is closed.");
  let mainActionCalled = false;
  let mainAction = {
    callback: () => { mainActionCalled = true; }
  };
  PanelUI.showNotification("update-manual", mainAction);

  isnot(PanelUI.notificationPanel.state, "closed", "update-manual doorhanger is showing.");
  let notifications = [...PanelUI.notificationPanel.children].filter(n => !n.hidden);
  is(notifications.length, 1, "PanelUI doorhanger is only displaying one notification.");
  let doorhanger = notifications[0];
  is(doorhanger.id, "appMenu-update-manual-notification", "PanelUI is displaying the update-manual notification.");

  let popuphiddenPromise = BrowserTestUtils.waitForEvent(PanelUI.notificationPanel, "popuphidden");
  EventUtils.synthesizeKey("VK_F11", {});
  yield popuphiddenPromise;
  yield new Promise(executeSoon);
  is(PanelUI.notificationPanel.state, "closed", "update-manual doorhanger is closed.");

  window.FullScreen.showNavToolbox();
  is(PanelUI.menuButton.getAttribute("badge-status"), "update-manual", "Badge is displaying on PanelUI button.");

  let popupshownPromise = BrowserTestUtils.waitForEvent(PanelUI.notificationPanel, "popupshown");
  EventUtils.synthesizeKey("VK_F11", {});
  yield popupshownPromise;
  yield new Promise(executeSoon);
  isnot(PanelUI.notificationPanel.state, "closed", "update-manual doorhanger is showing.");
  isnot(PanelUI.menuButton.getAttribute("badge-status"), "update-manual", "Badge is not displaying on PanelUI button.");

  let mainActionButton = doc.getAnonymousElementByAttribute(doorhanger, "anonid", "button");
  mainActionButton.click();
  ok(mainActionCalled, "Main action callback was called");
  is(PanelUI.notificationPanel.state, "closed", "update-manual doorhanger is closed.");
  is(PanelUI.menuButton.hasAttribute("badge-status"), false, "Should not have a badge status");
});
