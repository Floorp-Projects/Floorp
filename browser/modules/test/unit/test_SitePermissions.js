/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */
"use strict";

Components.utils.import("resource:///modules/SitePermissions.jsm");
Components.utils.import("resource://gre/modules/Services.jsm");

const STORAGE_MANAGER_ENABLED = Services.prefs.getBoolPref("browser.storageManager.enabled");

add_task(async function testPermissionsListing() {
  let expectedPermissions = ["camera", "cookie", "desktop-notification", "focus-tab-by-prompt",
     "geo", "image", "indexedDB", "install", "microphone", "popup", "screen"];
  if (STORAGE_MANAGER_ENABLED) {
    // The persistent-storage permission is still only pref-on on Nightly
    // so we add it only when it's pref-on.
    // Should remove this checking and add it as default after it is fully pref-on.
    expectedPermissions.push("persistent-storage");
  }
  Assert.deepEqual(SitePermissions.listPermissions().sort(), expectedPermissions.sort(),
    "Correct list of all permissions");
});

add_task(async function testGetAllByURI() {
  // check that it returns an empty array on an invalid URI
  // like a file URI, which doesn't support site permissions
  let wrongURI = Services.io.newURI("file:///example.js")
  Assert.deepEqual(SitePermissions.getAllByURI(wrongURI), []);

  let uri = Services.io.newURI("https://example.com")
  Assert.deepEqual(SitePermissions.getAllByURI(uri), []);

  SitePermissions.set(uri, "camera", SitePermissions.ALLOW);
  Assert.deepEqual(SitePermissions.getAllByURI(uri), [
      { id: "camera", state: SitePermissions.ALLOW, scope: SitePermissions.SCOPE_PERSISTENT }
  ]);

  SitePermissions.set(uri, "microphone", SitePermissions.ALLOW, SitePermissions.SCOPE_SESSION);
  SitePermissions.set(uri, "desktop-notification", SitePermissions.BLOCK);

  Assert.deepEqual(SitePermissions.getAllByURI(uri), [
      { id: "camera", state: SitePermissions.ALLOW, scope: SitePermissions.SCOPE_PERSISTENT },
      { id: "microphone", state: SitePermissions.ALLOW, scope: SitePermissions.SCOPE_SESSION },
      { id: "desktop-notification", state: SitePermissions.BLOCK, scope: SitePermissions.SCOPE_PERSISTENT }
  ]);

  SitePermissions.remove(uri, "microphone");
  Assert.deepEqual(SitePermissions.getAllByURI(uri), [
      { id: "camera", state: SitePermissions.ALLOW, scope: SitePermissions.SCOPE_PERSISTENT },
      { id: "desktop-notification", state: SitePermissions.BLOCK, scope: SitePermissions.SCOPE_PERSISTENT }
  ]);

  SitePermissions.remove(uri, "camera");
  SitePermissions.remove(uri, "desktop-notification");
  Assert.deepEqual(SitePermissions.getAllByURI(uri), []);

  // XXX Bug 1303108 - Control Center should only show non-default permissions
  SitePermissions.set(uri, "addon", SitePermissions.BLOCK);
  Assert.deepEqual(SitePermissions.getAllByURI(uri), []);
  SitePermissions.remove(uri, "addon");
});

add_task(async function testGetAvailableStates() {
  Assert.deepEqual(SitePermissions.getAvailableStates("camera"),
                   [ SitePermissions.UNKNOWN,
                     SitePermissions.ALLOW,
                     SitePermissions.BLOCK ]);

  Assert.deepEqual(SitePermissions.getAvailableStates("cookie"),
                   [ SitePermissions.ALLOW,
                     SitePermissions.ALLOW_COOKIES_FOR_SESSION,
                     SitePermissions.BLOCK ]);

  Assert.deepEqual(SitePermissions.getAvailableStates("popup"),
                   [ SitePermissions.ALLOW,
                     SitePermissions.BLOCK ]);
});

add_task(async function testExactHostMatch() {
  let uri = Services.io.newURI("https://example.com");
  let subUri = Services.io.newURI("https://test1.example.com");

  let exactHostMatched = ["desktop-notification", "focus-tab-by-prompt", "camera",
                          "microphone", "screen", "geo"];
  if (STORAGE_MANAGER_ENABLED) {
    // The persistent-storage permission is still only pref-on on Nightly
    // so we add it only when it's pref-on.
    // Should remove this checking and add it as default after it is fully pref-on.
    exactHostMatched.push("persistent-storage");
  }
  let nonExactHostMatched = ["image", "cookie", "popup", "install", "indexedDB"];

  let permissions = SitePermissions.listPermissions();
  for (let permission of permissions) {
    SitePermissions.set(uri, permission, SitePermissions.ALLOW);

    if (exactHostMatched.includes(permission)) {
      // Check that the sub-origin does not inherit the permission from its parent.
      Assert.equal(SitePermissions.get(subUri, permission).state, SitePermissions.UNKNOWN);
    } else if (nonExactHostMatched.includes(permission)) {
      // Check that the sub-origin does inherit the permission from its parent.
      Assert.equal(SitePermissions.get(subUri, permission).state, SitePermissions.ALLOW);
    } else {
      Assert.ok(false, `Found an unknown permission ${permission} in exact host match test.` +
                       "Please add new permissions from SitePermissions.jsm to this test.");
    }

    // Check that the permission can be made specific to the sub-origin.
    SitePermissions.set(subUri, permission, SitePermissions.BLOCK);
    Assert.equal(SitePermissions.get(subUri, permission).state, SitePermissions.BLOCK);
    Assert.equal(SitePermissions.get(uri, permission).state, SitePermissions.ALLOW);

    SitePermissions.remove(subUri, permission);
    SitePermissions.remove(uri, permission);
  }
});
