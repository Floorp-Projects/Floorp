/*
 * Test the Permissions section in the Control Center.
 */

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;
const PERMISSIONS_PAGE = "http://example.com/browser/browser/base/content/test/general/permissions.html";
let {SitePermissions} = Cu.import("resource:///modules/SitePermissions.jsm", {});

registerCleanupFunction(function() {
  SitePermissions.remove(gBrowser.currentURI, "install");
  while (gBrowser.tabs.length > 1) {
    gBrowser.removeCurrentTab();
  }
});

add_task(function* testMainViewVisible() {
  let {gIdentityHandler} = gBrowser.ownerGlobal;
  let tab = gBrowser.selectedTab = gBrowser.addTab();
  yield promiseTabLoadEvent(tab, PERMISSIONS_PAGE);

  gIdentityHandler._identityBox.click();
  ok(is_hidden(gIdentityHandler._permissionsContainer), "The container is hidden");
  gIdentityHandler._identityPopup.hidden = true;

  gIdentityHandler.setPermission("install", 1);

  gIdentityHandler._identityBox.click();
  ok(!is_hidden(gIdentityHandler._permissionsContainer), "The container is visible");
  let menulists = gIdentityHandler._permissionsContainer.querySelectorAll("menulist");
  is(menulists.length, 1, "One permission visible in main view");
  is(menulists[0].id, "identity-popup-permission:install", "Install permission visible");
  is(menulists[0].value, "1", "Correct value on install menulist");
  gIdentityHandler._identityPopup.hidden = true;

  gIdentityHandler.setPermission("install", SitePermissions.getDefault("install"));

  gIdentityHandler._identityBox.click();
  ok(is_hidden(gIdentityHandler._permissionsContainer), "The container is hidden");
  gIdentityHandler._identityPopup.hidden = true;
});
