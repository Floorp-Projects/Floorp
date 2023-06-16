/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const PERMISSIONS_URL =
  "chrome://browser/content/preferences/dialogs/permissions.xhtml";

const TRACKING_URL = "https://example.com";

async function openETPExceptionsDialog(doc) {
  let exceptionsButton = doc.getElementById("trackingProtectionExceptions");
  ok(exceptionsButton, "trackingProtectionExceptions button found");
  let dialogPromise = promiseLoadSubDialog(PERMISSIONS_URL);
  exceptionsButton.click();
  let dialog = await dialogPromise;
  return dialog;
}

async function addETPPermission(doc) {
  let dialog = await openETPExceptionsDialog(doc);
  let url = dialog.document.getElementById("url");
  let buttonDisableETP = dialog.document.getElementById("btnDisableETP");
  let permissionsBox = dialog.document.getElementById("permissionsBox");
  let currentPermissions = permissionsBox.itemCount;

  url.value = TRACKING_URL;
  url.dispatchEvent(new Event("input", { bubbles: true }));
  is(
    buttonDisableETP.hasAttribute("disabled"),
    false,
    "Disable ETP button is selectable after url is entered"
  );
  buttonDisableETP.click();

  // Website is listed
  is(
    permissionsBox.itemCount,
    currentPermissions + 1,
    "Website added in url should be in the list"
  );
  let saveButton = dialog.document.querySelector("dialog").getButton("accept");
  saveButton.click();
  BrowserTestUtils.waitForEvent(dialog, "unload");
}

async function removeETPPermission(doc) {
  let dialog = await openETPExceptionsDialog(doc);
  let permissionsBox = dialog.document.getElementById("permissionsBox");
  let elements = permissionsBox.getElementsByAttribute("origin", TRACKING_URL);
  // Website is listed
  ok(permissionsBox.itemCount, "List is not empty");
  permissionsBox.selectItem(elements[0]);
  let removePermissionButton =
    dialog.document.getElementById("removePermission");
  is(
    removePermissionButton.hasAttribute("disabled"),
    false,
    "The button should be clickable to remove selected item"
  );
  removePermissionButton.click();

  let saveButton = dialog.document.querySelector("dialog").getButton("accept");
  saveButton.click();
  BrowserTestUtils.waitForEvent(dialog, "unload");
}

async function checkShieldIcon(shieldIcon) {
  // Open the website and check that the tracking protection icon is enabled/disabled
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TRACKING_URL);
  let icon = document.getElementById("tracking-protection-icon");
  is(
    gBrowser.ownerGlobal
      .getComputedStyle(icon)
      .getPropertyValue("list-style-image"),
    shieldIcon,
    `The tracking protection icon shows the icon ${shieldIcon}`
  );
  BrowserTestUtils.removeTab(tab);
}

// test adds and removes an ETP permission via the about:preferences#privacy and checks if the ProtectionsUI shield icon resembles the state
add_task(async function ETPPermissionSyncedFromPrivacyPane() {
  await openPreferencesViaOpenPreferencesAPI("panePrivacy", {
    leaveOpen: true,
  });
  let win = gBrowser.selectedBrowser.contentWindow;
  let doc = win.document;
  await addETPPermission(doc);
  await checkShieldIcon(
    `url("chrome://browser/skin/tracking-protection-disabled.svg")`
  );
  await removeETPPermission(doc);
  await checkShieldIcon(`url("chrome://browser/skin/tracking-protection.svg")`);
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});
