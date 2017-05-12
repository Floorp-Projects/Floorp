/*
 * Test the Permissions section in the Control Center.
 */

const PERMISSIONS_PAGE  = getRootDirectory(gTestPath).replace("chrome://mochitests/content", "https://example.com") + "permissions.html";

function openIdentityPopup() {
  let promise = BrowserTestUtils.waitForEvent(gIdentityHandler._identityPopup, "popupshown");
  gIdentityHandler._identityBox.click();
  return promise;
}

function closeIdentityPopup() {
  let promise = BrowserTestUtils.waitForEvent(gIdentityHandler._identityPopup, "popuphidden");
  gIdentityHandler._identityPopup.hidePopup();
  return promise;
}

add_task(async function testMainViewVisible() {
  await BrowserTestUtils.withNewTab(PERMISSIONS_PAGE, async function() {
    let permissionsList = document.getElementById("identity-popup-permission-list");
    let emptyLabel = permissionsList.nextSibling.nextSibling;

    await openIdentityPopup();

    ok(!is_hidden(emptyLabel), "List of permissions is empty");

    await closeIdentityPopup();

    SitePermissions.set(gBrowser.currentURI, "camera", SitePermissions.ALLOW);

    await openIdentityPopup();

    ok(is_hidden(emptyLabel), "List of permissions is not empty");

    let labelText = SitePermissions.getPermissionLabel("camera");
    let labels = permissionsList.querySelectorAll(".identity-popup-permission-label");
    is(labels.length, 1, "One permission visible in main view");
    is(labels[0].textContent, labelText, "Correct value");

    let img = permissionsList.querySelector("image.identity-popup-permission-icon");
    ok(img, "There is an image for the permissions");
    ok(img.classList.contains("camera-icon"), "proper class is in image class");

    await closeIdentityPopup();

    SitePermissions.remove(gBrowser.currentURI, "camera");

    await openIdentityPopup();

    ok(!is_hidden(emptyLabel), "List of permissions is empty");

    await closeIdentityPopup();
  });
});

add_task(async function testIdentityIcon() {
  await BrowserTestUtils.withNewTab(PERMISSIONS_PAGE, function() {
    SitePermissions.set(gBrowser.currentURI, "geo", SitePermissions.ALLOW);

    ok(gIdentityHandler._identityBox.classList.contains("grantedPermissions"),
      "identity-box signals granted permissions");

    SitePermissions.remove(gBrowser.currentURI, "geo");

    ok(!gIdentityHandler._identityBox.classList.contains("grantedPermissions"),
      "identity-box doesn't signal granted permissions");

    SitePermissions.set(gBrowser.currentURI, "camera", SitePermissions.BLOCK);

    ok(!gIdentityHandler._identityBox.classList.contains("grantedPermissions"),
      "identity-box doesn't signal granted permissions");

    SitePermissions.set(gBrowser.currentURI, "cookie", SitePermissions.ALLOW_COOKIES_FOR_SESSION);

    ok(gIdentityHandler._identityBox.classList.contains("grantedPermissions"),
      "identity-box signals granted permissions");

    SitePermissions.remove(gBrowser.currentURI, "geo");
    SitePermissions.remove(gBrowser.currentURI, "camera");
    SitePermissions.remove(gBrowser.currentURI, "cookie");
  });
});

add_task(async function testCancelPermission() {
  await BrowserTestUtils.withNewTab(PERMISSIONS_PAGE, async function() {
    let permissionsList = document.getElementById("identity-popup-permission-list");
    let emptyLabel = permissionsList.nextSibling.nextSibling;

    SitePermissions.set(gBrowser.currentURI, "geo", SitePermissions.ALLOW);
    SitePermissions.set(gBrowser.currentURI, "camera", SitePermissions.BLOCK);

    await openIdentityPopup();

    ok(is_hidden(emptyLabel), "List of permissions is not empty");

    let cancelButtons = permissionsList
      .querySelectorAll(".identity-popup-permission-remove-button");

    cancelButtons[0].click();
    let labels = permissionsList.querySelectorAll(".identity-popup-permission-label");
    is(labels.length, 1, "One permission should be removed");
    cancelButtons[1].click();
    labels = permissionsList.querySelectorAll(".identity-popup-permission-label");
    is(labels.length, 0, "One permission should be removed");

    await closeIdentityPopup();
  });
});

add_task(async function testPermissionHints() {
  await BrowserTestUtils.withNewTab(PERMISSIONS_PAGE, async function(browser) {
    let permissionsList = document.getElementById("identity-popup-permission-list");
    let emptyHint = document.getElementById("identity-popup-permission-empty-hint");
    let reloadHint = document.getElementById("identity-popup-permission-reload-hint");

    await openIdentityPopup();

    ok(!is_hidden(emptyHint), "Empty hint is visible");
    ok(is_hidden(reloadHint), "Reload hint is hidden");

    await closeIdentityPopup();

    SitePermissions.set(gBrowser.currentURI, "geo", SitePermissions.ALLOW);
    SitePermissions.set(gBrowser.currentURI, "camera", SitePermissions.BLOCK);

    await openIdentityPopup();

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

    await closeIdentityPopup();
    let loaded = BrowserTestUtils.browserLoaded(browser);
    BrowserTestUtils.loadURI(browser, PERMISSIONS_PAGE);
    await loaded;
    await openIdentityPopup();

    ok(!is_hidden(emptyHint), "Empty hint is visible after reloading");
    ok(is_hidden(reloadHint), "Reload hint is hidden after reloading");

    await closeIdentityPopup();
  });
});

add_task(async function testPermissionIcons() {
  await BrowserTestUtils.withNewTab(PERMISSIONS_PAGE, function() {
    SitePermissions.set(gBrowser.currentURI, "camera", SitePermissions.ALLOW);
    SitePermissions.set(gBrowser.currentURI, "geo", SitePermissions.BLOCK);

    let geoIcon = gIdentityHandler._identityBox
      .querySelector(".blocked-permission-icon[data-permission-id='geo']");
    ok(geoIcon.hasAttribute("showing"), "blocked permission icon is shown");

    let cameraIcon = gIdentityHandler._identityBox
      .querySelector(".blocked-permission-icon[data-permission-id='camera']");
    ok(!cameraIcon.hasAttribute("showing"),
      "allowed permission icon is not shown");

    SitePermissions.remove(gBrowser.currentURI, "geo");

    ok(!geoIcon.hasAttribute("showing"),
      "blocked permission icon is not shown after reset");

    SitePermissions.remove(gBrowser.currentURI, "camera");
  });
});
