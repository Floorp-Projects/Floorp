/* -- Mode: indent-tabs-mode: nil; js-indent-level: 2 -- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

const { PermissionTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/PermissionTestUtils.sys.mjs"
);

function openPermissionPopup() {
  let promise = BrowserTestUtils.waitForEvent(
    window,
    "popupshown",
    true,
    event => event.target == gPermissionPanel._permissionPopup
  );
  gPermissionPanel._identityPermissionBox.click();
  info("Wait permission popup to be shown");
  return promise;
}

function closePermissionPopup() {
  let promise = BrowserTestUtils.waitForEvent(
    gPermissionPanel._permissionPopup,
    "popuphidden"
  );
  gPermissionPanel._permissionPopup.hidePopup();
  info("Wait permission popup to be hidden");
  return promise;
}

async function testPermissionPopup({ expectPermissionHidden }) {
  await openPermissionPopup();

  if (expectPermissionHidden) {
    let permissionsList = document.getElementById(
      "permission-popup-permission-list"
    );
    is(
      permissionsList.querySelectorAll(
        ".permission-popup-permission-label-persistent-storage"
      ).length,
      0,
      "Persistent storage Permission should be hidden"
    );
  }

  await closePermissionPopup();

  // We need to test this after the popup has been closed.
  // The permission icon will be shown as long as the popup is open, event if
  // no permissions are set.
  let permissionsGrantedIcon = document.getElementById(
    "permissions-granted-icon"
  );

  if (expectPermissionHidden) {
    ok(
      BrowserTestUtils.is_hidden(permissionsGrantedIcon),
      "Permission Granted Icon is hidden"
    );
  } else {
    ok(
      BrowserTestUtils.is_visible(permissionsGrantedIcon),
      "Permission Granted Icon is visible"
    );
  }
}

add_task(async function testPersistentStoragePermissionHidden() {
  let extension = ExtensionTestUtils.loadExtension({
    background() {
      browser.test.sendMessage("url", browser.runtime.getURL("testpage.html"));
    },
    manifest: {
      name: "Test Extension",
      permissions: ["unlimitedStorage"],
    },
    files: {
      "testpage.html": "<h1>Extension Test Page</h1>",
    },
  });

  await extension.startup();

  let url = await extension.awaitMessage("url");
  await BrowserTestUtils.withNewTab("about:blank", async browser => {
    // Wait the tab to be fully loade, then run the test on the permission prompt.
    let loaded = BrowserTestUtils.browserLoaded(browser, false, url);
    BrowserTestUtils.startLoadingURIString(browser, url);
    await loaded;
    await testPermissionPopup({ expectPermissionHidden: true });
  });

  await extension.unload();
});

add_task(async function testPersistentStoragePermissionVisible() {
  let extension = ExtensionTestUtils.loadExtension({
    background() {
      browser.test.sendMessage("url", browser.runtime.getURL("testpage.html"));
    },
    manifest: {
      name: "Test Extension",
    },
    files: {
      "testpage.html": "<h1>Extension Test Page</h1>",
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

  await BrowserTestUtils.withNewTab("about:blank", async browser => {
    // Wait the tab to be fully loade, then run the test on the permission prompt.
    let loaded = BrowserTestUtils.browserLoaded(browser, false, url);
    BrowserTestUtils.startLoadingURIString(browser, url);
    await loaded;
    await testPermissionPopup({ expectPermissionHidden: false });
  });

  await extension.unload();
});
