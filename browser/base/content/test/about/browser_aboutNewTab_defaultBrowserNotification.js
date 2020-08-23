/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function notification_shown_on_first_newtab_when_not_default() {
  await test_with_mock_shellservice({ isDefault: false }, async function() {
    ok(
      !gBrowser.getNotificationBox(gBrowser.selectedBrowser)
        .currentNotification,
      "There shouldn't be a notification when the test starts"
    );
    let firstTab = await BrowserTestUtils.openNewForegroundTab({
      gBrowser,
      opening: "about:newtab",
      waitForLoad: false,
    });
    let notification = await TestUtils.waitForCondition(
      () =>
        firstTab.linkedBrowser &&
        gBrowser.getNotificationBox(firstTab.linkedBrowser) &&
        gBrowser.getNotificationBox(firstTab.linkedBrowser).currentNotification,
      "waiting for notification"
    );
    ok(notification, "A notification should be shown on the new tab page");
    is(
      notification.getAttribute("value"),
      "default-browser",
      "Notification should be default browser"
    );

    let secondTab = await BrowserTestUtils.openNewForegroundTab({
      gBrowser,
      opening: "about:newtab",
      waitForLoad: false,
    });

    ok(
      secondTab.linkedBrowser &&
        gBrowser.getNotificationBox(secondTab.linkedBrowser) &&
        !gBrowser.getNotificationBox(secondTab.linkedBrowser)
          .currentNotification,
      "A notification should not be shown on the second new tab page"
    );

    gBrowser.removeTab(firstTab);
    gBrowser.removeTab(secondTab);
  });
});

add_task(async function notification_not_shown_on_first_newtab_when_default() {
  await test_with_mock_shellservice({ isDefault: true }, async function() {
    ok(
      !gBrowser.getNotificationBox(gBrowser.selectedBrowser)
        .currentNotification,
      "There shouldn't be a notification when the test starts"
    );
    let firstTab = await BrowserTestUtils.openNewForegroundTab({
      gBrowser,
      opening: "about:newtab",
      waitForLoad: false,
    });
    ok(
      firstTab.linkedBrowser &&
        gBrowser.getNotificationBox(firstTab.linkedBrowser) &&
        !gBrowser.getNotificationBox(firstTab.linkedBrowser)
          .currentNotification,
      "No notification on first tab when browser is default"
    );

    gBrowser.removeTab(firstTab);
  });
});

add_task(async function clicking_button_on_notification_calls_setAsDefault() {
  await test_with_mock_shellservice({ isDefault: false }, async function() {
    let firstTab = await BrowserTestUtils.openNewForegroundTab({
      gBrowser,
      opening: "about:newtab",
      waitForLoad: false,
    });
    let notification = await TestUtils.waitForCondition(
      () =>
        firstTab.linkedBrowser &&
        gBrowser.getNotificationBox(firstTab.linkedBrowser) &&
        gBrowser.getNotificationBox(firstTab.linkedBrowser).currentNotification,
      "waiting for notification"
    );
    ok(notification, "A notification should be shown on the new tab page");
    is(
      notification.getAttribute("value"),
      "default-browser",
      "Notification should be default browser"
    );

    let shellService = window.getShellService();
    ok(
      !shellService.isDefaultBrowser(),
      "should not be default prior to clicking button"
    );
    let button = notification.querySelector(".notification-button");
    button.click();
    ok(
      shellService.isDefaultBrowser(),
      "should be default after clicking button"
    );

    gBrowser.removeTab(firstTab);
  });
});

add_task(async function clicking_dismiss_disables_default_browser_checking() {
  await test_with_mock_shellservice({ isDefault: false }, async function() {
    let firstTab = await BrowserTestUtils.openNewForegroundTab({
      gBrowser,
      opening: "about:newtab",
      waitForLoad: false,
    });
    let notification = await TestUtils.waitForCondition(
      () =>
        firstTab.linkedBrowser &&
        gBrowser.getNotificationBox(firstTab.linkedBrowser) &&
        gBrowser.getNotificationBox(firstTab.linkedBrowser).currentNotification,
      "waiting for notification"
    );
    ok(notification, "A notification should be shown on the new tab page");
    is(
      notification.getAttribute("value"),
      "default-browser",
      "Notification should be default browser"
    );

    let closeButton = notification.querySelector(".close-icon");
    closeButton.click();
    ok(
      !Services.prefs.getBoolPref("browser.shell.checkDefaultBrowser"),
      "checkDefaultBrowser bar pref should be false after dismissing notification"
    );

    gBrowser.removeTab(firstTab);
  });
});

add_task(async function notification_bar_removes_itself_on_navigation() {
  await test_with_mock_shellservice({ isDefault: false }, async function() {
    let firstTab = await BrowserTestUtils.openNewForegroundTab({
      gBrowser,
      opening: "about:newtab",
      waitForLoad: false,
    });
    let notification = await TestUtils.waitForCondition(
      () =>
        firstTab.linkedBrowser &&
        gBrowser.getNotificationBox(firstTab.linkedBrowser) &&
        gBrowser.getNotificationBox(firstTab.linkedBrowser).currentNotification,
      "waiting for notification"
    );
    ok(notification, "A notification should be shown on the new tab page");
    is(
      notification.getAttribute("value"),
      "default-browser",
      "Notification should be default browser"
    );

    await BrowserTestUtils.loadURI(
      gBrowser.selectedBrowser,
      "https://example.com"
    );

    let notificationRemoved = await TestUtils.waitForCondition(
      () =>
        firstTab.linkedBrowser &&
        gBrowser.getNotificationBox(firstTab.linkedBrowser) &&
        !gBrowser.getNotificationBox(firstTab.linkedBrowser)
          .currentNotification,
      "waiting for notification to get removed"
    );
    ok(
      notificationRemoved,
      "A notification should not be shown after navigation"
    );

    gBrowser.removeTab(firstTab);
  });
});

async function test_with_mock_shellservice(options, testFn) {
  let oldShellService = window.getShellService;
  let mockShellService = {
    _isDefault: options.isDefault,
    canSetDesktopBackground() {},
    isDefaultBrowserOptOut() {},
    get shouldCheckDefaultBrowser() {
      return true;
    },
    isDefaultBrowser() {
      return this._isDefault;
    },
    setAsDefault() {
      this.setDefaultBrowser();
    },
    setDefaultBrowser() {
      this._isDefault = true;
    },
  };
  window.getShellService = function() {
    return mockShellService;
  };
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.shell.checkDefaultBrowser", true],
      ["browser.defaultbrowser.notificationbar", true],
      ["browser.defaultbrowser.notificationbar.checkcount", 0],
    ],
  });
  // Re-add the event listener since it gets removed on the first about:newtab
  await window.DefaultBrowserNotificationOnNewTabPage.init();

  await testFn();

  window.getShellService = oldShellService;
}
