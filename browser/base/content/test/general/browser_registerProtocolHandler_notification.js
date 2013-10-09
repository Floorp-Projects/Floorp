/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function test() {
  waitForExplicitFinish();
  let notificationValue = "Protocol Registration: testprotocol";
  let testURI = "http://example.com/browser/" +
    "browser/base/content/test/general/browser_registerProtocolHandler_notification.html";

    waitForCondition(function() {
        // Do not start until the notification is up
        let notificationBox = window.gBrowser.getNotificationBox();
        let notification = notificationBox.getNotificationWithValue(notificationValue);
        return notification;
    },
    function() {

        let notificationBox = window.gBrowser.getNotificationBox();
        let notification = notificationBox.getNotificationWithValue(notificationValue);
        ok(notification, "Notification box should be displayed");
        is(notification.type, "info", "We expect this notification to have the type of 'info'.");
        isnot(notification.image, null, "We expect this notification to have an icon.");

        let buttons = notification.getElementsByClassName("notification-button-default");
        is(buttons.length, 1, "We expect see one default button.");

        buttons = notification.getElementsByClassName("notification-button");
        is(buttons.length, 1, "We expect see one button.");

        let button = buttons[0];
        isnot(button.label, null, "We expect the add button to have a label.");
        todo_isnot(button.accesskey, null, "We expect the add button to have a accesskey.");

        finish();
    }, 100);

    window.gBrowser.selectedBrowser.loadURI(testURI);
}
