"use strict";

let tab;
let notification;
let notificationURL = "http://example.org/browser/browser/base/content/test/alerts/file_dom_notifications.html";

add_task(function* test_notificationClose() {
  let pm = Services.perms;
  pm.add(makeURI(notificationURL), "desktop-notification", pm.ALLOW_ACTION);

  yield BrowserTestUtils.withNewTab({
    gBrowser,
    url: notificationURL
  }, function* dummyTabTask(aBrowser) {
    let win = aBrowser.contentWindow.wrappedJSObject;
    notification = win.showNotification2();
    yield BrowserTestUtils.waitForEvent(notification, "show");

    info("Notification alert showing");

    let alertWindow = Services.wm.getMostRecentWindow("alert:alert");
    if (!alertWindow) {
      ok(true, "Notifications don't use XUL windows on all platforms.");
      notification.close();
      return;
    }

    let alertCloseButton = alertWindow.document.querySelector(".alertCloseButton");
    is(alertCloseButton.localName, "toolbarbutton", "close button found");
    let promiseBeforeUnloadEvent =
      BrowserTestUtils.waitForEvent(alertWindow, "beforeunload");
    let closedTime = alertWindow.Date.now();
    alertCloseButton.click();
    info("Clicked on close button");
    let beforeUnloadEvent = yield promiseBeforeUnloadEvent;

    ok(true, "Alert should close when the close button is clicked");
    let currentTime = alertWindow.Date.now();
    // The notification will self-close at 12 seconds, so this checks
    // that the notification closed before the timeout.
    ok(currentTime - closedTime < 5000,
       "Close requested at " + closedTime + ", actually closed at " + currentTime);
  });
});

add_task(function* cleanup() {
  Services.perms.remove(makeURI(notificationURL), "desktop-notification");
});
