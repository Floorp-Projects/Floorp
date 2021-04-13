/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const TEST_PATH = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "https://example.com"
);
add_task(async function() {
  let notificationValue = "Protocol Registration: web+testprotocol";
  let testURI = TEST_PATH + "browser_registerProtocolHandler_notification.html";

  BrowserTestUtils.loadURI(window.gBrowser.selectedBrowser, testURI);
  await TestUtils.waitForCondition(
    function() {
      // Do not start until the notification is up
      let notificationBox = window.gBrowser.getNotificationBox();
      let notification = notificationBox.getNotificationWithValue(
        notificationValue
      );
      return notification;
    },
    "Still can not get notification after retrying 100 times.",
    100,
    100
  );

  let notificationBox = window.gBrowser.getNotificationBox();
  let notification = notificationBox.getNotificationWithValue(
    notificationValue
  );
  ok(notification, "Notification box should be displayed");
  if (notification == null) {
    finish();
    return;
  }
  is(
    notification.getAttribute("type"),
    "info",
    "We expect this notification to have the type of 'info'."
  );

  if (gProton) {
    // Make sure the CSS is fully loaded...
    await TestUtils.waitForCondition(
      () =>
        notification.shadowRoot.styleSheets.length &&
        [...notification.shadowRoot.styleSheets].every(s => s.rules.length)
    );
    is(
      notification.ownerGlobal.getComputedStyle(
        notification.messageImage,
        "::after"
      ).content,
      'url("chrome://global/skin/icons/info-filled.svg")',
      "We expect this notification to have an icon."
    );
  } else {
    ok(
      notification.messageImage.getAttribute("src"),
      "We expect this notification to have an icon."
    );
  }

  let buttons = notification.buttonContainer.getElementsByClassName(
    "notification-button"
  );
  is(buttons.length, 1, "We expect see one button.");

  let button = buttons[0];
  isnot(button.label, null, "We expect the add button to have a label.");
  todo(button.accesskey, "We expect the add button to have a accesskey.");
});
