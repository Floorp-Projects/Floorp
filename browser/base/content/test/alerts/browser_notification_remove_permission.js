"use strict";

var tab;
var notificationURL = "http://example.org/browser/browser/base/content/test/alerts/file_dom_notifications.html";
var alertWindowClosed = false;
var permRemoved = false;

function test() {
  waitForExplicitFinish();

  let pm = Services.perms;
  registerCleanupFunction(function() {
    pm.remove(makeURI(notificationURL), "desktop-notification");
    gBrowser.removeTab(tab);
    window.restore();
  });

  pm.add(makeURI(notificationURL), "desktop-notification", pm.ALLOW_ACTION);

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
  ok(Services.perms.testExactPermission(makeURI(notificationURL), "desktop-notification"),
     "Permission should exist prior to removal");
  let disableForOriginMenuItem = alertWindow.document.getElementById("disableForOriginMenuItem");
  is(disableForOriginMenuItem.localName, "menuitem", "menuitem found");
  Services.obs.addObserver(permObserver, "perm-changed");
  alertWindow.addEventListener("beforeunload", onAlertClosing);
  disableForOriginMenuItem.click();
  info("Clicked on disable-for-origin menuitem")
}

function permObserver(subject, topic, data) {
  if (topic != "perm-changed") {
    return;
  }

  let permission = subject.QueryInterface(Ci.nsIPermission);
  is(permission.type, "desktop-notification", "desktop-notification permission changed");
  is(data, "deleted", "desktop-notification permission deleted");

  Services.obs.removeObserver(permObserver, "perm-changed");
  permRemoved = true;
  if (alertWindowClosed) {
    finish();
  }
}

function onAlertClosing(event) {
  event.target.removeEventListener("beforeunload", onAlertClosing);

  alertWindowClosed = true;
  if (permRemoved) {
    finish();
  }
}
