/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

"use strict";

var tab;
var notification;
var notificationURL = "http://example.org/browser/browser/base/content/test/alerts/file_dom_notifications.html";
var newWindowOpenedFromTab;

add_task(function* test_notificationPreventDefaultAndSwitchTabs() {
  let pm = Services.perms;
  pm.add(makeURI(notificationURL), "desktop-notification", pm.ALLOW_ACTION);

  let originalTab = gBrowser.selectedTab;
  yield BrowserTestUtils.withNewTab({
    gBrowser,
    url: notificationURL
  }, function* dummyTabTask(aBrowser) {
    // Put new tab in background so it is obvious when it is re-focused.
    yield BrowserTestUtils.switchTab(gBrowser, originalTab);
    isnot(gBrowser.selectedBrowser, aBrowser, "Notification page loaded as a background tab");

    // First, show a notification that will be have the tab-switching prevented.
    function promiseNotificationEvent(evt) {
      return ContentTask.spawn(aBrowser, evt, function* (evt) {
        return yield new Promise(resolve => {
          let notification = content.wrappedJSObject._notification;
          notification.addEventListener(evt, function l(event) {
            notification.removeEventListener(evt, l);
            resolve({ defaultPrevented: event.defaultPrevented });
          });
        });
      });
    }
    yield openNotification(aBrowser, "showNotification1");
    info("Notification alert showing");
    let alertWindow = Services.wm.getMostRecentWindow("alert:alert");
    if (!alertWindow) {
      ok(true, "Notifications don't use XUL windows on all platforms.");
      yield closeNotification(aBrowser);
      return;
    }
    info("Clicking on notification");
    let promiseClickEvent = promiseNotificationEvent("click");

    // NB: This executeSoon is needed to allow the non-e10s runs of this test
    // a chance to set the event listener on the page. Otherwise, we
    // synchronously fire the click event before we listen for the event.
    executeSoon(() => {
      EventUtils.synthesizeMouseAtCenter(alertWindow.document.getElementById("alertTitleLabel"),
                                         {}, alertWindow);
    });
    let clickEvent = yield promiseClickEvent;
    ok(clickEvent.defaultPrevented, "The event handler for the first notification cancels the event");
    isnot(gBrowser.selectedBrowser, aBrowser, "Notification page still a background tab");
    let notificationClosed = promiseNotificationEvent("close");
    yield closeNotification(aBrowser);
    yield notificationClosed;

    // Second, show a notification that will cause the tab to get switched.
    yield openNotification(aBrowser, "showNotification2");
    alertWindow = Services.wm.getMostRecentWindow("alert:alert");
    let promiseTabSelect = BrowserTestUtils.waitForEvent(gBrowser.tabContainer, "TabSelect");
    EventUtils.synthesizeMouseAtCenter(alertWindow.document.getElementById("alertTitleLabel"),
                                       {},
                                       alertWindow);
    yield promiseTabSelect;
    is(gBrowser.selectedBrowser.currentURI.spec, notificationURL,
       "Clicking on the second notification should select its originating tab");
    notificationClosed = promiseNotificationEvent("close");
    yield closeNotification(aBrowser);
    yield notificationClosed;
  });
});

add_task(function* cleanup() {
  Services.perms.remove(makeURI(notificationURL), "desktop-notification");
});
