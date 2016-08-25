/*
 * Test the Permissions section in the Control Center.
 */

var {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;
const PERMISSIONS_PAGE = "http://example.com/browser/browser/base/content/test/general/permissions.html";
var {SitePermissions} = Cu.import("resource:///modules/SitePermissions.jsm", {});

registerCleanupFunction(function() {
  SitePermissions.remove(gBrowser.currentURI, "cookie");
  SitePermissions.remove(gBrowser.currentURI, "geo");
  SitePermissions.remove(gBrowser.currentURI, "camera");
  SitePermissions.remove(gBrowser.currentURI, "microphone");

  while (gBrowser.tabs.length > 1) {
    gBrowser.removeCurrentTab();
  }
});

function* openIdentityPopup() {
  let {gIdentityHandler} = gBrowser.ownerGlobal;
  let promise = BrowserTestUtils.waitForEvent(gIdentityHandler._identityPopup, "popupshown");
  gIdentityHandler._identityBox.click();
  return promise;
}

function* closeIdentityPopup() {
  let {gIdentityHandler} = gBrowser.ownerGlobal;
  let promise = BrowserTestUtils.waitForEvent(gIdentityHandler._identityPopup, "popuphidden");
  gIdentityHandler._identityPopup.hidePopup();
  return promise;
}

add_task(function* testMainViewVisible() {
  let {gIdentityHandler} = gBrowser.ownerGlobal;
  let tab = gBrowser.selectedTab = gBrowser.addTab();
  yield promiseTabLoadEvent(tab, PERMISSIONS_PAGE);

  let permissionsList = document.getElementById("identity-popup-permission-list");
  let emptyLabel = permissionsList.nextSibling;

  yield openIdentityPopup();

  ok(!is_hidden(emptyLabel), "List of permissions is empty");

  yield closeIdentityPopup();

  SitePermissions.set(gBrowser.currentURI, "camera", SitePermissions.ALLOW);

  yield openIdentityPopup();

  ok(is_hidden(emptyLabel), "List of permissions is not empty");

  let labelText = SitePermissions.getPermissionLabel("camera");
  let labels = permissionsList.querySelectorAll(".identity-popup-permission-label");
  is(labels.length, 1, "One permission visible in main view");
  is(labels[0].textContent, labelText, "Correct value");

  let img = permissionsList.querySelector("image.identity-popup-permission-icon");
  ok(img, "There is an image for the permissions");
  ok(img.classList.contains("camera-icon"), "proper class is in image class");

  yield closeIdentityPopup();

  SitePermissions.remove(gBrowser.currentURI, "camera");

  yield openIdentityPopup();

  ok(!is_hidden(emptyLabel), "List of permissions is empty");

  yield closeIdentityPopup();
});

add_task(function* testIdentityIcon() {
  let {gIdentityHandler} = gBrowser.ownerGlobal;
  let tab = gBrowser.selectedTab = gBrowser.addTab();
  yield promiseTabLoadEvent(tab, PERMISSIONS_PAGE);

  SitePermissions.set(gBrowser.currentURI, "geo", SitePermissions.ALLOW);

  ok(gIdentityHandler._identityBox.classList.contains("grantedPermissions"),
    "identity-box signals granted permissions");

  SitePermissions.remove(gBrowser.currentURI, "geo");

  ok(!gIdentityHandler._identityBox.classList.contains("grantedPermissions"),
    "identity-box doesn't signal granted permissions");

  SitePermissions.set(gBrowser.currentURI, "camera", SitePermissions.BLOCK);

  ok(!gIdentityHandler._identityBox.classList.contains("grantedPermissions"),
    "identity-box doesn't signal granted permissions");

  SitePermissions.set(gBrowser.currentURI, "cookie", SitePermissions.SESSION);

  ok(gIdentityHandler._identityBox.classList.contains("grantedPermissions"),
    "identity-box signals granted permissions");

  SitePermissions.remove(gBrowser.currentURI, "geo");
  SitePermissions.remove(gBrowser.currentURI, "camera");
  SitePermissions.remove(gBrowser.currentURI, "cookie");
});

add_task(function* testCancelPermission() {
  let {gIdentityHandler} = gBrowser.ownerGlobal;
  let tab = gBrowser.selectedTab = gBrowser.addTab();
  yield promiseTabLoadEvent(tab, PERMISSIONS_PAGE);

  let permissionsList = document.getElementById("identity-popup-permission-list");
  let emptyLabel = permissionsList.nextSibling;

  SitePermissions.set(gBrowser.currentURI, "geo", SitePermissions.ALLOW);
  SitePermissions.set(gBrowser.currentURI, "camera", SitePermissions.BLOCK);

  yield openIdentityPopup();

  ok(is_hidden(emptyLabel), "List of permissions is not empty");

  let cancelButtons = permissionsList
    .querySelectorAll(".identity-popup-permission-remove-button");

  cancelButtons[0].click();
  let labels = permissionsList.querySelectorAll(".identity-popup-permission-label");
  is(labels.length, 1, "One permission should be removed");
  cancelButtons[1].click();
  labels = permissionsList.querySelectorAll(".identity-popup-permission-label");
  is(labels.length, 0, "One permission should be removed");

  yield closeIdentityPopup();
});

add_task(function* testPermissionHints() {
  let {gIdentityHandler} = gBrowser.ownerGlobal;
  let tab = gBrowser.selectedTab = gBrowser.addTab();
  yield promiseTabLoadEvent(tab, PERMISSIONS_PAGE);

  let permissionsList = document.getElementById("identity-popup-permission-list");
  let emptyHint = document.getElementById("identity-popup-permission-empty-hint");
  let reloadHint = document.getElementById("identity-popup-permission-reload-hint");

  yield openIdentityPopup();

  ok(!is_hidden(emptyHint), "Empty hint is visible");
  ok(is_hidden(reloadHint), "Reload hint is hidden");

  yield closeIdentityPopup();

  SitePermissions.set(gBrowser.currentURI, "geo", SitePermissions.ALLOW);
  SitePermissions.set(gBrowser.currentURI, "camera", SitePermissions.BLOCK);

  yield openIdentityPopup();

  ok(is_hidden(emptyHint), "Empty hint is hidden");
  ok(is_hidden(reloadHint), "Reload hint is hidden");

  let cancelButtons = permissionsList
    .querySelectorAll(".identity-popup-permission-remove-button");
  SitePermissions.remove(gBrowser.currentURI, "camera");

  cancelButtons[0].click();
  ok(is_hidden(emptyHint), "Empty hint is hidden");
  ok(!is_hidden(reloadHint), "Reload hint is visible");

  cancelButtons[1].click();
  ok(is_hidden(emptyHint), "Empty hint is hidden");
  ok(!is_hidden(reloadHint), "Reload hint is visible");

  yield closeIdentityPopup();
  yield promiseTabLoadEvent(tab, PERMISSIONS_PAGE);
  yield openIdentityPopup();

  ok(!is_hidden(emptyHint), "Empty hint is visible after reloading");
  ok(is_hidden(reloadHint), "Reload hint is hidden after reloading");

  yield closeIdentityPopup();
});

add_task(function* testPermissionIcons() {
  let {gIdentityHandler} = gBrowser.ownerGlobal;
  let tab = gBrowser.selectedTab = gBrowser.addTab();
  yield promiseTabLoadEvent(tab, PERMISSIONS_PAGE);

  SitePermissions.set(gBrowser.currentURI, "camera", SitePermissions.ALLOW);
  SitePermissions.set(gBrowser.currentURI, "geo", SitePermissions.BLOCK);
  SitePermissions.set(gBrowser.currentURI, "microphone", SitePermissions.SESSION);

  let geoIcon = gIdentityHandler._identityBox.querySelector("[data-permission-id='geo']");
  ok(geoIcon.hasAttribute("showing"), "blocked permission icon is shown");

  let cameraIcon = gIdentityHandler._identityBox.querySelector("[data-permission-id='camera']");
  ok(!cameraIcon.hasAttribute("showing"),
    "allowed permission icon is not shown");

  let microphoneIcon  = gIdentityHandler._identityBox.querySelector("[data-permission-id='microphone']");
  ok(!microphoneIcon.hasAttribute("showing"),
    "allowed permission icon is not shown");

  SitePermissions.remove(gBrowser.currentURI, "geo");

  ok(!geoIcon.hasAttribute("showing"),
    "blocked permission icon is not shown after reset");
});
