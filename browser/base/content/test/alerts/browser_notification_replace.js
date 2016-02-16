"use strict";

let notificationURL = "http://example.org/browser/browser/base/content/test/alerts/file_dom_notifications.html";

add_task(function* test_notificationReplace() {
  let pm = Services.perms;
  pm.add(makeURI(notificationURL), "desktop-notification", pm.ALLOW_ACTION);

  yield BrowserTestUtils.withNewTab({
    gBrowser,
    url: notificationURL
  }, function* dummyTabTask(aBrowser) {
    yield ContentTask.spawn(aBrowser, {}, function* () {
      let win = content.window.wrappedJSObject;
      let notification = win.showNotification1();
      let promiseCloseEvent = ContentTaskUtils.waitForEvent(notification, "close");

      let showEvent = yield ContentTaskUtils.waitForEvent(notification, "show");
      is(showEvent.target.body, "Test body 1", "Showed tagged notification");

      let newNotification = win.showNotification2();
      let newShowEvent = yield ContentTaskUtils.waitForEvent(newNotification, "show");
      is(newShowEvent.target.body, "Test body 2", "Showed new notification with same tag");

      let closeEvent = yield promiseCloseEvent;
      is(closeEvent.target.body, "Test body 1", "Closed previous tagged notification");

      let promiseNewCloseEvent = ContentTaskUtils.waitForEvent(newNotification, "close");
      newNotification.close();
      let newCloseEvent = yield promiseNewCloseEvent;
      is(newCloseEvent.target.body, "Test body 2", "Closed new notification");
    });
  });
});

add_task(function* cleanup() {
  Services.perms.remove(makeURI(notificationURL), "desktop-notification");
});
