"use strict";

var tab;
var notification;
var notification2;
var notificationURL = "http://example.org/browser/browser/base/content/test/alerts/file_dom_notifications.html";

const ALERT_SERVICE = Cc["@mozilla.org/alerts-service;1"]
                        .getService(Ci.nsIAlertsService)
                        .QueryInterface(Ci.nsIAlertsDoNotDisturb);

function test () {
  waitForExplicitFinish();

  try {
    // Only run the test if the do-not-disturb
    // interface has been implemented.
    ALERT_SERVICE.manualDoNotDisturb;
    ok(true, "Alert service implements do-not-disturb interface");
  } catch (e) {
    ok(true, "Alert service doesn't implement do-not-disturb interface, exiting test");
    finish();
    return;
  }

  let pm = Services.perms;
  registerCleanupFunction(function() {
    ALERT_SERVICE.manualDoNotDisturb = false;
    pm.remove(makeURI(notificationURL), "desktop-notification");
    gBrowser.removeTab(tab);
    window.restore();
  });

  pm.add(makeURI(notificationURL), "desktop-notification", pm.ALLOW_ACTION);

  // Make sure that do-not-disturb is not enabled.
  ok(!ALERT_SERVICE.manualDoNotDisturb, "Alert service should not be disabled when test starts");
  ALERT_SERVICE.manualDoNotDisturb = false;

  tab = gBrowser.addTab(notificationURL);
  gBrowser.selectedTab = tab;
  tab.linkedBrowser.addEventListener("load", onLoad, true);
}

function onLoad() {
  tab.linkedBrowser.removeEventListener("load", onLoad, true);
  let win = tab.linkedBrowser.contentWindow.wrappedJSObject;
  notification = win.showNotification2();
  notification.addEventListener("show", onAlertShowing);
}

function onAlertShowing() {
  info("Notification alert showing");
  notification.removeEventListener("show", onAlertShowing);

  let alertWindow = Services.wm.getMostRecentWindow("alert:alert");
  if (!alertWindow) {
    ok(true, "Notifications don't use XUL windows on all platforms.");
    notification.close();
    finish();
    return;
  }
  let doNotDisturbMenuItem = alertWindow.document.getElementById("doNotDisturbMenuItem");
  is(doNotDisturbMenuItem.localName, "menuitem", "menuitem found");
  alertWindow.addEventListener("beforeunload", onAlertClosing);
  doNotDisturbMenuItem.click();
  info("Clicked on do-not-disturb menuitem");
}

function onAlertClosing(event) {
  event.target.removeEventListener("beforeunload", onAlertClosing);

  ok(ALERT_SERVICE.manualDoNotDisturb, "Alert service should be disabled after clicking menuitem");
  let win = tab.linkedBrowser.contentWindow.wrappedJSObject;
  notification2 = win.showNotification2();
  notification2.addEventListener("show", onAlert2Showing);

  // The notification should not appear, but there is
  // no way from the client-side to know that it was
  // blocked, except for waiting some time and realizing
  // that the "onshow" event never fired.
  setTimeout(function() {
    notification2.removeEventListener("show", onAlert2Showing);
    finish();
  }, 2000);
}

function onAlert2Showing() {
  ok(false, "the second alert should not have been shown");
  notification2.close();
}
