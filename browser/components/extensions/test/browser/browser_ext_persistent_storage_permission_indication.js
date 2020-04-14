/* -- Mode: indent-tabs-mode: nil; js-indent-level: 2 -- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

const { PermissionTestUtils } = ChromeUtils.import(
  "resource://testing-common/PermissionTestUtils.jsm"
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

async function testIdentityPopup({ expectPermissionHidden }) {
  await openIdentityPopup();

  let permissionsGrantedIcon = document.getElementById(
    "permissions-granted-icon"
  );

  let permissionsList = document.getElementById(
    "identity-popup-permission-list"
  );

  if (expectPermissionHidden) {
    ok(
      BrowserTestUtils.is_hidden(permissionsGrantedIcon),
      "Permission Granted Icon is hidden"
    );

    is(
      permissionsList.querySelectorAll(
        ".identity-popup-permission-label-persistent-storage"
      ).length,
      0,
      "Persistent storage Permission should be hidden"
    );
  } else {
    ok(
      BrowserTestUtils.is_visible(permissionsGrantedIcon),
      "Permission Granted Icon is visible"
    );
  }

  await closeIdentityPopup();
}

add_task(async function testPersistentStoragePermissionHidden() {
  let extension = ExtensionTestUtils.loadExtension({
    background() {
      browser.test.sendMessage("url", browser.extension.getURL("icon.png"));
    },
    manifest: {
      name: "Test Extension",
      permissions: ["unlimitedStorage"],
    },
    files: {
      "icon.png": "",
    },
  });

  await extension.startup();

  let url = await extension.awaitMessage("url");
  await BrowserTestUtils.withNewTab({ gBrowser, url }, async function() {
    await testIdentityPopup({ expectPermissionHidden: true });
  });

  await extension.unload();
});

add_task(async function testPersistentStoragePermissionVisible() {
  let extension = ExtensionTestUtils.loadExtension({
    background() {
      browser.test.sendMessage("url", browser.extension.getURL("icon.png"));
    },
    manifest: {
      name: "Test Extension",
    },
    files: {
      "icon.png": "",
    },
  });

  await extension.startup();

  let url = await extension.awaitMessage("url");

  let policy = WebExtensionPolicy.getByID(extension.id);
  let principal = policy.extension.principal;
  PermissionTestUtils.add(
    principal,
    "persistent-storage",
    Services.perms.ALLOW_ACTION
  );

  await BrowserTestUtils.withNewTab({ gBrowser, url }, async function() {
    await testIdentityPopup({ expectPermissionHidden: false });
  });

  await extension.unload();
});
