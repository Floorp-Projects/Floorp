"use strict";

let notificationURL =
  "http://example.org/browser/browser/base/content/test/alerts/file_dom_notifications.html";

add_task(async function test_notificationReplace() {
  await addNotificationPermission(notificationURL);

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: notificationURL,
    },
    async function dummyTabTask(aBrowser) {
      await ContentTask.spawn(aBrowser, {}, async function() {
        let win = content.window.wrappedJSObject;
        let notification = win.showNotification1();
        let promiseCloseEvent = ContentTaskUtils.waitForEvent(
          notification,
          "close"
        );

        let showEvent = await ContentTaskUtils.waitForEvent(
          notification,
          "show"
        );
        Assert.equal(
          showEvent.target.body,
          "Test body 1",
          "Showed tagged notification"
        );

        let newNotification = win.showNotification2();
        let newShowEvent = await ContentTaskUtils.waitForEvent(
          newNotification,
          "show"
        );
        Assert.equal(
          newShowEvent.target.body,
          "Test body 2",
          "Showed new notification with same tag"
        );

        let closeEvent = await promiseCloseEvent;
        Assert.equal(
          closeEvent.target.body,
          "Test body 1",
          "Closed previous tagged notification"
        );

        let promiseNewCloseEvent = ContentTaskUtils.waitForEvent(
          newNotification,
          "close"
        );
        newNotification.close();
        let newCloseEvent = await promiseNewCloseEvent;
        Assert.equal(
          newCloseEvent.target.body,
          "Test body 2",
          "Closed new notification"
        );
      });
    }
  );
});
