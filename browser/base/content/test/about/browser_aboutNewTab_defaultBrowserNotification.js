/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let { DefaultBrowserNotification } = ChromeUtils.import(
  "resource:///actors/AboutNewTabParent.jsm",
  {}
);

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

    BrowserTestUtils.loadURI(gBrowser.selectedBrowser, "https://example.com");

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

add_task(async function notification_appears_on_first_navigation_to_homepage() {
  await test_with_mock_shellservice({ isDefault: false }, async function() {
    let tab = await BrowserTestUtils.openNewForegroundTab({
      gBrowser,
      opening: "about:robots",
    });
    ok(
      tab.linkedBrowser &&
        gBrowser.getNotificationBox(tab.linkedBrowser) &&
        !gBrowser.getNotificationBox(tab.linkedBrowser).currentNotification,
      "a notification should not be shown on about:robots"
    );
    BrowserTestUtils.loadURI(gBrowser.selectedBrowser, "about:home");

    let notification = await TestUtils.waitForCondition(
      () =>
        tab.linkedBrowser &&
        gBrowser.getNotificationBox(tab.linkedBrowser) &&
        gBrowser.getNotificationBox(tab.linkedBrowser).currentNotification,
      "waiting for notification to appear on about:home after coming from about:robots"
    );
    ok(
      notification,
      "A notification should be shown after navigation to about:home"
    );
    is(
      notification.getAttribute("value"),
      "default-browser",
      "Notification should be default browser"
    );

    gBrowser.removeTab(tab);
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
    let button = notification.buttonContainer.querySelector(
      ".notification-button"
    );
    button.click();
    ok(
      shellService.isDefaultBrowser(),
      "should be default after clicking button"
    );

    gBrowser.removeTab(firstTab);
  });
});

add_task(async function notification_not_displayed_on_private_window() {
  let privateWin = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
  });
  await test_with_mock_shellservice(
    { win: privateWin, isDefault: false },
    async function() {
      await BrowserTestUtils.openNewForegroundTab({
        gBrowser: privateWin.gBrowser,
        opening: "about:newtab",
        waitForLoad: false,
      });
      ok(
        !privateWin.gBrowser.getNotificationBox(
          privateWin.gBrowser.selectedBrowser
        ).currentNotification,
        "There shouldn't be a notification in the private window"
      );
      await BrowserTestUtils.closeWindow(privateWin);
    }
  );
});

add_task(async function notification_displayed_on_perm_private_window() {
  SpecialPowers.pushPrefEnv({
    set: [["browser.privatebrowsing.autostart", true]],
  });
  let privateWin = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
  });
  await test_with_mock_shellservice(
    { win: privateWin, isDefault: false },
    async function() {
      let tab = await BrowserTestUtils.openNewForegroundTab({
        gBrowser: privateWin.gBrowser,
        opening: "about:newtab",
        waitForLoad: false,
      });
      ok(
        PrivateBrowsingUtils.isBrowserPrivate(
          privateWin.gBrowser.selectedBrowser
        ),
        "Browser should be private"
      );
      let notification = await TestUtils.waitForCondition(
        () =>
          tab.linkedBrowser &&
          gBrowser.getNotificationBox(tab.linkedBrowser) &&
          gBrowser.getNotificationBox(tab.linkedBrowser).currentNotification,
        "waiting for notification"
      );
      ok(notification, "A notification should be shown on the new tab page");
      is(
        notification.getAttribute("value"),
        "default-browser",
        "Notification should be default browser"
      );
      await BrowserTestUtils.closeWindow(privateWin);
    }
  );
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

    notification.closeButton.click();
    ok(
      !Services.prefs.getBoolPref("browser.shell.checkDefaultBrowser"),
      "checkDefaultBrowser bar pref should be false after dismissing notification"
    );

    gBrowser.removeTab(firstTab);
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

add_task(async function modal_notification_shown_when_bar_disabled() {
  await test_with_mock_shellservice({ useModal: true }, async function() {
    let modalOpenPromise = BrowserTestUtils.promiseAlertDialogOpen("cancel");

    // This method is called during startup. Call it now so we don't have to test startup.
    let { BrowserGlue } = ChromeUtils.import(
      "resource:///modules/BrowserGlue.jsm",
      {}
    );
    BrowserGlue.prototype._maybeShowDefaultBrowserPrompt();

    await modalOpenPromise;
  });
});

async function test_with_mock_shellservice(options, testFn) {
  let win = options.win || window;
  let oldShellService = win.getShellService;
  let mockShellService = {
    _isDefault: !!options.isDefault,
    canSetDesktopBackground() {},
    isDefaultBrowserOptOut() {
      return false;
    },
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
  win.getShellService = function() {
    return mockShellService;
  };
  let prefs = {
    set: [
      ["browser.shell.checkDefaultBrowser", true],
      ["browser.defaultbrowser.notificationbar", !options.useModal],
      ["browser.defaultbrowser.notificationbar.checkcount", 0],
    ],
  };
  if (options.useModal) {
    prefs.set.push(["browser.shell.skipDefaultBrowserCheckOnFirstRun", false]);
  }
  await SpecialPowers.pushPrefEnv(prefs);

  // Reset the state so the notification can be shown multiple times in one session
  DefaultBrowserNotification.reset();

  await testFn();

  win.getShellService = oldShellService;
}
