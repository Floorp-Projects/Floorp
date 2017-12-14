/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */
"use strict";

Components.utils.import("resource:///modules/SitePermissions.jsm");
Components.utils.import("resource://gre/modules/Services.jsm");

const STORAGE_MANAGER_ENABLED = Services.prefs.getBoolPref("browser.storageManager.enabled");
const RESIST_FINGERPRINTING_ENABLED = Services.prefs.getBoolPref("privacy.resistFingerprinting");

add_task(async function testPermissionsListing() {
  let expectedPermissions = ["camera", "cookie", "desktop-notification", "focus-tab-by-prompt",
     "geo", "image", "install", "microphone", "popup", "screen", "shortcuts"];
  if (STORAGE_MANAGER_ENABLED) {
    // The persistent-storage permission is still only pref-on on Nightly
    // so we add it only when it's pref-on.
    // Should remove this checking and add it as default after it is fully pref-on.
    expectedPermissions.push("persistent-storage");
  }
  if (RESIST_FINGERPRINTING_ENABLED) {
    // Canvas permission should be hidden unless privacy.resistFingerprinting
    // is true.
    expectedPermissions.push("canvas");
  }
  Assert.deepEqual(SitePermissions.listPermissions().sort(), expectedPermissions.sort(),
    "Correct list of all permissions");
});

add_task(async function testGetAllByURI() {
  // check that it returns an empty array on an invalid URI
  // like a file URI, which doesn't support site permissions
  let wrongURI = Services.io.newURI("file:///example.js");
  Assert.deepEqual(SitePermissions.getAllByURI(wrongURI), []);

  let uri = Services.io.newURI("https://example.com");
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

  Assert.equal(Services.prefs.getIntPref("permissions.default.shortcuts"), 0);
  SitePermissions.set(uri, "shortcuts", SitePermissions.BLOCK);

  // Customized preference should have been enabled, but the default should not.
  Assert.equal(Services.prefs.getIntPref("permissions.default.shortcuts"), 0);
  Assert.deepEqual(SitePermissions.getAllByURI(uri), [
      { id: "shortcuts", state: SitePermissions.BLOCK, scope: SitePermissions.SCOPE_PERSISTENT },
  ]);

  SitePermissions.remove(uri, "shortcuts");
  Services.prefs.clearUserPref("permissions.default.shortcuts");
});

add_task(async function testGetAvailableStates() {
  Assert.deepEqual(SitePermissions.getAvailableStates("camera"),
                   [ SitePermissions.UNKNOWN,
                     SitePermissions.ALLOW,
                     SitePermissions.BLOCK ]);

  // Test available states with a default permission set.
  Services.prefs.setIntPref("permissions.default.camera", SitePermissions.ALLOW);
  Assert.deepEqual(SitePermissions.getAvailableStates("camera"),
                   [ SitePermissions.PROMPT,
                     SitePermissions.ALLOW,
                     SitePermissions.BLOCK ]);
  Services.prefs.clearUserPref("permissions.default.camera");

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
  if (RESIST_FINGERPRINTING_ENABLED) {
    // Canvas permission should be hidden unless privacy.resistFingerprinting
    // is true.
    exactHostMatched.push("canvas");
  }
  let nonExactHostMatched = ["image", "cookie", "popup", "install", "shortcuts"];

  let permissions = SitePermissions.listPermissions();
  for (let permission of permissions) {
    SitePermissions.set(uri, permission, SitePermissions.ALLOW);

    if (exactHostMatched.includes(permission)) {
      // Check that the sub-origin does not inherit the permission from its parent.
      Assert.equal(SitePermissions.get(subUri, permission).state, SitePermissions.UNKNOWN,
        `${permission} should exact-host match`);
    } else if (nonExactHostMatched.includes(permission)) {
      // Check that the sub-origin does inherit the permission from its parent.
      Assert.equal(SitePermissions.get(subUri, permission).state, SitePermissions.ALLOW,
        `${permission} should not exact-host match`);
    } else {
      Assert.ok(false, `Found an unknown permission ${permission} in exact host match test.` +
                       "Please add new permissions from SitePermissions.jsm to this test.");
    }

    // Check that the permission can be made specific to the sub-origin.
    SitePermissions.set(subUri, permission, SitePermissions.PROMPT);
    Assert.equal(SitePermissions.get(subUri, permission).state, SitePermissions.PROMPT);
    Assert.equal(SitePermissions.get(uri, permission).state, SitePermissions.ALLOW);

    SitePermissions.remove(subUri, permission);
    SitePermissions.remove(uri, permission);
  }
});

add_task(function* testDefaultPrefs() {
  let uri = Services.io.newURI("https://example.com");

  // Check that without a pref the default return value is UNKNOWN.
  Assert.deepEqual(SitePermissions.get(uri, "camera"), {
    state: SitePermissions.UNKNOWN,
    scope: SitePermissions.SCOPE_PERSISTENT,
  });

  // Check that the default return value changed after setting the pref.
  Services.prefs.setIntPref("permissions.default.camera", SitePermissions.BLOCK);
  Assert.deepEqual(SitePermissions.get(uri, "camera"), {
    state: SitePermissions.BLOCK,
    scope: SitePermissions.SCOPE_PERSISTENT,
  });

  // Check that other permissions still return UNKNOWN.
  Assert.deepEqual(SitePermissions.get(uri, "microphone"), {
    state: SitePermissions.UNKNOWN,
    scope: SitePermissions.SCOPE_PERSISTENT,
  });

  // Check that the default return value changed after changing the pref.
  Services.prefs.setIntPref("permissions.default.camera", SitePermissions.ALLOW);
  Assert.deepEqual(SitePermissions.get(uri, "camera"), {
    state: SitePermissions.ALLOW,
    scope: SitePermissions.SCOPE_PERSISTENT,
  });

  // Check that the preference is ignored if there is a value.
  SitePermissions.set(uri, "camera", SitePermissions.BLOCK);
  Assert.deepEqual(SitePermissions.get(uri, "camera"), {
    state: SitePermissions.BLOCK,
    scope: SitePermissions.SCOPE_PERSISTENT,
  });

  // The preference should be honored again, after resetting the permissions.
  SitePermissions.remove(uri, "camera");
  Assert.deepEqual(SitePermissions.get(uri, "camera"), {
    state: SitePermissions.ALLOW,
    scope: SitePermissions.SCOPE_PERSISTENT,
  });

  // Should be UNKNOWN after clearing the pref.
  Services.prefs.clearUserPref("permissions.default.camera");
  Assert.deepEqual(SitePermissions.get(uri, "camera"), {
    state: SitePermissions.UNKNOWN,
    scope: SitePermissions.SCOPE_PERSISTENT,
  });
});

add_task(async function testCanvasPermission() {
  let resistFingerprinting = Services.prefs.getBoolPref("privacy.resistFingerprinting", false);
  let uri = Services.io.newURI("https://example.com");

  SitePermissions.set(uri, "canvas", SitePermissions.ALLOW);

  // Canvas permission is hidden when privacy.resistFingerprinting is false.
  Services.prefs.setBoolPref("privacy.resistFingerprinting", false);
  Assert.equal(SitePermissions.listPermissions().indexOf("canvas"), -1);
  Assert.equal(SitePermissions.getAllByURI(uri).filter(permission => permission.id === "canvas").length, 0);

  // Canvas permission is visible when privacy.resistFingerprinting is true.
  Services.prefs.setBoolPref("privacy.resistFingerprinting", true);
  Assert.notEqual(SitePermissions.listPermissions().indexOf("canvas"), -1);
  Assert.notEqual(SitePermissions.getAllByURI(uri).filter(permission => permission.id === "canvas").length, 0);

  SitePermissions.remove(uri, "canvas");
  Services.prefs.setBoolPref("privacy.resistFingerprinting", resistFingerprinting);
});

