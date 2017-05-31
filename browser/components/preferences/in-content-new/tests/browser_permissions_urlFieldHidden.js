"use strict";

const PERMISSIONS_URL = "chrome://browser/content/preferences/permissions.xul";

add_task(async function urlFieldVisibleForPopupPermissions(finish) {
  await openPreferencesViaOpenPreferencesAPI("panePrivacy", {leaveOpen: true});
  let win = gBrowser.selectedBrowser.contentWindow;
  let doc = win.document;
  let popupPolicyCheckbox = doc.getElementById("popupPolicy");
  ok(!popupPolicyCheckbox.checked, "popupPolicyCheckbox should be unchecked by default");
  popupPolicyCheckbox.click();
  let popupPolicyButton = doc.getElementById("popupPolicyButton");
  ok(popupPolicyButton, "popupPolicyButton found");
  let dialogPromise = promiseLoadSubDialog(PERMISSIONS_URL);
  popupPolicyButton.click();
  let dialog = await dialogPromise;
  ok(dialog, "dialog loaded");

  let urlLabel = dialog.document.getElementById("urlLabel");
  ok(!urlLabel.hidden, "urlLabel should be visible when one of block/session/allow visible");
  let url = dialog.document.getElementById("url");
  ok(!url.hidden, "url should be visible when one of block/session/allow visible");

  popupPolicyCheckbox.click();
  gBrowser.removeCurrentTab();
});

add_task(async function urlFieldHiddenForNotificationPermissions() {
  await openPreferencesViaOpenPreferencesAPI("panePrivacy", {leaveOpen: true});
  let win = gBrowser.selectedBrowser.contentWindow;
  let doc = win.document;
  let notificationsPolicyButton = doc.getElementById("notificationsPolicyButton");
  ok(notificationsPolicyButton, "notificationsPolicyButton found");
  let dialogPromise = promiseLoadSubDialog(PERMISSIONS_URL);
  notificationsPolicyButton.click();
  let dialog = await dialogPromise;
  ok(dialog, "dialog loaded");

  let urlLabel = dialog.document.getElementById("urlLabel");
  ok(urlLabel.hidden, "urlLabel should be hidden as requested");
  let url = dialog.document.getElementById("url");
  ok(url.hidden, "url should be hidden as requested");

  gBrowser.removeCurrentTab();
});
