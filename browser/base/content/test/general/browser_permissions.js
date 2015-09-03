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

add_task(function* testSubviewListing() {
  let {gIdentityHandler} = gBrowser.ownerGlobal;
  gIdentityHandler.setPermission("install", 1);

  info("Opening control center and expanding permissions subview");
  gIdentityHandler._identityBox.click();

  info("Checking 'Page Functionality' permissions");
  let pageFunctionalityMenulists = gIdentityHandler._permissionSubviewListPageFunctionality.querySelectorAll("menulist");
  let pageFunctionalityPerms = SitePermissions.listPageFunctionalityPermissions();
  is(pageFunctionalityMenulists.length, pageFunctionalityPerms.length, "One menulist for each permission");

  for (let i = 0; i < pageFunctionalityMenulists.length; i++) {
    let menulist = pageFunctionalityMenulists[i];
    let perm = pageFunctionalityPerms[i];
    let expectedValue = SitePermissions.get(gBrowser.currentURI, perm);
    if (expectedValue == SitePermissions.UNKNOWN) {
      expectedValue = SitePermissions.getDefault(perm);
    }

    is(menulist.id, "identity-popup-permission:" + perm, "Correct id for menulist: " + perm);
    is(menulist.value, expectedValue, "Correct value on menulist: " + perm);
  }

  info("Checking 'System Access' permissions");
  let systemAccessMenulists = gIdentityHandler._permissionSubviewListSystemAccess.querySelectorAll("menulist");
  let systemAccessPerms = SitePermissions.listSystemAccessPermissions();
  is(systemAccessMenulists.length, systemAccessPerms.length, "One menulist for each permission");

  for (let i = 0; i < systemAccessMenulists.length; i++) {
    let menulist = systemAccessMenulists[i];
    let perm = systemAccessPerms[i];
    let expectedValue = SitePermissions.get(gBrowser.currentURI, perm);
    if (expectedValue == SitePermissions.UNKNOWN) {
      expectedValue = SitePermissions.getDefault(perm);
    }

    is(menulist.id, "identity-popup-permission:" + perm, "Correct id for menulist: " + perm);
    is(menulist.value, expectedValue, "Correct value on menulist: " + perm);
  }

  gIdentityHandler._identityPopup.hidden = true;
});
