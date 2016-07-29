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

add_task(function* testMainViewVisible() {
  let {gIdentityHandler} = gBrowser.ownerGlobal;
  let tab = gBrowser.selectedTab = gBrowser.addTab();
  yield promiseTabLoadEvent(tab, PERMISSIONS_PAGE);

  let permissionsList = document.getElementById("identity-popup-permission-list");
  let emptyLabel = permissionsList.nextSibling;

  gIdentityHandler._identityBox.click();
  ok(!is_hidden(emptyLabel), "List of permissions is empty");
  gIdentityHandler._identityPopup.hidden = true;

  SitePermissions.set(gBrowser.currentURI, "camera", SitePermissions.ALLOW);

  gIdentityHandler._identityBox.click();
  ok(is_hidden(emptyLabel), "List of permissions is not empty");

  let labelText = SitePermissions.getPermissionLabel("camera");
  let labels = permissionsList.querySelectorAll(".identity-popup-permission-label");
  is(labels.length, 1, "One permission visible in main view");
  is(labels[0].textContent, labelText, "Correct value");

  let img = permissionsList.querySelector("image.identity-popup-permission-icon");
  ok(img, "There is an image for the permissions");
  ok(img.classList.contains("camera-icon"), "proper class is in image class");

  SitePermissions.remove(gBrowser.currentURI, "camera");

  gIdentityHandler._identityBox.click();
  ok(!is_hidden(emptyLabel), "List of permissions is empty");
  gIdentityHandler._identityPopup.hidden = true;
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

  gIdentityHandler._identityBox.click();

  ok(is_hidden(emptyLabel), "List of permissions is not empty");

  let cancelButtons = permissionsList
    .querySelectorAll(".identity-popup-permission-remove-button");

  cancelButtons[0].click();
  let labels = permissionsList.querySelectorAll(".identity-popup-permission-label");
  is(labels.length, 1, "One permission should be removed");
  cancelButtons[1].click();
  labels = permissionsList.querySelectorAll(".identity-popup-permission-label");
  is(labels.length, 0, "One permission should be removed");

  gIdentityHandler._identityPopup.hidden = true;
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
  ok(geoIcon.classList.contains("blocked"),
    "blocked permission icon is shown as blocked");

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
