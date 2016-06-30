/*
 * Test the Permissions section in the Control Center.
 */

var {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;
const PERMISSIONS_PAGE = "http://example.com/browser/browser/base/content/test/general/permissions.html";
var {SitePermissions} = Cu.import("resource:///modules/SitePermissions.jsm", {});

registerCleanupFunction(function() {
  SitePermissions.remove(gBrowser.currentURI, "install");
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

  gIdentityHandler.setPermission("install", SitePermissions.ALLOW);

  gIdentityHandler._identityBox.click();
  ok(is_hidden(emptyLabel), "List of permissions is not empty");

  let labelText = SitePermissions.getPermissionLabel("install");
  let labels = permissionsList.querySelectorAll(".identity-popup-permission-label");
  is(labels.length, 1, "One permission visible in main view");
  is(labels[0].textContent, labelText, "Correct value");

  let menulists = permissionsList.querySelectorAll("menulist");
  is(menulists.length, 1, "One permission visible in main view");
  is(menulists[0].id, "identity-popup-permission:install", "Install permission visible");
  is(menulists[0].value, "1", "Correct value on install menulist");
  gIdentityHandler._identityPopup.hidden = true;

  let img = menulists[0].parentNode.querySelector("image");
  ok(img, "There is an image for the permissions");
  ok(img.classList.contains("install-icon"), "proper class is in image class");

  gIdentityHandler.setPermission("install", SitePermissions.getDefault("install"));

  gIdentityHandler._identityBox.click();
  ok(!is_hidden(emptyLabel), "List of permissions is empty");
  gIdentityHandler._identityPopup.hidden = true;
});

add_task(function* testIdentityIcon() {
  let {gIdentityHandler} = gBrowser.ownerGlobal;
  let tab = gBrowser.selectedTab = gBrowser.addTab();
  yield promiseTabLoadEvent(tab, PERMISSIONS_PAGE);

  gIdentityHandler.setPermission("geo", SitePermissions.ALLOW);

  ok(gIdentityHandler._identityBox.classList.contains("grantedPermissions"),
    "identity-box signals granted permissions");

  gIdentityHandler.setPermission("geo", SitePermissions.getDefault("geo"));

  ok(!gIdentityHandler._identityBox.classList.contains("grantedPermissions"),
    "identity-box doesn't signal granted permissions");

  gIdentityHandler.setPermission("camera", SitePermissions.BLOCK);

  ok(!gIdentityHandler._identityBox.classList.contains("grantedPermissions"),
    "identity-box doesn't signal granted permissions");

  gIdentityHandler.setPermission("cookie", SitePermissions.SESSION);

  ok(gIdentityHandler._identityBox.classList.contains("grantedPermissions"),
    "identity-box signals granted permissions");
});

add_task(function* testPermissionIcons() {
  let {gIdentityHandler} = gBrowser.ownerGlobal;
  let tab = gBrowser.selectedTab = gBrowser.addTab();
  yield promiseTabLoadEvent(tab, PERMISSIONS_PAGE);

  gIdentityHandler.setPermission("camera", SitePermissions.ALLOW);
  gIdentityHandler.setPermission("geo", SitePermissions.BLOCK);
  gIdentityHandler.setPermission("microphone", SitePermissions.SESSION);

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

  gIdentityHandler.setPermission("geo", SitePermissions.getDefault("geo"));

  ok(!geoIcon.hasAttribute("showing"),
    "blocked permission icon is not shown after reset");
});
