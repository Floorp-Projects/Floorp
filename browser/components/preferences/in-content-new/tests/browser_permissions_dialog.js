"use strict";

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

Components.utils.import("resource:///modules/SitePermissions.jsm");
Components.utils.import("resource://gre/modules/Services.jsm");

const PERMISSIONS_URL = "chrome://browser/content/preferences/sitePermissions.xul";
const URL = "http://www.example.com";
const URI = Services.io.newURI(URL);
var sitePermissionsDialog;

function checkPermissionItem(origin, state) {
  let doc = sitePermissionsDialog.document;

  let label = doc.getElementsByTagName("label")[0];
  Assert.equal(label.value, origin);

  let menulist = doc.getElementsByTagName("menulist")[0];
  Assert.equal(menulist.label, state);
}

async function openPermissionsDialog() {
  let dialogOpened = promiseLoadSubDialog(PERMISSIONS_URL);

  await ContentTask.spawn(gBrowser.selectedBrowser, null, function() {
    let doc = content.document;
    let settingsButton = doc.getElementById("notificationsPolicyButton");
    settingsButton.click();
  });

  sitePermissionsDialog = await dialogOpened;
}

add_task(async function openSitePermissionsDialog() {
  await openPreferencesViaOpenPreferencesAPI("privacy", {leaveOpen: true});
  await openPermissionsDialog();
});

add_task(async function addPermission() {
  let doc = sitePermissionsDialog.document;
  let richlistbox = doc.getElementById("permissionsBox");

  // First item in the richlistbox contains column headers.
  Assert.equal(richlistbox.itemCount - 1, 0,
               "Number of permission items is 0 initially");

  // Add notification permission for a website.
  SitePermissions.set(URI, "desktop-notification", SitePermissions.ALLOW);

  // Observe the added permission changes in the dialog UI.
  Assert.equal(richlistbox.itemCount - 1, 1);
  checkPermissionItem(URL, "Allow");

  SitePermissions.remove(URL, "desktop-notification");
});

add_task(async function observePermissionChange() {
  SitePermissions.set(URI, "desktop-notification", SitePermissions.ALLOW);

  // Change the permission.
  SitePermissions.set(URI, "desktop-notification", SitePermissions.BLOCK);

  checkPermissionItem(URL, "Block");

  SitePermissions.remove(URL, "desktop-notification");
});

add_task(async function observePermissionDelete() {
  let doc = sitePermissionsDialog.document;
  let richlistbox = doc.getElementById("permissionsBox");

  SitePermissions.set(URI, "desktop-notification", SitePermissions.ALLOW);

  Assert.equal(richlistbox.itemCount - 1, 1,
               "The box contains one permission item initially");

  SitePermissions.remove(URI, "desktop-notification");

  Assert.equal(richlistbox.itemCount - 1, 0);
});

add_task(async function onPermissionChange() {
  let doc = sitePermissionsDialog.document;
  SitePermissions.set(URI, "desktop-notification", SitePermissions.ALLOW);

  // Change the permission state in the UI.
  doc.getElementsByAttribute("value", SitePermissions.BLOCK)[0].click();

  Assert.equal(SitePermissions.get(URI, "desktop-notification").state,
               SitePermissions.ALLOW,
               "Permission state does not change before saving changes");

  doc.getElementById("btnApplyChanges").click();

  await waitForCondition(() =>
    SitePermissions.get(URI, "desktop-notification").state == SitePermissions.BLOCK);

  SitePermissions.remove(URI, "desktop-notification");
});

add_task(async function onPermissionDelete() {
  await openPermissionsDialog();

  let doc = sitePermissionsDialog.document;
  let richlistbox = doc.getElementById("permissionsBox");

  SitePermissions.set(URI, "desktop-notification", SitePermissions.ALLOW);

  richlistbox.selectItem(richlistbox.getItemAtIndex(1));
  doc.getElementById("removePermission").click();

  await waitForCondition(() => richlistbox.itemCount - 1 == 0);

  Assert.equal(SitePermissions.get(URI, "desktop-notification").state,
               SitePermissions.ALLOW,
               "Permission is not deleted before saving changes");

  doc.getElementById("btnApplyChanges").click();

  await waitForCondition(() =>
    SitePermissions.get(URI, "desktop-notification").state == SitePermissions.UNKNOWN);
});

add_task(async function onAllPermissionsDelete() {
  await openPermissionsDialog();

  let doc = sitePermissionsDialog.document;
  let richlistbox = doc.getElementById("permissionsBox");

  SitePermissions.set(URI, "desktop-notification", SitePermissions.ALLOW);
  let u = Services.io.newURI("http://www.test.com");
  SitePermissions.set(u, "desktop-notification", SitePermissions.ALLOW);

  doc.getElementById("removeAllPermissions").click();
  await waitForCondition(() => richlistbox.itemCount - 1 == 0);

  Assert.equal(SitePermissions.get(URI, "desktop-notification").state,
     SitePermissions.ALLOW);
  Assert.equal(SitePermissions.get(u, "desktop-notification").state,
    SitePermissions.ALLOW, "Permissions are not deleted before saving changes");

  doc.getElementById("btnApplyChanges").click();

  await waitForCondition(() =>
    (SitePermissions.get(URI, "desktop-notification").state == SitePermissions.UNKNOWN) &&
      (SitePermissions.get(u, "desktop-notification").state == SitePermissions.UNKNOWN));
});

add_task(async function onPermissionChangeAndDelete() {
  await openPermissionsDialog();

  let doc = sitePermissionsDialog.document;
  let richlistbox = doc.getElementById("permissionsBox");

  SitePermissions.set(URI, "desktop-notification", SitePermissions.ALLOW);

  // Change the permission state in the UI.
  doc.getElementsByAttribute("value", SitePermissions.BLOCK)[0].click();

  // Remove that permission by clicking the "Remove" button.
  richlistbox.selectItem(richlistbox.getItemAtIndex(1));
  doc.getElementById("removePermission").click();

  await waitForCondition(() => richlistbox.itemCount - 1 == 0);

  doc.getElementById("btnApplyChanges").click();

  await waitForCondition(() =>
    SitePermissions.get(URI, "desktop-notification").state == SitePermissions.UNKNOWN);
});

add_task(async function onPermissionChangeCancel() {
  await openPermissionsDialog();

  let doc = sitePermissionsDialog.document;
  SitePermissions.set(URI, "desktop-notification", SitePermissions.ALLOW);

  // Change the permission state in the UI.
  doc.getElementsByAttribute("value", SitePermissions.BLOCK)[0].click();

  doc.getElementById("cancel").click();

  Assert.equal(SitePermissions.get(URI, "desktop-notification").state,
               SitePermissions.ALLOW,
               "Permission state does not change on clicking cancel");

  SitePermissions.remove(URI, "desktop-notification");
});

add_task(async function onPermissionDeleteCancel() {
  await openPermissionsDialog();

  let doc = sitePermissionsDialog.document;
  let richlistbox = doc.getElementById("permissionsBox");
  SitePermissions.set(URI, "desktop-notification", SitePermissions.ALLOW);

  // Remove that permission by clicking the "Remove" button.
  richlistbox.selectItem(richlistbox.getItemAtIndex(1));
  doc.getElementById("removePermission").click();

  await waitForCondition(() => richlistbox.itemCount - 1 == 0);

  doc.getElementById("cancel").click();

  Assert.equal(SitePermissions.get(URI, "desktop-notification").state,
               SitePermissions.ALLOW,
               "Permission state does not change on clicking cancel");
});

add_task(async function removeTab() {
  gBrowser.removeCurrentTab();
});
