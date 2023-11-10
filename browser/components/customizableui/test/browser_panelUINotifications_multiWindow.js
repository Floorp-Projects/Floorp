"use strict";

const { AppMenuNotifications } = ChromeUtils.importESModule(
  "resource://gre/modules/AppMenuNotifications.sys.mjs"
);

/**
 * Tests that when we try to show a notification in a background window, it
 * does not display until the window comes back into the foreground. However,
 * it should display a badge.
 */
add_task(async function testDoesNotShowDoorhangerForBackgroundWindow() {
  let options = {
    gBrowser: window.gBrowser,
    url: "about:blank",
  };

  await BrowserTestUtils.withNewTab(options, async function (browser) {
    let win = await BrowserTestUtils.openNewBrowserWindow();
    await SimpleTest.promiseFocus(win);
    let mainActionCalled = false;
    let mainAction = {
      callback: () => {
        mainActionCalled = true;
      },
    };
    AppMenuNotifications.showNotification("update-manual", mainAction);
    is(
      PanelUI.notificationPanel.state,
      "closed",
      "The background window's doorhanger is closed."
    );
    is(
      PanelUI.menuButton.hasAttribute("badge-status"),
      true,
      "The background window has a badge."
    );

    let popupShown = BrowserTestUtils.waitForEvent(
      PanelUI.notificationPanel,
      "popupshown"
    );

    await BrowserTestUtils.closeWindow(win);
    await SimpleTest.promiseFocus(window);
    await popupShown;

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

    Assert.equal(
      PanelUI.notificationPanel.state,
      "open",
      "Expect panel state to be open when clicking panel buttons"
    );
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
 * Tests that when we try to show a notification in a background window and in
 * a foreground window, if the foreground window's main action is called, the
 * background window's doorhanger will be removed.
 */
add_task(
  async function testBackgroundWindowNotificationsAreRemovedByForeground() {
    let options = {
      gBrowser: window.gBrowser,
      url: "about:blank",
    };

    await BrowserTestUtils.withNewTab(options, async function (browser) {
      let win = await BrowserTestUtils.openNewBrowserWindow();
      await SimpleTest.promiseFocus(win);
      AppMenuNotifications.showNotification("update-manual", { callback() {} });
      let notifications = [...win.PanelUI.notificationPanel.children].filter(
        n => !n.hidden
      );
      is(
        notifications.length,
        1,
        "PanelUI doorhanger is only displaying one notification."
      );
      let doorhanger = notifications[0];
      doorhanger.button.click();

      await BrowserTestUtils.closeWindow(win);
      await SimpleTest.promiseFocus(window);

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
  }
);

/**
 * Tests that when we try to show a notification in a background window and in
 * a foreground window, if the foreground window's doorhanger is dismissed,
 * the background window's doorhanger will also be dismissed once the window
 * regains focus.
 */
add_task(
  async function testBackgroundWindowNotificationsAreDismissedByForeground() {
    let options = {
      gBrowser: window.gBrowser,
      url: "about:blank",
    };

    await BrowserTestUtils.withNewTab(options, async function (browser) {
      let win = await BrowserTestUtils.openNewBrowserWindow();
      await SimpleTest.promiseFocus(win);
      AppMenuNotifications.showNotification("update-manual", { callback() {} });
      let notifications = [...win.PanelUI.notificationPanel.children].filter(
        n => !n.hidden
      );
      is(
        notifications.length,
        1,
        "PanelUI doorhanger is only displaying one notification."
      );
      let doorhanger = notifications[0];
      let button = doorhanger.secondaryButton;
      button.click();

      await BrowserTestUtils.closeWindow(win);
      await SimpleTest.promiseFocus(window);

      is(
        PanelUI.notificationPanel.state,
        "closed",
        "The background window's doorhanger is closed."
      );
      is(
        PanelUI.menuButton.hasAttribute("badge-status"),
        true,
        "The dismissed notification should still have a badge status"
      );

      AppMenuNotifications.removeNotification(/.*/);
    });
  }
);

/**
 * Tests that when we open a new window while a notification is showing, the
 * notification also shows on the new window.
 */
add_task(async function testOpenWindowAfterShowingNotification() {
  AppMenuNotifications.showNotification("update-manual", { callback() {} });

  let win = await BrowserTestUtils.openNewBrowserWindow();
  await SimpleTest.promiseFocus(win);
  let notifications = [...win.PanelUI.notificationPanel.children].filter(
    n => !n.hidden
  );
  is(
    notifications.length,
    1,
    "PanelUI doorhanger is only displaying one notification."
  );
  let doorhanger = notifications[0];
  let button = doorhanger.secondaryButton;
  button.click();

  await BrowserTestUtils.closeWindow(win);
  await SimpleTest.promiseFocus(window);

  is(
    PanelUI.notificationPanel.state,
    "closed",
    "The background window's doorhanger is closed."
  );
  is(
    PanelUI.menuButton.hasAttribute("badge-status"),
    true,
    "The dismissed notification should still have a badge status"
  );

  AppMenuNotifications.removeNotification(/.*/);
});
