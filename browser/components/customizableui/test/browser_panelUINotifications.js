"use strict";

const { AppMenuNotifications } = ChromeUtils.import(
  "resource://gre/modules/AppMenuNotifications.jsm"
);

/**
 * Tests that when we click on the main call-to-action of the doorhanger, the provided
 * action is called, and the doorhanger removed.
 */
add_task(async function testMainActionCalled() {
  let options = {
    gBrowser: window.gBrowser,
    url: "about:blank",
  };

  await BrowserTestUtils.withNewTab(options, function(browser) {
    is(
      PanelUI.notificationPanel.state,
      "closed",
      "update-manual doorhanger is closed."
    );
    let mainActionCalled = false;
    let mainAction = {
      callback: () => {
        mainActionCalled = true;
      },
    };
    AppMenuNotifications.showNotification("update-manual", mainAction);

    isnot(
      PanelUI.notificationPanel.state,
      "closed",
      "update-manual doorhanger is showing."
    );
    let notifications = [...PanelUI.notificationPanel.children].filter(
      n => !n.hidden
    );
    is(
      notifications.length,
      1,
      "PanelUI doorhanger is only displaying one notification."
    );
    let doorhanger = notifications[0];
    is(
      doorhanger.id,
      "appMenu-update-manual-notification",
      "PanelUI is displaying the update-manual notification."
    );

    let button = doorhanger.button;
    button.click();

    ok(mainActionCalled, "Main action callback was called");
    is(
      PanelUI.notificationPanel.state,
      "closed",
      "update-manual doorhanger is closed."
    );
    is(
      PanelUI.menuButton.hasAttribute("badge-status"),
      false,
      "Should not have a badge status"
    );
  });
});

/**
 * This tests that when we click the secondary action for a notification,
 * it will display the badge for that notification on the PanelUI menu button.
 * Once we click on this button, we should see an item in the menu which will
 * call our main action.
 */
add_task(async function testSecondaryActionWorkflow() {
  let options = {
    gBrowser: window.gBrowser,
    url: "about:blank",
  };

  await BrowserTestUtils.withNewTab(options, async function(browser) {
    is(
      PanelUI.notificationPanel.state,
      "closed",
      "update-manual doorhanger is closed."
    );

    let mainActionCalled = false;
    let mainAction = {
      callback: () => {
        mainActionCalled = true;
      },
    };
    AppMenuNotifications.showNotification("update-manual", mainAction);

    isnot(
      PanelUI.notificationPanel.state,
      "closed",
      "update-manual doorhanger is showing."
    );
    let notifications = [...PanelUI.notificationPanel.children].filter(
      n => !n.hidden
    );
    is(
      notifications.length,
      1,
      "PanelUI doorhanger is only displaying one notification."
    );
    let doorhanger = notifications[0];
    is(
      doorhanger.id,
      "appMenu-update-manual-notification",
      "PanelUI is displaying the update-manual notification."
    );

    let secondaryActionButton = doorhanger.secondaryButton;
    secondaryActionButton.click();

    is(
      PanelUI.notificationPanel.state,
      "closed",
      "update-manual doorhanger is closed."
    );

    is(
      PanelUI.menuButton.getAttribute("badge-status"),
      "update-manual",
      "Badge is displaying on PanelUI button."
    );

    await gCUITestUtils.openMainMenu();
    is(
      PanelUI.menuButton.getAttribute("badge-status"),
      "update-manual",
      "Badge is displaying on PanelUI button."
    );
    let menuItem = PanelUI.mainView.querySelector(".panel-banner-item");
    is(
      menuItem.label,
      menuItem.getAttribute("label-update-manual"),
      "Showing correct label"
    );
    is(menuItem.hidden, false, "update-manual menu item is showing.");

    await gCUITestUtils.hideMainMenu();
    is(
      PanelUI.menuButton.getAttribute("badge-status"),
      "update-manual",
      "Badge is shown on PanelUI button."
    );

    await gCUITestUtils.openMainMenu();
    menuItem.click();
    ok(mainActionCalled, "Main action callback was called");

    AppMenuNotifications.removeNotification(/.*/);
  });
});

/**
 * This tests that the PanelUI update downloading badge and banner
 * notification are correctly displayed and that clicking the banner
 * item calls the main action.
 */
add_task(async function testDownloadingBadge() {
  let options = {
    gBrowser: window.gBrowser,
    url: "about:blank",
  };

  await BrowserTestUtils.withNewTab(options, async function(browser) {
    let mainActionCalled = false;
    let mainAction = {
      callback: () => {
        mainActionCalled = true;
      },
    };
    // The downloading notification is always displayed in a dismissed state.
    AppMenuNotifications.showNotification(
      "update-downloading",
      mainAction,
      undefined,
      { dismissed: true }
    );
    is(PanelUI.notificationPanel.state, "closed", "doorhanger is closed.");

    is(
      PanelUI.menuButton.getAttribute("badge-status"),
      "update-downloading",
      "Downloading badge is displaying on PanelUI button."
    );

    await gCUITestUtils.openMainMenu();
    is(
      PanelUI.menuButton.getAttribute("badge-status"),
      "update-downloading",
      "Downloading badge is displaying on PanelUI button."
    );
    let menuItem = PanelUI.mainView.querySelector(".panel-banner-item");
    is(
      menuItem.label,
      menuItem.getAttribute("label-update-downloading"),
      "Showing correct label (downloading)"
    );
    is(menuItem.hidden, false, "update-downloading menu item is showing.");

    await gCUITestUtils.hideMainMenu();
    is(
      PanelUI.menuButton.getAttribute("badge-status"),
      "update-downloading",
      "Downloading badge is shown on PanelUI button."
    );

    await gCUITestUtils.openMainMenu();
    menuItem.click();
    ok(mainActionCalled, "Main action callback was called");

    AppMenuNotifications.removeNotification(/.*/);
  });
});

/**
 * We want to ensure a few things with this:
 * - Adding a doorhanger will make a badge disappear
 * - once the notification for the doorhanger is resolved (removed, not just dismissed),
 *   then we display any other badges that are remaining.
 */
add_task(async function testInteractionWithBadges() {
  await BrowserTestUtils.withNewTab("about:blank", async function(browser) {
    AppMenuNotifications.showBadgeOnlyNotification("fxa-needs-authentication");
    is(
      PanelUI.menuButton.getAttribute("badge-status"),
      "fxa-needs-authentication",
      "Fxa badge is shown on PanelUI button."
    );
    is(
      PanelUI.notificationPanel.state,
      "closed",
      "update-manual doorhanger is closed."
    );

    let mainActionCalled = false;
    let mainAction = {
      callback: () => {
        mainActionCalled = true;
      },
    };
    AppMenuNotifications.showNotification("update-manual", mainAction);

    isnot(
      PanelUI.menuButton.getAttribute("badge-status"),
      "fxa-needs-authentication",
      "Fxa badge is hidden on PanelUI button."
    );
    isnot(
      PanelUI.notificationPanel.state,
      "closed",
      "update-manual doorhanger is showing."
    );
    let notifications = [...PanelUI.notificationPanel.children].filter(
      n => !n.hidden
    );
    is(
      notifications.length,
      1,
      "PanelUI doorhanger is only displaying one notification."
    );
    let doorhanger = notifications[0];
    is(
      doorhanger.id,
      "appMenu-update-manual-notification",
      "PanelUI is displaying the update-manual notification."
    );

    let secondaryActionButton = doorhanger.secondaryButton;
    secondaryActionButton.click();

    is(
      PanelUI.notificationPanel.state,
      "closed",
      "update-manual doorhanger is closed."
    );

    is(
      PanelUI.menuButton.getAttribute("badge-status"),
      "update-manual",
      "Badge is displaying on PanelUI button."
    );

    await gCUITestUtils.openMainMenu();
    is(
      PanelUI.menuButton.getAttribute("badge-status"),
      "update-manual",
      "Badge is displaying on PanelUI button."
    );
    let menuItem = PanelUI.mainView.querySelector(".panel-banner-item");
    is(
      menuItem.label,
      menuItem.getAttribute("label-update-manual"),
      "Showing correct label"
    );
    is(menuItem.hidden, false, "update-manual menu item is showing.");

    menuItem.click();
    ok(mainActionCalled, "Main action callback was called");

    is(
      PanelUI.menuButton.getAttribute("badge-status"),
      "fxa-needs-authentication",
      "Fxa badge is shown on PanelUI button."
    );
    AppMenuNotifications.removeNotification(/.*/);
    is(
      PanelUI.menuButton.hasAttribute("badge-status"),
      false,
      "Should not have a badge status"
    );
  });
});

/**
 * This tests that adding a badge will not dismiss any existing doorhangers.
 */
add_task(async function testAddingBadgeWhileDoorhangerIsShowing() {
  await BrowserTestUtils.withNewTab("about:blank", function(browser) {
    is(
      PanelUI.notificationPanel.state,
      "closed",
      "update-manual doorhanger is closed."
    );
    let mainActionCalled = false;
    let mainAction = {
      callback: () => {
        mainActionCalled = true;
      },
    };
    AppMenuNotifications.showNotification("update-manual", mainAction);
    AppMenuNotifications.showBadgeOnlyNotification("fxa-needs-authentication");

    isnot(
      PanelUI.menuButton.getAttribute("badge-status"),
      "fxa-needs-authentication",
      "Fxa badge is hidden on PanelUI button."
    );
    isnot(
      PanelUI.notificationPanel.state,
      "closed",
      "update-manual doorhanger is showing."
    );
    let notifications = [...PanelUI.notificationPanel.children].filter(
      n => !n.hidden
    );
    is(
      notifications.length,
      1,
      "PanelUI doorhanger is only displaying one notification."
    );
    let doorhanger = notifications[0];
    is(
      doorhanger.id,
      "appMenu-update-manual-notification",
      "PanelUI is displaying the update-manual notification."
    );

    let mainActionButton = doorhanger.button;
    mainActionButton.click();

    ok(mainActionCalled, "Main action callback was called");
    is(
      PanelUI.notificationPanel.state,
      "closed",
      "update-manual doorhanger is closed."
    );
    is(
      PanelUI.menuButton.getAttribute("badge-status"),
      "fxa-needs-authentication",
      "Fxa badge is shown on PanelUI button."
    );
    AppMenuNotifications.removeNotification(/.*/);
    is(
      PanelUI.menuButton.hasAttribute("badge-status"),
      false,
      "Should not have a badge status"
    );
  });
});

/**
 * Tests that badges operate like a stack.
 */
add_task(async function testMultipleBadges() {
  await BrowserTestUtils.withNewTab("about:blank", async function(browser) {
    let doc = browser.ownerDocument;
    let menuButton = doc.getElementById("PanelUI-menu-button");

    is(
      menuButton.hasAttribute("badge-status"),
      false,
      "Should not have a badge status"
    );
    is(
      menuButton.hasAttribute("badge"),
      false,
      "Should not have the badge attribute set"
    );

    AppMenuNotifications.showBadgeOnlyNotification("fxa-needs-authentication");
    is(
      menuButton.getAttribute("badge-status"),
      "fxa-needs-authentication",
      "Should have fxa-needs-authentication badge status"
    );

    AppMenuNotifications.showBadgeOnlyNotification("update-succeeded");
    is(
      menuButton.getAttribute("badge-status"),
      "update-succeeded",
      "Should have update-succeeded badge status (update > fxa)"
    );

    AppMenuNotifications.showBadgeOnlyNotification("update-failed");
    is(
      menuButton.getAttribute("badge-status"),
      "update-failed",
      "Should have update-failed badge status"
    );

    AppMenuNotifications.removeNotification(/^update-/);
    is(
      menuButton.getAttribute("badge-status"),
      "fxa-needs-authentication",
      "Should have fxa-needs-authentication badge status"
    );

    AppMenuNotifications.removeNotification(/^fxa-/);
    is(
      menuButton.hasAttribute("badge-status"),
      false,
      "Should not have a badge status"
    );

    await gCUITestUtils.openMainMenu();
    is(
      menuButton.hasAttribute("badge-status"),
      false,
      "Should not have a badge status (Hamburger menu opened)"
    );
    await gCUITestUtils.hideMainMenu();

    AppMenuNotifications.showBadgeOnlyNotification("fxa-needs-authentication");
    AppMenuNotifications.showBadgeOnlyNotification("update-succeeded");
    AppMenuNotifications.removeNotification(/.*/);
    is(
      menuButton.hasAttribute("badge-status"),
      false,
      "Should not have a badge status"
    );
  });
});

/**
 * Tests that non-badges also operate like a stack.
 */
add_task(async function testMultipleNonBadges() {
  await BrowserTestUtils.withNewTab("about:blank", async function(browser) {
    is(
      PanelUI.notificationPanel.state,
      "closed",
      "update-manual doorhanger is closed."
    );

    let updateManualAction = {
      called: false,
      callback: () => {
        updateManualAction.called = true;
      },
    };
    let updateRestartAction = {
      called: false,
      callback: () => {
        updateRestartAction.called = true;
      },
    };

    AppMenuNotifications.showNotification("update-manual", updateManualAction);

    let notifications;
    let doorhanger;

    isnot(PanelUI.notificationPanel.state, "closed", "Doorhanger is showing.");
    notifications = [...PanelUI.notificationPanel.children].filter(
      n => !n.hidden
    );
    is(
      notifications.length,
      1,
      "PanelUI doorhanger is only displaying one notification."
    );
    doorhanger = notifications[0];
    is(
      doorhanger.id,
      "appMenu-update-manual-notification",
      "PanelUI is displaying the update-manual notification."
    );

    AppMenuNotifications.showNotification(
      "update-restart",
      updateRestartAction
    );

    isnot(PanelUI.notificationPanel.state, "closed", "Doorhanger is showing.");
    notifications = [...PanelUI.notificationPanel.children].filter(
      n => !n.hidden
    );
    is(
      notifications.length,
      1,
      "PanelUI doorhanger is only displaying one notification."
    );
    doorhanger = notifications[0];
    is(
      doorhanger.id,
      "appMenu-update-restart-notification",
      "PanelUI is displaying the update-restart notification."
    );

    let secondaryActionButton = doorhanger.secondaryButton;
    secondaryActionButton.click();

    is(
      PanelUI.notificationPanel.state,
      "closed",
      "update-manual doorhanger is closed."
    );
    is(
      PanelUI.menuButton.getAttribute("badge-status"),
      "update-restart",
      "update-restart badge is displaying on PanelUI button."
    );

    await gCUITestUtils.openMainMenu();
    is(
      PanelUI.menuButton.getAttribute("badge-status"),
      "update-restart",
      "update-restart badge is displaying on PanelUI button."
    );
    let menuItem = PanelUI.mainView.querySelector(".panel-banner-item");
    is(
      menuItem.label,
      menuItem.getAttribute("label-update-restart"),
      "Showing correct label"
    );
    is(menuItem.hidden, false, "update-restart menu item is showing.");

    menuItem.click();
    ok(
      updateRestartAction.called,
      "update-restart main action callback was called"
    );

    is(
      PanelUI.notificationPanel.state,
      "closed",
      "update-manual doorhanger is closed."
    );
    is(
      PanelUI.menuButton.getAttribute("badge-status"),
      "update-manual",
      "update-manual badge is displaying on PanelUI button."
    );

    await gCUITestUtils.openMainMenu();
    is(
      PanelUI.menuButton.getAttribute("badge-status"),
      "update-manual",
      "update-manual badge is displaying on PanelUI button."
    );
    is(
      menuItem.label,
      menuItem.getAttribute("label-update-manual"),
      "Showing correct label"
    );
    is(menuItem.hidden, false, "update-manual menu item is showing.");

    menuItem.click();
    ok(
      updateManualAction.called,
      "update-manual main action callback was called"
    );
  });
});
