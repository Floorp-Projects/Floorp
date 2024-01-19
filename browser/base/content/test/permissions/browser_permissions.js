/*
 * Test the Permissions section in the Control Center.
 */

const PERMISSIONS_PAGE =
  getRootDirectory(gTestPath).replace(
    "chrome://mochitests/content",
    "https://example.com"
  ) + "permissions.html";

function testPermListHasEntries(expectEntries) {
  let permissionsList = document.getElementById(
    "permission-popup-permission-list"
  );
  let listEntryCount = permissionsList.querySelectorAll(
    ".permission-popup-permission-item"
  ).length;
  if (expectEntries) {
    ok(listEntryCount, "List of permissions is not empty");
    return;
  }
  ok(!listEntryCount, "List of permissions is empty");
}

add_task(async function testMainViewVisible() {
  await BrowserTestUtils.withNewTab(PERMISSIONS_PAGE, async function () {
    await openPermissionPopup();

    let permissionsList = document.getElementById(
      "permission-popup-permission-list"
    );
    testPermListHasEntries(false);

    await closePermissionPopup();

    PermissionTestUtils.add(
      gBrowser.currentURI,
      "camera",
      Services.perms.ALLOW_ACTION
    );

    await openPermissionPopup();

    testPermListHasEntries(true);

    let labelText = SitePermissions.getPermissionLabel("camera");
    let labels = permissionsList.querySelectorAll(
      ".permission-popup-permission-label"
    );
    is(labels.length, 1, "One permission visible in main view");
    is(labels[0].innerHTML, labelText, "Correct value");

    let img = permissionsList.querySelector(
      "image.permission-popup-permission-icon"
    );
    ok(img, "There is an image for the permissions");
    ok(img.classList.contains("camera-icon"), "proper class is in image class");

    await closePermissionPopup();

    PermissionTestUtils.remove(gBrowser.currentURI, "camera");

    // We intentionally turn off a11y_checks, because the following function
    // is expected to click a toolbar button that may be already hidden
    // with "display:none;". The permissions panel anchor is hidden because
    // the last permission was removed, however we force opening the panel
    // anyways in order to test that the list has been properly emptied:
    AccessibilityUtils.setEnv({
      mustHaveAccessibleRule: false,
    });
    await openPermissionPopup();
    AccessibilityUtils.resetEnv();

    testPermListHasEntries(false);

    await closePermissionPopup();
  });
});

add_task(async function testIdentityIcon() {
  await BrowserTestUtils.withNewTab(PERMISSIONS_PAGE, function () {
    PermissionTestUtils.add(
      gBrowser.currentURI,
      "geo",
      Services.perms.ALLOW_ACTION
    );

    ok(
      gPermissionPanel._identityPermissionBox.hasAttribute("hasPermissions"),
      "identity-box signals granted permissions"
    );

    PermissionTestUtils.remove(gBrowser.currentURI, "geo");

    ok(
      !gPermissionPanel._identityPermissionBox.hasAttribute("hasPermissions"),
      "identity-box doesn't signal granted permissions"
    );

    PermissionTestUtils.add(
      gBrowser.currentURI,
      "not-a-site-permission",
      Services.perms.ALLOW_ACTION
    );

    ok(
      !gPermissionPanel._identityPermissionBox.hasAttribute("hasPermissions"),
      "identity-box doesn't signal granted permissions"
    );

    PermissionTestUtils.add(
      gBrowser.currentURI,
      "cookie",
      Ci.nsICookiePermission.ACCESS_SESSION
    );

    ok(
      gPermissionPanel._identityPermissionBox.hasAttribute("hasPermissions"),
      "identity-box signals granted permissions"
    );

    PermissionTestUtils.remove(gBrowser.currentURI, "cookie");

    PermissionTestUtils.add(
      gBrowser.currentURI,
      "cookie",
      Ci.nsICookiePermission.ACCESS_DENY
    );

    ok(
      gPermissionPanel._identityPermissionBox.hasAttribute("hasPermissions"),
      "identity-box signals granted permissions"
    );

    PermissionTestUtils.add(
      gBrowser.currentURI,
      "cookie",
      Ci.nsICookiePermission.ACCESS_DEFAULT
    );

    ok(
      !gPermissionPanel._identityPermissionBox.hasAttribute("hasPermissions"),
      "identity-box doesn't signal granted permissions"
    );

    PermissionTestUtils.remove(gBrowser.currentURI, "geo");
    PermissionTestUtils.remove(gBrowser.currentURI, "not-a-site-permission");
    PermissionTestUtils.remove(gBrowser.currentURI, "cookie");
  });
});

add_task(async function testCancelPermission() {
  await BrowserTestUtils.withNewTab(PERMISSIONS_PAGE, async function () {
    let permissionsList = document.getElementById(
      "permission-popup-permission-list"
    );

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

    await openPermissionPopup();

    testPermListHasEntries(true);

    permissionsList
      .querySelector(".permission-popup-permission-remove-button")
      .click();

    is(
      permissionsList.querySelectorAll(".permission-popup-permission-label")
        .length,
      1,
      "First permission should be removed"
    );

    permissionsList
      .querySelector(".permission-popup-permission-remove-button")
      .click();

    is(
      permissionsList.querySelectorAll(".permission-popup-permission-label")
        .length,
      0,
      "Second permission should be removed"
    );

    await closePermissionPopup();
  });
});

add_task(async function testPermissionHints() {
  await BrowserTestUtils.withNewTab(PERMISSIONS_PAGE, async function (browser) {
    let permissionsList = document.getElementById(
      "permission-popup-permission-list"
    );
    let reloadHint = document.getElementById(
      "permission-popup-permission-reload-hint"
    );

    await openPermissionPopup();

    ok(BrowserTestUtils.isHidden(reloadHint), "Reload hint is hidden");

    await closePermissionPopup();

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

    await openPermissionPopup();

    ok(BrowserTestUtils.isHidden(reloadHint), "Reload hint is hidden");

    let cancelButtons = permissionsList.querySelectorAll(
      ".permission-popup-permission-remove-button"
    );
    PermissionTestUtils.remove(gBrowser.currentURI, "camera");

    cancelButtons[0].click();
    ok(!BrowserTestUtils.isHidden(reloadHint), "Reload hint is visible");

    cancelButtons[1].click();
    ok(!BrowserTestUtils.isHidden(reloadHint), "Reload hint is visible");

    await closePermissionPopup();
    let loaded = BrowserTestUtils.browserLoaded(browser);
    BrowserTestUtils.startLoadingURIString(browser, PERMISSIONS_PAGE);
    await loaded;
    await openPermissionPopup();

    ok(
      BrowserTestUtils.isHidden(reloadHint),
      "Reload hint is hidden after reloading"
    );

    await closePermissionPopup();
  });
});

add_task(async function testPermissionIcons() {
  await BrowserTestUtils.withNewTab(PERMISSIONS_PAGE, function () {
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

    let geoIcon = gPermissionPanel._identityPermissionBox.querySelector(
      ".blocked-permission-icon[data-permission-id='geo']"
    );
    ok(geoIcon.hasAttribute("showing"), "blocked permission icon is shown");

    let cameraIcon = gPermissionPanel._identityPermissionBox.querySelector(
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
  await BrowserTestUtils.withNewTab(PERMISSIONS_PAGE, async function (browser) {
    browser.focus();

    await new Promise(r => {
      SpecialPowers.pushPrefEnv(
        { set: [["permissions.default.shortcuts", 0]] },
        r
      );
    });

    async function tryKey(desc, expectedValue) {
      await EventUtils.synthesizeAndWaitKey("c", { accelKey: true });
      let result = await SpecialPowers.spawn(browser, [], function () {
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
      is(
        result.keypresses,
        0,
        "keypress event shouldn't be fired for shortcut key, " + desc
      );
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
  await BrowserTestUtils.withNewTab(PERMISSIONS_PAGE, async function () {
    await SpecialPowers.pushPrefEnv({
      set: [["dom.disable_open_during_load", true]],
    });

    let permissionsList = document.getElementById(
      "permission-popup-permission-list"
    );
    PermissionTestUtils.add(
      gBrowser.currentURI,
      "popup",
      Services.perms.ALLOW_ACTION,
      Services.perms.EXPIRE_POLICY
    );

    await openPermissionPopup();

    // Check if the icon, nameLabel and stateLabel are visible.
    let img, labelText, labels;

    img = permissionsList.querySelector(
      "image.permission-popup-permission-icon"
    );
    ok(img, "There is an image for the popup permission");
    ok(img.classList.contains("popup-icon"), "proper class is in image class");

    labelText = SitePermissions.getPermissionLabel("popup");
    labels = permissionsList.querySelectorAll(
      ".permission-popup-permission-label"
    );
    is(labels.length, 1, "One permission visible in main view");
    is(labels[0].innerHTML, labelText, "Correct name label value");

    labelText = SitePermissions.getCurrentStateLabel(
      SitePermissions.ALLOW,
      SitePermissions.SCOPE_POLICY
    );
    labels = permissionsList.querySelectorAll(
      ".permission-popup-permission-state-label"
    );
    is(labels[0].innerHTML, labelText, "Correct state label value");

    // Check if the menulist and the remove button are hidden.
    // The menulist is specific to the "popup" permission.
    let menulist = document.getElementById("permission-popup-menulist");
    ok(menulist == null, "The popup permission menulist is not visible");

    let removeButton = permissionsList.querySelector(
      ".permission-popup-permission-remove-button"
    );
    ok(removeButton == null, "The permission remove button is not visible");

    Services.perms.removeAll();
    await closePermissionPopup();
  });
});

add_task(async function testHiddenAfterRefresh() {
  await BrowserTestUtils.withNewTab(PERMISSIONS_PAGE, async function (browser) {
    ok(
      BrowserTestUtils.isHidden(gPermissionPanel._permissionPopup),
      "Popup is hidden"
    );

    await openPermissionPopup();

    ok(
      !BrowserTestUtils.isHidden(gPermissionPanel._permissionPopup),
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
      BrowserTestUtils.isHidden(gPermissionPanel._permissionPopup),
      "Popup is hidden"
    );
  });
});

async function helper3rdPartyStoragePermissionTest(permissionID) {
  // 3rdPartyStorage permissions are listed under an anchor container - test
  // that this works correctly, i.e. the permission items are added to the
  // anchor when relevant, and other permission items are added to the default
  // anchor, and adding/removing permissions preserves this behavior correctly.

  await BrowserTestUtils.withNewTab(PERMISSIONS_PAGE, async function (browser) {
    await openPermissionPopup();

    let permissionsList = document.getElementById(
      "permission-popup-permission-list"
    );
    let storagePermissionAnchor = permissionsList.querySelector(
      `.permission-popup-permission-list-anchor[anchorfor="3rdPartyStorage"]`
    );

    testPermListHasEntries(false);

    ok(
      BrowserTestUtils.isHidden(storagePermissionAnchor.firstElementChild),
      "Anchor header is hidden"
    );

    await closePermissionPopup();

    let storagePermissionID = `${permissionID}^https://example2.com`;
    PermissionTestUtils.add(
      browser.currentURI,
      storagePermissionID,
      Services.perms.ALLOW_ACTION
    );

    await openPermissionPopup();

    testPermListHasEntries(true);
    ok(
      BrowserTestUtils.isVisible(storagePermissionAnchor.firstElementChild),
      "Anchor header is visible"
    );

    let labelText = SitePermissions.getPermissionLabel(storagePermissionID);
    let labels = storagePermissionAnchor.querySelectorAll(
      ".permission-popup-permission-label"
    );
    is(labels.length, 1, "One permission visible in 3rdPartyStorage anchor");
    is(
      labels[0].getAttribute("value"),
      labelText,
      "Permission label has the correct value"
    );

    await closePermissionPopup();

    PermissionTestUtils.add(
      browser.currentURI,
      "camera",
      Services.perms.ALLOW_ACTION
    );

    await openPermissionPopup();

    testPermListHasEntries(true);
    ok(
      BrowserTestUtils.isVisible(storagePermissionAnchor.firstElementChild),
      "Anchor header is visible"
    );

    labels = permissionsList.querySelectorAll(
      ".permission-popup-permission-label"
    );
    is(labels.length, 2, "Two permissions visible in main view");
    labels = storagePermissionAnchor.querySelectorAll(
      ".permission-popup-permission-label"
    );
    is(labels.length, 1, "One permission visible in 3rdPartyStorage anchor");

    storagePermissionAnchor
      .querySelector(".permission-popup-permission-remove-button")
      .click();
    is(
      storagePermissionAnchor.querySelectorAll(
        ".permission-popup-permission-label"
      ).length,
      0,
      "Permission item should be removed"
    );
    is(
      PermissionTestUtils.testPermission(
        browser.currentURI,
        storagePermissionID
      ),
      SitePermissions.UNKNOWN,
      "Permission removed from permission manager"
    );

    await closePermissionPopup();

    await openPermissionPopup();

    testPermListHasEntries(true);
    ok(
      BrowserTestUtils.isHidden(storagePermissionAnchor.firstElementChild),
      "Anchor header is hidden"
    );

    labels = permissionsList.querySelectorAll(
      ".permission-popup-permission-label"
    );
    is(labels.length, 1, "One permission visible in main view");

    await closePermissionPopup();

    PermissionTestUtils.remove(browser.currentURI, "camera");

    await openPermissionPopup();

    testPermListHasEntries(false);
    ok(
      BrowserTestUtils.isHidden(storagePermissionAnchor.firstElementChild),
      "Anchor header is hidden"
    );

    await closePermissionPopup();
  });
}

add_task(async function test3rdPartyStoragePermission() {
  await helper3rdPartyStoragePermissionTest("3rdPartyStorage");
});

add_task(async function test3rdPartyFrameStoragePermission() {
  await helper3rdPartyStoragePermissionTest("3rdPartyFrameStorage");
});

add_task(async function test3rdPartyBothStoragePermission() {
  // Test the handling of both types of 3rdParty(Frame)?Storage permissions together

  await BrowserTestUtils.withNewTab(PERMISSIONS_PAGE, async function (browser) {
    await openPermissionPopup();

    let permissionsList = document.getElementById(
      "permission-popup-permission-list"
    );
    let storagePermissionAnchor = permissionsList.querySelector(
      `.permission-popup-permission-list-anchor[anchorfor="3rdPartyStorage"]`
    );

    testPermListHasEntries(false);

    ok(
      BrowserTestUtils.isHidden(storagePermissionAnchor.firstElementChild),
      "Anchor header is hidden"
    );

    await closePermissionPopup();

    let storagePermissionID = "3rdPartyFrameStorage^https://example2.com";
    PermissionTestUtils.add(
      browser.currentURI,
      storagePermissionID,
      Services.perms.ALLOW_ACTION
    );

    await openPermissionPopup();

    testPermListHasEntries(true);
    ok(
      BrowserTestUtils.isVisible(storagePermissionAnchor.firstElementChild),
      "Anchor header is visible"
    );

    let labelText = SitePermissions.getPermissionLabel(storagePermissionID);
    let labels = storagePermissionAnchor.querySelectorAll(
      ".permission-popup-permission-label"
    );
    is(labels.length, 1, "One permission visible in 3rdPartyStorage anchor");
    is(
      labels[0].getAttribute("value"),
      labelText,
      "Permission label has the correct value"
    );

    await closePermissionPopup();

    PermissionTestUtils.add(
      browser.currentURI,
      "3rdPartyStorage^https://www.example2.com",
      Services.perms.ALLOW_ACTION
    );

    await openPermissionPopup();

    testPermListHasEntries(true);
    ok(
      BrowserTestUtils.isVisible(storagePermissionAnchor.firstElementChild),
      "Anchor header is visible"
    );

    labels = permissionsList.querySelectorAll(
      ".permission-popup-permission-label"
    );
    is(labels.length, 1, "One permissions visible in main view");
    labels = storagePermissionAnchor.querySelectorAll(
      ".permission-popup-permission-label"
    );
    is(labels.length, 1, "One permission visible in 3rdPartyStorage anchor");

    storagePermissionAnchor
      .querySelector(".permission-popup-permission-remove-button")
      .click();
    is(
      storagePermissionAnchor.querySelectorAll(
        ".permission-popup-permission-label"
      ).length,
      0,
      "Permission item should be removed"
    );
    is(
      PermissionTestUtils.testPermission(
        browser.currentURI,
        storagePermissionID
      ),
      SitePermissions.UNKNOWN,
      "Permission removed from permission manager"
    );
    is(
      PermissionTestUtils.testPermission(
        browser.currentURI,
        "3rdPartyStorage^https://www.example2.com"
      ),
      SitePermissions.UNKNOWN,
      "3rdPartyStorage permission removed from permission manager"
    );

    await closePermissionPopup();
  });
});
