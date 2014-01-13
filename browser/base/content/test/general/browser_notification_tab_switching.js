/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

"use strict";

let tab;
let notification;
let notificationURL = "http://example.org/browser/browser/base/content/test/general/file_dom_notifications.html";

function test () {
  waitForExplicitFinish();

  let pm = Services.perms;
  registerCleanupFunction(function() {
    pm.remove(notificationURL, "desktop-notification");
    gBrowser.removeTab(tab);
    window.restore();
  });

  pm.add(makeURI(notificationURL), "desktop-notification", pm.ALLOW_ACTION);

  tab = gBrowser.addTab(notificationURL);
  tab.linkedBrowser.addEventListener("load", onLoad, true);
}

function onLoad() {
  isnot(gBrowser.selectedTab, tab, "Notification page loaded as a background tab");
  tab.linkedBrowser.removeEventListener("load", onLoad, true);
  let win = tab.linkedBrowser.contentWindow.wrappedJSObject;
  notification = win.showNotification();
  notification.addEventListener("show", onAlertShowing);
}

function onAlertShowing() {
  info("Notification alert showing");
  notification.removeEventListener("show", onAlertShowing);

  gBrowser.tabContainer.addEventListener("TabSelect", onTabSelect);
  let alertWindow = findChromeWindowByURI("chrome://global/content/alerts/alert.xul");
  if (!alertWindow) {
    todo("Notifications don't use XUL windows on all platforms.");
    notification.close();
    finish();
  }
  EventUtils.synthesizeMouseAtCenter(alertWindow.document.getElementById("alertTitleLabel"), {});
  alertWindow.close();
}

function onTabSelect() {
  gBrowser.tabContainer.removeEventListener("TabSelect", onTabSelect);
  is(gBrowser.selectedTab.linkedBrowser.contentWindow.location.href, notificationURL,
     "Notification tab should be selected.");

  finish();
}
