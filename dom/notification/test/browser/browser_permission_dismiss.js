"use strict";

const { PermissionTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/PermissionTestUtils.sys.mjs"
);

const ORIGIN_URI = Services.io.newURI("https://example.com");
const PERMISSION_NAME = "desktop-notification";
const PROMPT_ALLOW_BUTTON = -1;
const PROMPT_NOT_NOW_BUTTON = 0;
const PROMPT_NEVER_BUTTON = 1;
const TEST_URL =
  "https://example.com/browser/dom/notification/test/browser/notification.html";

/**
 * Clicks the specified web-notifications prompt button.
 *
 * @param {Number} aButtonIndex Number indicating which button to click.
 *                              See the constants in this file.
 * @note modified from toolkit/components/passwordmgr/test/browser/head.js
 */
function clickDoorhangerButton(aButtonIndex, browser) {
  let popup = PopupNotifications.getNotification("web-notifications", browser);
  let notifications = popup.owner.panel.childNodes;
  ok(notifications.length, "at least one notification displayed");
  ok(true, notifications.length + " notification(s)");
  let notification = notifications[0];

  if (aButtonIndex == PROMPT_ALLOW_BUTTON) {
    ok(true, "Triggering main action (allow the permission)");
    notification.button.doCommand();
  } else if (aButtonIndex == PROMPT_NEVER_BUTTON) {
    ok(true, "Triggering secondary action (deny the permission permanently)");
    notification.menupopup.querySelector("menuitem").doCommand();
  } else {
    ok(true, "Triggering secondary action (deny the permission temporarily)");
    notification.secondaryButton.doCommand();
  }
}

/**
 * Opens a tab which calls `Notification.requestPermission()` with a callback
 * argument, calls the `task` function while the permission prompt is open,
 * and verifies that the expected permission is set.
 *
 * @param {Function} task Task function to run to interact with the prompt.
 * @param {String} permission Expected permission value.
 * @return {Promise} resolving when the task function is done and the tab
 *                   closes.
 */
function tabWithRequest(
  task,
  permission,
  browser = window.gBrowser,
  privateWindow = false
) {
  clearPermission(ORIGIN_URI, PERMISSION_NAME, privateWindow);

  return BrowserTestUtils.withNewTab(
    {
      gBrowser: browser,
      url: TEST_URL,
    },
    async function (linkedBrowser) {
      let requestPromise = SpecialPowers.spawn(
        linkedBrowser,
        [
          {
            permission,
          },
        ],
        async function ({ permission }) {
          function requestCallback(perm) {
            is(
              perm,
              permission,
              "Should call the legacy callback with the permission state"
            );
          }
          let perm = await content.window.Notification.requestPermission(
            requestCallback
          );
          is(
            perm,
            permission,
            "Should resolve the promise with the permission state"
          );
        }
      );

      await task(linkedBrowser);
      await requestPromise;
    }
  );
}

function clearPermission(origin, permissionName, isPrivate) {
  let principal = Services.scriptSecurityManager.createContentPrincipal(
    origin,
    isPrivate ? { privateBrowsingId: 1 } : {} /* attrs */
  );
  PermissionTestUtils.remove(principal, permissionName);
}

add_setup(async function () {
  Services.prefs.setBoolPref(
    "dom.webnotifications.requireuserinteraction",
    false
  );
  Services.prefs.setBoolPref(
    "permissions.desktop-notification.notNow.enabled",
    true
  );
  SimpleTest.registerCleanupFunction(() => {
    Services.prefs.clearUserPref("dom.webnotifications.requireuserinteraction");
    Services.prefs.clearUserPref(
      "permissions.desktop-notification.notNow.enabled"
    );

    clearPermission(ORIGIN_URI, PERMISSION_NAME, false /* private origin */);
    clearPermission(ORIGIN_URI, PERMISSION_NAME, true /* private origin */);
  });
});

add_task(async function test_requestPermission_granted() {
  await tabWithRequest(async function (linkedBrowser) {
    await BrowserTestUtils.waitForEvent(PopupNotifications.panel, "popupshown");
    clickDoorhangerButton(PROMPT_ALLOW_BUTTON, linkedBrowser);
  }, "granted");

  ok(
    !PopupNotifications.getNotification("web-notifications"),
    "Should remove the doorhanger notification icon if granted"
  );

  is(
    PermissionTestUtils.testPermission(ORIGIN_URI, PERMISSION_NAME),
    Services.perms.ALLOW_ACTION,
    "Check permission in perm. manager"
  );
});

add_task(async function test_requestPermission_denied_temporarily() {
  await tabWithRequest(async function (linkedBrowser) {
    await BrowserTestUtils.waitForEvent(PopupNotifications.panel, "popupshown");
    clickDoorhangerButton(PROMPT_NOT_NOW_BUTTON, linkedBrowser);
  }, "default");

  ok(
    !PopupNotifications.getNotification("web-notifications"),
    "Should remove the doorhanger notification icon if denied"
  );

  is(
    PermissionTestUtils.testPermission(ORIGIN_URI, PERMISSION_NAME),
    Services.perms.UNKNOWN_ACTION,
    "Check permission in perm. manager"
  );
});

add_task(async function test_requestPermission_denied_permanently() {
  await tabWithRequest(async function (linkedBrowser) {
    await BrowserTestUtils.waitForEvent(PopupNotifications.panel, "popupshown");
    clickDoorhangerButton(PROMPT_NEVER_BUTTON, linkedBrowser);
  }, "denied");

  ok(
    !PopupNotifications.getNotification("web-notifications"),
    "Should remove the doorhanger notification icon if denied"
  );

  is(
    PermissionTestUtils.testPermission(ORIGIN_URI, PERMISSION_NAME),
    Services.perms.DENY_ACTION,
    "Check permission in perm. manager"
  );
});

add_task(
  async function test_requestPermission_defaultPrivateNotificationsPref() {
    ok(
      !SpecialPowers.getBoolPref(
        "dom.webnotifications.privateBrowsing.enableDespiteLimitations"
      ),
      "Pref should be default disabled"
    );
  }
);

add_task(async function test_requestPermission_privateNotifications() {
  async function run(perm) {
    let privateWindow = await BrowserTestUtils.openNewBrowserWindow({
      private: true,
    });

    await tabWithRequest(
      async linkedBrowser => {
        if (perm != Services.perms.UNKNOWN_ACTION) {
          await BrowserTestUtils.waitForEvent(
            privateWindow.PopupNotifications.panel,
            "popupshown"
          );

          clickDoorhangerButton(PROMPT_ALLOW_BUTTON, linkedBrowser);
        }
      },
      perm == Services.perms.ALLOW_ACTION ? "granted" : "denied",
      privateWindow.gBrowser,
      true /* privateWindow */
    );

    ok(
      !PopupNotifications.getNotification(
        "web-notifications",
        privateWindow.gBrowser
      ),
      "doorhanger should have been removed in all cases by now"
    );

    await BrowserTestUtils.closeWindow(privateWindow);
  }

  await run(Services.perms.UNKNOWN_ACTION);

  await SpecialPowers.pushPrefEnv({
    set: [
      ["dom.webnotifications.privateBrowsing.enableDespiteLimitations", true],
    ],
  });
  await run(Services.perms.ALLOW_ACTION);
});
