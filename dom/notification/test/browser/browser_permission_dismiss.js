"use strict";

const ORIGIN_URI = Services.io.newURI("http://mochi.test:8888", null, null);
const PERMISSION_NAME = "desktop-notification";
const PROMPT_ALLOW_BUTTON = -1;
const PROMPT_BLOCK_BUTTON = 0;
const TEST_URL = "http://mochi.test:8888/browser/dom/notification/test/browser/notification.html";

/**
 * Clicks the specified web-notifications prompt button.
 *
 * @param {Number} aButtonIndex Number indicating which button to click.
 *                              See the constants in this file.
 * @note modified from toolkit/components/passwordmgr/test/browser/head.js
 */
function clickDoorhangerButton(aButtonIndex) {
  ok(true, "Looking for action at index " + aButtonIndex);

  let popup = PopupNotifications.getNotification("web-notifications");
  let notifications = popup.owner.panel.childNodes;
  ok(notifications.length > 0, "at least one notification displayed");
  ok(true, notifications.length + " notification(s)");
  let notification = notifications[0];

  if (aButtonIndex == -1) {
    ok(true, "Triggering main action");
    notification.button.doCommand();
  } else if (aButtonIndex <= popup.secondaryActions.length) {
    ok(true, "Triggering secondary action " + aButtonIndex);
    notification.childNodes[aButtonIndex].doCommand();
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
  }, function*(browser) {
    let requestPromise = ContentTask.spawn(browser, {
      permission
    }, function*({permission}) {
      function requestCallback(perm) {
        is(perm, permission,
          "Should call the legacy callback with the permission state");
      }
      let perm = yield content.window.Notification
                              .requestPermission(requestCallback);
      is(perm, permission,
         "Should resolve the promise with the permission state");
    });

    yield BrowserTestUtils.waitForEvent(PopupNotifications.panel, "popupshown");
    yield task();
    yield requestPromise;
  });
}

add_task(function* setup() {
  SimpleTest.registerCleanupFunction(() => {
    Services.perms.remove(ORIGIN_URI, PERMISSION_NAME);
  });
});

add_task(function* test_requestPermission_granted() {
  yield tabWithRequest(function() {
    clickDoorhangerButton(PROMPT_ALLOW_BUTTON);
  }, "granted");

  ok(!PopupNotifications.getNotification("web-notifications"),
     "Should remove the doorhanger notification icon if granted");

  is(Services.perms.testPermission(ORIGIN_URI, PERMISSION_NAME),
     Services.perms.ALLOW_ACTION,
     "Check permission in perm. manager");
});

add_task(function* test_requestPermission_denied() {
  yield tabWithRequest(function() {
    clickDoorhangerButton(PROMPT_BLOCK_BUTTON);
  }, "denied");

  ok(!PopupNotifications.getNotification("web-notifications"),
     "Should remove the doorhanger notification icon if denied");

  is(Services.perms.testPermission(ORIGIN_URI, PERMISSION_NAME),
     Services.perms.DENY_ACTION,
     "Check permission in perm. manager");
});

add_task(function* test_requestPermission_dismissed() {
  yield tabWithRequest(function() {
    PopupNotifications.panel.hidePopup();
  }, "default");

  ok(!PopupNotifications.getNotification("web-notifications"),
     "Should remove the doorhanger notification icon if dismissed");

  is(Services.perms.testPermission(ORIGIN_URI, PERMISSION_NAME),
     Services.perms.UNKNOWN_ACTION,
     "Check permission in perm. manager");
});
