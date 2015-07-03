/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

"use strict";

let tab;
let notification;
let notificationURL = "http://example.org/browser/browser/base/content/test/general/file_dom_notifications.html";
let newWindowOpenedFromTab;

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
  win.newWindow = win.open("about:blank", "", "height=100,width=100");
  newWindowOpenedFromTab = win.newWindow;
  win.newWindow.addEventListener("load", function() {
    info("new window loaded");
    win.newWindow.addEventListener("blur", function b() {
      info("new window got blur");
      win.newWindow.removeEventListener("blur", b);
      notification = win.showNotification1();
      win.newWindow.addEventListener("focus", onNewWindowFocused);
      notification.addEventListener("show", onAlertShowing);
    });

    function waitUntilNewWindowHasFocus() {
      if (!win.newWindow.document.hasFocus()) {
        setTimeout(waitUntilNewWindowHasFocus, 50);
      } else {
        // Focus another window so that new window gets blur event.
        gBrowser.selectedBrowser.contentWindow.focus();
      }
    }
    win.newWindow.focus();
    waitUntilNewWindowHasFocus();
  });
}

function onAlertShowing() {
  info("Notification alert showing");
  notification.removeEventListener("show", onAlertShowing);

  let alertWindow = findChromeWindowByURI("chrome://global/content/alerts/alert.xul");
  if (!alertWindow) {
    todo(false, "Notifications don't use XUL windows on all platforms.");
    notification.close();
    newWindowOpenedFromTab.close();
    finish();
    return;
  }
  gBrowser.tabContainer.addEventListener("TabSelect", onTabSelect);
  EventUtils.synthesizeMouseAtCenter(alertWindow.document.getElementById("alertTitleLabel"), {}, alertWindow);
  info("Clicked on notification");
  alertWindow.close();
}

function onNewWindowFocused(event) {
  event.target.close();
  isnot(gBrowser.selectedTab, tab, "Notification page loaded as a background tab");
  // Using timeout to test that something do *not* happen!
  setTimeout(openSecondNotification, 50);
}

function openSecondNotification() {
  isnot(gBrowser.selectedTab, tab, "Notification page loaded as a background tab");
  let win = tab.linkedBrowser.contentWindow.wrappedJSObject;
  notification = win.showNotification2();
  notification.addEventListener("show", onAlertShowing);
}

function onTabSelect() {
  gBrowser.tabContainer.removeEventListener("TabSelect", onTabSelect);
  is(gBrowser.selectedBrowser.contentWindow.location.href, notificationURL,
     "Notification tab should be selected.");

  finish();
}
