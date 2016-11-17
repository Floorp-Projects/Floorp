"use strict";

var tab;
var notificationURL = "http://example.org/browser/browser/base/content/test/alerts/file_dom_notifications.html";

const ALERT_SERVICE = Cc["@mozilla.org/alerts-service;1"]
                        .getService(Ci.nsIAlertsService)
                        .QueryInterface(Ci.nsIAlertsDoNotDisturb);

function test() {
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
  openNotification(tab.linkedBrowser, "showNotification2").then(onAlertShowing);
}

function onAlertShowing() {
  info("Notification alert showing");

  let alertWindow = Services.wm.getMostRecentWindow("alert:alert");
  if (!alertWindow) {
    ok(true, "Notifications don't use XUL windows on all platforms.");
    closeNotification(tab.linkedBrowser).then(finish);
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

  // The notification should not appear, but there is
  // no way from the client-side to know that it was
  // blocked, except for waiting some time and realizing
  // that the "onshow" event never fired.
  openNotification(tab.linkedBrowser, "showNotification2", 2000)
    .then(onAlert2Showing, finish);
}

function onAlert2Showing() {
  ok(false, "the second alert should not have been shown");
  closeNotification(tab.linkedBrowser).then(finish);
}
