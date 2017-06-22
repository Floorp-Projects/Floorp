"use strict";

const ORIGIN_URI = Services.io.newURI("http://mochi.test:8888");
const PERMISSION_NAME = "desktop-notification";
const PROMPT_ALLOW_BUTTON = -1;
const PROMPT_NOT_NOW_BUTTON = 0;
const PROMPT_NEVER_BUTTON = 1;
const TEST_URL = "http://mochi.test:8888/browser/dom/notification/test/browser/notification.html";

/**
 * Clicks the specified web-notifications prompt button.
 *
 * @param {Number} aButtonIndex Number indicating which button to click.
 *                              See the constants in this file.
 * @note modified from toolkit/components/passwordmgr/test/browser/head.js
 */
function clickDoorhangerButton(aButtonIndex) {
  let popup = PopupNotifications.getNotification("web-notifications");
  let notifications = popup.owner.panel.childNodes;
  ok(notifications.length > 0, "at least one notification displayed");
  ok(true, notifications.length + " notification(s)");
  let notification = notifications[0];

  if (aButtonIndex == PROMPT_ALLOW_BUTTON) {
    ok(true, "Triggering main action (allow the permission)");
    notification.button.doCommand();
  } else if (aButtonIndex == PROMPT_NEVER_BUTTON) {
    ok(true, "Triggering secondary action (deny the permission permanently)");
    // The menuitems in the dropdown are accessible as direct children of the panel,
    // because they are injected into a <children> node in the XBL binding.
    // The "never" button is the first menuitem in the dropdown.
    notification.querySelector("menuitem").doCommand();
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
function tabWithRequest(task, permission) {
  Services.perms.remove(ORIGIN_URI, PERMISSION_NAME);

  return BrowserTestUtils.withNewTab({
    gBrowser,
    url: TEST_URL,
  }, async function(browser) {
    let requestPromise = ContentTask.spawn(browser, {
      permission
    }, async function({permission}) {
      function requestCallback(perm) {
        is(perm, permission,
          "Should call the legacy callback with the permission state");
      }
      let perm = await content.window.Notification
                              .requestPermission(requestCallback);
      is(perm, permission,
         "Should resolve the promise with the permission state");
    });

    await BrowserTestUtils.waitForEvent(PopupNotifications.panel, "popupshown");
    await task();
    await requestPromise;
  });
}

add_task(async function setup() {
  SimpleTest.registerCleanupFunction(() => {
    Services.perms.remove(ORIGIN_URI, PERMISSION_NAME);
  });
});

add_task(async function test_requestPermission_granted() {
  await tabWithRequest(function() {
    clickDoorhangerButton(PROMPT_ALLOW_BUTTON);
  }, "granted");

  ok(!PopupNotifications.getNotification("web-notifications"),
     "Should remove the doorhanger notification icon if granted");

  is(Services.perms.testPermission(ORIGIN_URI, PERMISSION_NAME),
     Services.perms.ALLOW_ACTION,
     "Check permission in perm. manager");
});

add_task(async function test_requestPermission_denied_temporarily() {
  await tabWithRequest(function() {
    clickDoorhangerButton(PROMPT_NOT_NOW_BUTTON);
  }, "default");

  ok(!PopupNotifications.getNotification("web-notifications"),
     "Should remove the doorhanger notification icon if denied");

  is(Services.perms.testPermission(ORIGIN_URI, PERMISSION_NAME),
     Services.perms.UNKNOWN_ACTION,
     "Check permission in perm. manager");
});

add_task(async function test_requestPermission_denied_permanently() {
  await tabWithRequest(async function() {
    await clickDoorhangerButton(PROMPT_NEVER_BUTTON);
  }, "denied");

  ok(!PopupNotifications.getNotification("web-notifications"),
     "Should remove the doorhanger notification icon if denied");

  is(Services.perms.testPermission(ORIGIN_URI, PERMISSION_NAME),
     Services.perms.DENY_ACTION,
     "Check permission in perm. manager");
});
