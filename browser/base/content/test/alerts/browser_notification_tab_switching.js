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
    let win = aBrowser.contentWindow.wrappedJSObject;
    let notification = win.showNotification1();
    yield BrowserTestUtils.waitForEvent(notification, "show");
    info("Notification alert showing");
    let alertWindow = Services.wm.getMostRecentWindow("alert:alert");
    if (!alertWindow) {
      ok(true, "Notifications don't use XUL windows on all platforms.");
      notification.close();
      return;
    }
    info("Clicking on notification");
    let promiseClickEvent = BrowserTestUtils.waitForEvent(notification, "click");
    EventUtils.synthesizeMouseAtCenter(alertWindow.document.getElementById("alertTitleLabel"),
                                       {},
                                       alertWindow);
    let clickEvent = yield promiseClickEvent;
    ok(clickEvent.defaultPrevented, "The event handler for the first notification cancels the event");
    isnot(gBrowser.selectedBrowser, aBrowser, "Notification page still a background tab");
    notification.close();
    yield BrowserTestUtils.waitForEvent(notification, "close");

    // Second, show a notification that will cause the tab to get switched.
    let notification2 = win.showNotification2();
    yield BrowserTestUtils.waitForEvent(notification2, "show");
    alertWindow = Services.wm.getMostRecentWindow("alert:alert");
    let promiseTabSelect = BrowserTestUtils.waitForEvent(gBrowser.tabContainer, "TabSelect");
    EventUtils.synthesizeMouseAtCenter(alertWindow.document.getElementById("alertTitleLabel"),
                                       {},
                                       alertWindow);
    yield promiseTabSelect;
    is(gBrowser.selectedBrowser.currentURI.spec, notificationURL,
       "Clicking on the second notification should select its originating tab");
    notification2.close();
    yield BrowserTestUtils.waitForEvent(notification2, "close");
  });
});

add_task(function* cleanup() {
  Services.perms.remove(makeURI(notificationURL), "desktop-notification");
});
