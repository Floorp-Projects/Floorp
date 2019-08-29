/*
 * Test the Permissions section in the Control Center.
 */

const PERMISSIONS_PAGE =
  getRootDirectory(gTestPath).replace(
    "chrome://mochitests/content",
    "https://example.com"
  ) + "permissions.html";
const kStrictKeyPressEvents = SpecialPowers.getBoolPref(
  "dom.keyboardevent.keypress.dispatch_non_printable_keys_only_system_group_in_content"
);

function openIdentityPopup() {
  let promise = BrowserTestUtils.waitForEvent(
    gIdentityHandler._identityPopup,
    "popupshown"
  );
  gIdentityHandler._identityBox.click();
  return promise;
}

function closeIdentityPopup() {
  let promise = BrowserTestUtils.waitForEvent(
    gIdentityHandler._identityPopup,
    "popuphidden"
  );
  gIdentityHandler._identityPopup.hidePopup();
  return promise;
}

add_task(async function testMainViewVisible() {
  await BrowserTestUtils.withNewTab(PERMISSIONS_PAGE, async function() {
    let permissionsList = document.getElementById(
      "identity-popup-permission-list"
    );
    let emptyLabel = permissionsList.nextElementSibling.nextElementSibling;

    await openIdentityPopup();

    ok(!BrowserTestUtils.is_hidden(emptyLabel), "List of permissions is empty");

    await closeIdentityPopup();

    PermissionTestUtils.add(
      gBrowser.currentURI,
      "camera",
      Services.perms.ALLOW_ACTION
    );

    await openIdentityPopup();

    ok(
      BrowserTestUtils.is_hidden(emptyLabel),
      "List of permissions is not empty"
    );

    let labelText = SitePermissions.getPermissionLabel("camera");
    let labels = permissionsList.querySelectorAll(
      ".identity-popup-permission-label"
    );
    is(labels.length, 1, "One permission visible in main view");
    is(labels[0].textContent, labelText, "Correct value");

    let img = permissionsList.querySelector(
      "image.identity-popup-permission-icon"
    );
    ok(img, "There is an image for the permissions");
    ok(img.classList.contains("camera-icon"), "proper class is in image class");

    await closeIdentityPopup();

    PermissionTestUtils.remove(gBrowser.currentURI, "camera");

    await openIdentityPopup();

    ok(!BrowserTestUtils.is_hidden(emptyLabel), "List of permissions is empty");

    await closeIdentityPopup();
  });
});

add_task(async function testIdentityIcon() {
  await BrowserTestUtils.withNewTab(PERMISSIONS_PAGE, function() {
    PermissionTestUtils.add(
      gBrowser.currentURI,
      "geo",
      Services.perms.ALLOW_ACTION
    );

    ok(
      gIdentityHandler._identityBox.classList.contains("grantedPermissions"),
      "identity-box signals granted permissions"
    );

    PermissionTestUtils.remove(gBrowser.currentURI, "geo");

    ok(
      !gIdentityHandler._identityBox.classList.contains("grantedPermissions"),
      "identity-box doesn't signal granted permissions"
    );

    PermissionTestUtils.add(
      gBrowser.currentURI,
      "camera",
      Services.perms.DENY_ACTION
    );

    ok(
      !gIdentityHandler._identityBox.classList.contains("grantedPermissions"),
      "identity-box doesn't signal granted permissions"
    );

    PermissionTestUtils.add(
      gBrowser.currentURI,
      "cookie",
      Ci.nsICookiePermission.ACCESS_SESSION
    );

    ok(
      gIdentityHandler._identityBox.classList.contains("grantedPermissions"),
      "identity-box signals granted permissions"
    );

    PermissionTestUtils.remove(gBrowser.currentURI, "geo");
    PermissionTestUtils.remove(gBrowser.currentURI, "camera");
    PermissionTestUtils.remove(gBrowser.currentURI, "cookie");
  });
});

add_task(async function testCancelPermission() {
  await BrowserTestUtils.withNewTab(PERMISSIONS_PAGE, async function() {
    let permissionsList = document.getElementById(
      "identity-popup-permission-list"
    );
    let emptyLabel = permissionsList.nextElementSibling.nextElementSibling;

    PermissionTestUtils.add(
      gBrowser.currentURI,
      "geo",
      Services.perms.ALLOW_ACTION
    );
    PermissionTestUtils.add(
      gBrowser.currentURI,
      "camera",
      Services.perms.DENY_ACTION
    );

    await openIdentityPopup();

    ok(
      BrowserTestUtils.is_hidden(emptyLabel),
      "List of permissions is not empty"
    );

    permissionsList
      .querySelector(".identity-popup-permission-remove-button")
      .click();

    is(
      permissionsList.querySelectorAll(".identity-popup-permission-label")
        .length,
      1,
      "First permission should be removed"
    );

    permissionsList
      .querySelector(".identity-popup-permission-remove-button")
      .click();

    is(
      permissionsList.querySelectorAll(".identity-popup-permission-label")
        .length,
      0,
      "Second permission should be removed"
    );

    await closeIdentityPopup();
  });
});

add_task(async function testPermissionHints() {
  await BrowserTestUtils.withNewTab(PERMISSIONS_PAGE, async function(browser) {
    let permissionsList = document.getElementById(
      "identity-popup-permission-list"
    );
    let emptyHint = document.getElementById(
      "identity-popup-permission-empty-hint"
    );
    let reloadHint = document.getElementById(
      "identity-popup-permission-reload-hint"
    );

    await openIdentityPopup();

    ok(!BrowserTestUtils.is_hidden(emptyHint), "Empty hint is visible");
    ok(BrowserTestUtils.is_hidden(reloadHint), "Reload hint is hidden");

    await closeIdentityPopup();

    PermissionTestUtils.add(
      gBrowser.currentURI,
      "geo",
      Services.perms.ALLOW_ACTION
    );
    PermissionTestUtils.add(
      gBrowser.currentURI,
      "camera",
      Services.perms.DENY_ACTION
    );

    await openIdentityPopup();

    ok(BrowserTestUtils.is_hidden(emptyHint), "Empty hint is hidden");
    ok(BrowserTestUtils.is_hidden(reloadHint), "Reload hint is hidden");

    let cancelButtons = permissionsList.querySelectorAll(
      ".identity-popup-permission-remove-button"
    );
    PermissionTestUtils.remove(gBrowser.currentURI, "camera");

    cancelButtons[0].click();
    ok(BrowserTestUtils.is_hidden(emptyHint), "Empty hint is hidden");
    ok(!BrowserTestUtils.is_hidden(reloadHint), "Reload hint is visible");

    cancelButtons[1].click();
    ok(BrowserTestUtils.is_hidden(emptyHint), "Empty hint is hidden");
    ok(!BrowserTestUtils.is_hidden(reloadHint), "Reload hint is visible");

    await closeIdentityPopup();
    let loaded = BrowserTestUtils.browserLoaded(browser);
    BrowserTestUtils.loadURI(browser, PERMISSIONS_PAGE);
    await loaded;
    await openIdentityPopup();

    ok(
      !BrowserTestUtils.is_hidden(emptyHint),
      "Empty hint is visible after reloading"
    );
    ok(
      BrowserTestUtils.is_hidden(reloadHint),
      "Reload hint is hidden after reloading"
    );

    await closeIdentityPopup();
  });
});

add_task(async function testPermissionIcons() {
  await BrowserTestUtils.withNewTab(PERMISSIONS_PAGE, function() {
    PermissionTestUtils.add(
      gBrowser.currentURI,
      "camera",
      Services.perms.ALLOW_ACTION
    );
    PermissionTestUtils.add(
      gBrowser.currentURI,
      "geo",
      Services.perms.DENY_ACTION
    );

    let geoIcon = gIdentityHandler._identityBox.querySelector(
      ".blocked-permission-icon[data-permission-id='geo']"
    );
    ok(geoIcon.hasAttribute("showing"), "blocked permission icon is shown");

    let cameraIcon = gIdentityHandler._identityBox.querySelector(
      ".blocked-permission-icon[data-permission-id='camera']"
    );
    ok(
      !cameraIcon.hasAttribute("showing"),
      "allowed permission icon is not shown"
    );

    PermissionTestUtils.remove(gBrowser.currentURI, "geo");

    ok(
      !geoIcon.hasAttribute("showing"),
      "blocked permission icon is not shown after reset"
    );

    PermissionTestUtils.remove(gBrowser.currentURI, "camera");
  });
});

add_task(async function testPermissionShortcuts() {
  await BrowserTestUtils.withNewTab(PERMISSIONS_PAGE, async function(browser) {
    browser.focus();

    await new Promise(r => {
      SpecialPowers.pushPrefEnv(
        { set: [["permissions.default.shortcuts", 0]] },
        r
      );
    });

    async function tryKey(desc, expectedValue) {
      await EventUtils.synthesizeAndWaitKey("c", { accelKey: true });
      let result = await ContentTask.spawn(browser, null, function() {
        return {
          keydowns: content.wrappedJSObject.gKeyDowns,
          keypresses: content.wrappedJSObject.gKeyPresses,
        };
      });
      is(
        result.keydowns,
        expectedValue,
        "keydown event was fired or not fired as expected, " + desc
      );
      if (kStrictKeyPressEvents) {
        is(
          result.keypresses,
          0,
          "keypress event shouldn't be fired for shortcut key, " + desc
        );
      } else {
        is(
          result.keypresses,
          expectedValue,
          "keypress event should be fired even for shortcut key, " + desc
        );
      }
    }

    await tryKey("pressed with default permissions", 1);

    PermissionTestUtils.add(
      gBrowser.currentURI,
      "shortcuts",
      Services.perms.DENY_ACTION
    );
    await tryKey("pressed when site blocked", 1);

    PermissionTestUtils.add(
      gBrowser.currentURI,
      "shortcuts",
      PermissionTestUtils.ALLOW
    );
    await tryKey("pressed when site allowed", 2);

    PermissionTestUtils.remove(gBrowser.currentURI, "shortcuts");
    await new Promise(r => {
      SpecialPowers.pushPrefEnv(
        { set: [["permissions.default.shortcuts", 2]] },
        r
      );
    });

    await tryKey("pressed when globally blocked", 2);
    PermissionTestUtils.add(
      gBrowser.currentURI,
      "shortcuts",
      Services.perms.ALLOW_ACTION
    );
    await tryKey("pressed when globally blocked but site allowed", 3);

    PermissionTestUtils.add(
      gBrowser.currentURI,
      "shortcuts",
      Services.perms.DENY_ACTION
    );
    await tryKey("pressed when globally blocked and site blocked", 3);

    PermissionTestUtils.remove(gBrowser.currentURI, "shortcuts");
  });
});

// Test the control center UI when policy permissions are set.
add_task(async function testPolicyPermission() {
  await BrowserTestUtils.withNewTab(PERMISSIONS_PAGE, async function() {
    await SpecialPowers.pushPrefEnv({
      set: [["dom.disable_open_during_load", true]],
    });

    let permissionsList = document.getElementById(
      "identity-popup-permission-list"
    );
    PermissionTestUtils.add(
      gBrowser.currentURI,
      "popup",
      Services.perms.ALLOW_ACTION,
      Services.perms.EXPIRE_POLICY
    );

    await openIdentityPopup();

    // Check if the icon, nameLabel and stateLabel are visible.
    let img, labelText, labels;

    img = permissionsList.querySelector("image.identity-popup-permission-icon");
    ok(img, "There is an image for the popup permission");
    ok(img.classList.contains("popup-icon"), "proper class is in image class");

    labelText = SitePermissions.getPermissionLabel("popup");
    labels = permissionsList.querySelectorAll(
      ".identity-popup-permission-label"
    );
    is(labels.length, 1, "One permission visible in main view");
    is(labels[0].textContent, labelText, "Correct name label value");

    labelText = SitePermissions.getCurrentStateLabel(
      SitePermissions.ALLOW,
      SitePermissions.SCOPE_POLICY
    );
    labels = permissionsList.querySelectorAll(
      ".identity-popup-permission-state-label"
    );
    is(labels[0].textContent, labelText, "Correct state label value");

    // Check if the menulist and the remove button are hidden.
    // The menulist is specific to the "popup" permission.
    let menulist = document.getElementById("identity-popup-popup-menulist");
    ok(menulist == null, "The popup permission menulist is not visible");

    let removeButton = permissionsList.querySelector(
      ".identity-popup-permission-remove-button"
    );
    ok(removeButton == null, "The permission remove button is not visible");

    Services.perms.removeAll();
    await closeIdentityPopup();
  });
});

add_task(async function testHiddenAfterRefresh() {
  await BrowserTestUtils.withNewTab(PERMISSIONS_PAGE, async function(browser) {
    ok(
      BrowserTestUtils.is_hidden(gIdentityHandler._identityPopup),
      "Popup is hidden"
    );

    await openIdentityPopup();

    ok(
      !BrowserTestUtils.is_hidden(gIdentityHandler._identityPopup),
      "Popup is shown"
    );

    let reloaded = BrowserTestUtils.browserLoaded(
      browser,
      false,
      PERMISSIONS_PAGE
    );
    EventUtils.synthesizeKey("VK_F5", {}, browser.ownerGlobal);
    await reloaded;

    ok(
      BrowserTestUtils.is_hidden(gIdentityHandler._identityPopup),
      "Popup is hidden"
    );
  });
});
