"use strict";

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var { SitePermissions } = ChromeUtils.import(
  "resource:///modules/SitePermissions.jsm"
);

var { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

const PERMISSIONS_URL =
  "chrome://browser/content/preferences/dialogs/sitePermissions.xhtml";
const URL = "http://www.example.com";
const URI = Services.io.newURI(URL);
var sitePermissionsDialog;

function checkPermissionItem(origin, state) {
  let doc = sitePermissionsDialog.document;

  let label = doc.getElementsByTagName("label")[3];
  Assert.equal(label.value, origin);

  let menulist = doc.getElementsByTagName("menulist")[0];
  Assert.equal(menulist.value, state);
}

async function openPermissionsDialog() {
  let dialogOpened = promiseLoadSubDialog(PERMISSIONS_URL);

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], function() {
    let doc = content.document;
    let settingsButton = doc.getElementById("notificationSettingsButton");
    settingsButton.click();
  });

  sitePermissionsDialog = await dialogOpened;
  await sitePermissionsDialog.document.mozSubdialogReady;
}

add_task(async function openSitePermissionsDialog() {
  await openPreferencesViaOpenPreferencesAPI("privacy", { leaveOpen: true });
  await openPermissionsDialog();
});

add_task(async function addPermission() {
  let doc = sitePermissionsDialog.document;
  let richlistbox = doc.getElementById("permissionsBox");

  // First item in the richlistbox contains column headers.
  Assert.equal(
    richlistbox.itemCount,
    0,
    "Number of permission items is 0 initially"
  );

  // Add notification permission for a website.
  PermissionTestUtils.add(
    URI,
    "desktop-notification",
    Services.perms.ALLOW_ACTION
  );

  // Observe the added permission changes in the dialog UI.
  Assert.equal(richlistbox.itemCount, 1);
  checkPermissionItem(URL, Services.perms.ALLOW_ACTION);

  PermissionTestUtils.remove(URI, "desktop-notification");
});

add_task(async function addPermissionPrivateBrowsing() {
  let privateBrowsingPrincipal = Services.scriptSecurityManager.createContentPrincipal(
    URI,
    { privateBrowsingId: 1 }
  );
  let doc = sitePermissionsDialog.document;
  let richlistbox = doc.getElementById("permissionsBox");

  Assert.equal(
    richlistbox.itemCount,
    0,
    "Number of permission items is 0 initially"
  );

  // Add a session permission for private browsing.
  PermissionTestUtils.add(
    privateBrowsingPrincipal,
    "desktop-notification",
    Services.perms.ALLOW_ACTION,
    Services.perms.EXPIRE_SESSION
  );

  // The permission should not show in the dialog UI.
  Assert.equal(richlistbox.itemCount, 0);

  PermissionTestUtils.remove(privateBrowsingPrincipal, "desktop-notification");

  // Add a permanent permission for private browsing
  // The permission manager will store it as EXPIRE_SESSION
  PermissionTestUtils.add(
    privateBrowsingPrincipal,
    "desktop-notification",
    Services.perms.ALLOW_ACTION
  );

  // The permission should not show in the dialog UI.
  Assert.equal(richlistbox.itemCount, 0);

  PermissionTestUtils.remove(privateBrowsingPrincipal, "desktop-notification");
});

add_task(async function observePermissionChange() {
  PermissionTestUtils.add(
    URI,
    "desktop-notification",
    Services.perms.ALLOW_ACTION
  );

  // Change the permission.
  PermissionTestUtils.add(
    URI,
    "desktop-notification",
    Services.perms.DENY_ACTION
  );

  checkPermissionItem(URL, Services.perms.DENY_ACTION);

  PermissionTestUtils.remove(URI, "desktop-notification");
});

add_task(async function observePermissionDelete() {
  let doc = sitePermissionsDialog.document;
  let richlistbox = doc.getElementById("permissionsBox");

  PermissionTestUtils.add(
    URI,
    "desktop-notification",
    Services.perms.ALLOW_ACTION
  );

  Assert.equal(
    richlistbox.itemCount,
    1,
    "The box contains one permission item initially"
  );

  PermissionTestUtils.remove(URI, "desktop-notification");

  Assert.equal(richlistbox.itemCount, 0);
});

add_task(async function onPermissionChange() {
  let doc = sitePermissionsDialog.document;
  PermissionTestUtils.add(
    URI,
    "desktop-notification",
    Services.perms.ALLOW_ACTION
  );

  // Change the permission state in the UI.
  doc.getElementsByAttribute("value", SitePermissions.BLOCK)[0].click();

  Assert.equal(
    PermissionTestUtils.getPermissionObject(URI, "desktop-notification")
      .capability,
    Services.perms.ALLOW_ACTION,
    "Permission state does not change before saving changes"
  );

  doc.getElementById("btnApplyChanges").click();

  await TestUtils.waitForCondition(
    () =>
      PermissionTestUtils.getPermissionObject(URI, "desktop-notification")
        .capability == Services.perms.DENY_ACTION
  );

  PermissionTestUtils.remove(URI, "desktop-notification");
});

add_task(async function onPermissionDelete() {
  await openPermissionsDialog();

  let doc = sitePermissionsDialog.document;
  let richlistbox = doc.getElementById("permissionsBox");

  PermissionTestUtils.add(
    URI,
    "desktop-notification",
    Services.perms.ALLOW_ACTION
  );

  richlistbox.selectItem(richlistbox.getItemAtIndex(0));
  doc.getElementById("removePermission").click();

  await TestUtils.waitForCondition(() => richlistbox.itemCount == 0);

  Assert.equal(
    PermissionTestUtils.getPermissionObject(URI, "desktop-notification")
      .capability,
    Services.perms.ALLOW_ACTION,
    "Permission is not deleted before saving changes"
  );

  doc.getElementById("btnApplyChanges").click();

  await TestUtils.waitForCondition(
    () =>
      PermissionTestUtils.getPermissionObject(URI, "desktop-notification") ==
      null
  );
});

add_task(async function onAllPermissionsDelete() {
  await openPermissionsDialog();

  let doc = sitePermissionsDialog.document;
  let richlistbox = doc.getElementById("permissionsBox");

  PermissionTestUtils.add(
    URI,
    "desktop-notification",
    Services.perms.ALLOW_ACTION
  );
  let u = Services.io.newURI("http://www.test.com");
  PermissionTestUtils.add(
    u,
    "desktop-notification",
    Services.perms.ALLOW_ACTION
  );

  doc.getElementById("removeAllPermissions").click();
  await TestUtils.waitForCondition(() => richlistbox.itemCount == 0);

  Assert.equal(
    PermissionTestUtils.getPermissionObject(URI, "desktop-notification")
      .capability,
    Services.perms.ALLOW_ACTION
  );
  Assert.equal(
    PermissionTestUtils.getPermissionObject(u, "desktop-notification")
      .capability,
    Services.perms.ALLOW_ACTION,
    "Permissions are not deleted before saving changes"
  );

  doc.getElementById("btnApplyChanges").click();

  await TestUtils.waitForCondition(
    () =>
      PermissionTestUtils.getPermissionObject(URI, "desktop-notification") ==
        null &&
      PermissionTestUtils.getPermissionObject(u, "desktop-notification") == null
  );
});

add_task(async function onPermissionChangeAndDelete() {
  await openPermissionsDialog();

  let doc = sitePermissionsDialog.document;
  let richlistbox = doc.getElementById("permissionsBox");

  PermissionTestUtils.add(
    URI,
    "desktop-notification",
    Services.perms.ALLOW_ACTION
  );

  // Change the permission state in the UI.
  doc.getElementsByAttribute("value", SitePermissions.BLOCK)[0].click();

  // Remove that permission by clicking the "Remove" button.
  richlistbox.selectItem(richlistbox.getItemAtIndex(0));
  doc.getElementById("removePermission").click();

  await TestUtils.waitForCondition(() => richlistbox.itemCount == 0);

  doc.getElementById("btnApplyChanges").click();

  await TestUtils.waitForCondition(
    () =>
      PermissionTestUtils.getPermissionObject(URI, "desktop-notification") ==
      null
  );
});

add_task(async function onPermissionChangeCancel() {
  await openPermissionsDialog();

  let doc = sitePermissionsDialog.document;
  PermissionTestUtils.add(
    URI,
    "desktop-notification",
    Services.perms.ALLOW_ACTION
  );

  // Change the permission state in the UI.
  doc.getElementsByAttribute("value", SitePermissions.BLOCK)[0].click();

  doc.getElementById("cancel").click();

  Assert.equal(
    PermissionTestUtils.getPermissionObject(URI, "desktop-notification")
      .capability,
    Services.perms.ALLOW_ACTION,
    "Permission state does not change on clicking cancel"
  );

  PermissionTestUtils.remove(URI, "desktop-notification");
});

add_task(async function onPermissionDeleteCancel() {
  await openPermissionsDialog();

  let doc = sitePermissionsDialog.document;
  let richlistbox = doc.getElementById("permissionsBox");
  PermissionTestUtils.add(
    URI,
    "desktop-notification",
    Services.perms.ALLOW_ACTION
  );

  // Remove that permission by clicking the "Remove" button.
  richlistbox.selectItem(richlistbox.getItemAtIndex(0));
  doc.getElementById("removePermission").click();

  await TestUtils.waitForCondition(() => richlistbox.itemCount == 0);

  doc.getElementById("cancel").click();

  Assert.equal(
    PermissionTestUtils.getPermissionObject(URI, "desktop-notification")
      .capability,
    Services.perms.ALLOW_ACTION,
    "Permission state does not change on clicking cancel"
  );

  PermissionTestUtils.remove(URI, "desktop-notification");
});

add_task(async function onSearch() {
  await openPermissionsDialog();
  let doc = sitePermissionsDialog.document;
  let richlistbox = doc.getElementById("permissionsBox");
  let searchBox = doc.getElementById("searchBox");

  PermissionTestUtils.add(
    URI,
    "desktop-notification",
    Services.perms.ALLOW_ACTION
  );
  searchBox.value = "www.example.com";

  let u = Services.io.newURI("http://www.test.com");
  PermissionTestUtils.add(
    u,
    "desktop-notification",
    Services.perms.ALLOW_ACTION
  );

  Assert.equal(
    doc.getElementsByAttribute("origin", "http://www.test.com")[0],
    null
  );
  Assert.equal(
    doc.getElementsByAttribute("origin", "http://www.example.com")[0],
    richlistbox.getItemAtIndex(0)
  );

  PermissionTestUtils.remove(URI, "desktop-notification");
  PermissionTestUtils.remove(u, "desktop-notification");

  doc.getElementById("cancel").click();
});

add_task(async function onPermissionsSort() {
  PermissionTestUtils.add(
    URI,
    "desktop-notification",
    Services.perms.ALLOW_ACTION
  );
  let u = Services.io.newURI("http://www.test.com");
  PermissionTestUtils.add(
    u,
    "desktop-notification",
    Services.perms.DENY_ACTION
  );

  await openPermissionsDialog();
  let doc = sitePermissionsDialog.document;
  let richlistbox = doc.getElementById("permissionsBox");

  // Test default arrangement(Allow followed by Block).
  Assert.equal(
    richlistbox.getItemAtIndex(0).getAttribute("origin"),
    "http://www.example.com"
  );
  Assert.equal(
    richlistbox.getItemAtIndex(1).getAttribute("origin"),
    "http://www.test.com"
  );

  doc.getElementById("statusCol").click();

  // Test the rearrangement(Block followed by Allow).
  Assert.equal(
    richlistbox.getItemAtIndex(0).getAttribute("origin"),
    "http://www.test.com"
  );
  Assert.equal(
    richlistbox.getItemAtIndex(1).getAttribute("origin"),
    "http://www.example.com"
  );

  doc.getElementById("siteCol").click();

  // Test the rearrangement(Website names arranged in alphabhetical order).
  Assert.equal(
    richlistbox.getItemAtIndex(0).getAttribute("origin"),
    "http://www.example.com"
  );
  Assert.equal(
    richlistbox.getItemAtIndex(1).getAttribute("origin"),
    "http://www.test.com"
  );

  doc.getElementById("siteCol").click();

  // Test the rearrangement(Website names arranged in reverse alphabhetical order).
  Assert.equal(
    richlistbox.getItemAtIndex(0).getAttribute("origin"),
    "http://www.test.com"
  );
  Assert.equal(
    richlistbox.getItemAtIndex(1).getAttribute("origin"),
    "http://www.example.com"
  );

  PermissionTestUtils.remove(URI, "desktop-notification");
  PermissionTestUtils.remove(u, "desktop-notification");

  doc.getElementById("cancel").click();
});

add_task(async function onPermissionDisable() {
  // Enable desktop-notification permission prompts.
  Services.prefs.setIntPref(
    "permissions.default.desktop-notification",
    SitePermissions.UNKNOWN
  );

  await openPermissionsDialog();
  let doc = sitePermissionsDialog.document;

  // Check if the enabled state is reflected in the checkbox.
  let checkbox = doc.getElementById("permissionsDisableCheckbox");
  Assert.equal(checkbox.checked, false);

  // Disable permission and click on "Cancel".
  checkbox.checked = true;
  doc.getElementById("cancel").click();

  // Check that the permission is not disabled yet.
  let perm = Services.prefs.getIntPref(
    "permissions.default.desktop-notification"
  );
  Assert.equal(perm, SitePermissions.UNKNOWN);

  // Open the dialog once again.
  await openPermissionsDialog();
  doc = sitePermissionsDialog.document;

  // Disable permission and save changes.
  checkbox = doc.getElementById("permissionsDisableCheckbox");
  checkbox.checked = true;
  doc.getElementById("btnApplyChanges").click();

  // Check if the permission is now disabled.
  perm = Services.prefs.getIntPref("permissions.default.desktop-notification");
  Assert.equal(perm, SitePermissions.BLOCK);

  // Open the dialog once again and check if the disabled state is still reflected in the checkbox.
  await openPermissionsDialog();
  doc = sitePermissionsDialog.document;
  checkbox = doc.getElementById("permissionsDisableCheckbox");
  Assert.equal(checkbox.checked, true);

  // Close the dialog and clean up.
  doc.getElementById("cancel").click();
  Services.prefs.setIntPref(
    "permissions.default.desktop-notification",
    SitePermissions.UNKNOWN
  );
});

add_task(async function checkDefaultPermissionState() {
  // Set default permission state to ALLOW.
  Services.prefs.setIntPref(
    "permissions.default.desktop-notification",
    SitePermissions.ALLOW
  );

  await openPermissionsDialog();
  let doc = sitePermissionsDialog.document;

  // Check if the enabled state is reflected in the checkbox.
  let checkbox = doc.getElementById("permissionsDisableCheckbox");
  Assert.equal(checkbox.checked, false);

  // Check the checkbox and then uncheck it.
  checkbox.checked = true;
  checkbox.checked = false;

  // Save changes.
  doc.getElementById("btnApplyChanges").click();

  // Check if the default permission state is retained (and not automatically set to SitePermissions.UNKNOWN).
  let state = Services.prefs.getIntPref(
    "permissions.default.desktop-notification"
  );
  Assert.equal(state, SitePermissions.ALLOW);

  // Clean up.
  Services.prefs.setIntPref(
    "permissions.default.desktop-notification",
    SitePermissions.UNKNOWN
  );
});

add_task(async function testTabBehaviour() {
  // Test tab behaviour inside the permissions setting dialog when site permissions are selected.
  // Only selected items in the richlistbox should be tabable for accessibility reasons.

  // Force tabfocus for all elements on OSX.
  SpecialPowers.pushPrefEnv({ set: [["accessibility.tabfocus", 7]] });

  PermissionTestUtils.add(
    URI,
    "desktop-notification",
    Services.perms.ALLOW_ACTION
  );
  let u = Services.io.newURI("http://www.test.com");
  PermissionTestUtils.add(
    u,
    "desktop-notification",
    Services.perms.ALLOW_ACTION
  );

  await openPermissionsDialog();
  let doc = sitePermissionsDialog.document;

  EventUtils.synthesizeKey("KEY_Tab", {}, sitePermissionsDialog);
  let richlistbox = doc.getElementById("permissionsBox");
  is(
    richlistbox,
    doc.activeElement.closest("#permissionsBox"),
    "The richlistbox is focused after pressing tab once."
  );

  EventUtils.synthesizeKey("KEY_ArrowDown", {}, sitePermissionsDialog);
  EventUtils.synthesizeKey("KEY_Tab", {}, sitePermissionsDialog);
  let menulist = doc
    .getElementById("permissionsBox")
    .itemChildren[1].getElementsByTagName("menulist")[0];
  is(
    menulist,
    doc.activeElement,
    "The menulist inside the selected richlistitem is focused now"
  );

  EventUtils.synthesizeKey("KEY_Tab", {}, sitePermissionsDialog);
  let removeButton = doc.getElementById("removePermission");
  is(
    removeButton,
    doc.activeElement,
    "The focus moves outside the richlistbox and onto the remove button"
  );

  PermissionTestUtils.remove(URI, "desktop-notification");
  PermissionTestUtils.remove(u, "desktop-notification");

  doc.getElementById("cancel").click();
});

add_task(async function removeTab() {
  gBrowser.removeCurrentTab();
});
