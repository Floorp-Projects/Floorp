/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

function URI(str) {
  return Services.io.newURI(str);
}

add_task(async function test_setup_preexisting_permissions() {
  // Pre-existing ALLOW permissions that should be overriden
  // with DENY.

  // No ALLOW -> DENY override for popup and install permissions,
  // because their policies only supports the Allow parameter.

  PermissionTestUtils.add(
    "https://www.pre-existing-allow.com",
    "cookie",
    Ci.nsIPermissionManager.ALLOW_ACTION,
    Ci.nsIPermissionManager.EXPIRE_SESSION
  );

  PermissionTestUtils.add(
    "https://www.pre-existing-allow.com",
    "plugin:flash",
    Ci.nsIPermissionManager.ALLOW_ACTION,
    Ci.nsIPermissionManager.EXPIRE_SESSION
  );

  // Pre-existing DENY permissions that should be overriden
  // with ALLOW.
  PermissionTestUtils.add(
    "https://www.pre-existing-deny.com",
    "popup",
    Ci.nsIPermissionManager.DENY_ACTION,
    Ci.nsIPermissionManager.EXPIRE_SESSION
  );

  PermissionTestUtils.add(
    "https://www.pre-existing-deny.com",
    "install",
    Ci.nsIPermissionManager.DENY_ACTION,
    Ci.nsIPermissionManager.EXPIRE_SESSION
  );

  PermissionTestUtils.add(
    "https://www.pre-existing-deny.com",
    "cookie",
    Ci.nsIPermissionManager.DENY_ACTION,
    Ci.nsIPermissionManager.EXPIRE_SESSION
  );

  PermissionTestUtils.add(
    "https://www.pre-existing-deny.com",
    "plugin:flash",
    Ci.nsIPermissionManager.DENY_ACTION,
    Ci.nsIPermissionManager.EXPIRE_SESSION
  );
});

add_task(async function test_setup_activate_policies() {
  await setupPolicyEngineWithJson("config_popups_cookies_addons_flash.json");
  equal(
    Services.policies.status,
    Ci.nsIEnterprisePolicies.ACTIVE,
    "Engine is active"
  );
});

function checkPermission(url, expected, permissionName) {
  let expectedValue = Ci.nsIPermissionManager[`${expected}_ACTION`];
  let uri = Services.io.newURI(`https://www.${url}`);

  equal(
    PermissionTestUtils.testPermission(uri, permissionName),
    expectedValue,
    `Correct (${permissionName}=${expected}) for URL ${url}`
  );

  if (expected != "UNKNOWN") {
    let permission = PermissionTestUtils.getPermissionObject(
      uri,
      permissionName,
      true
    );
    ok(permission, "Permission object exists");
    equal(
      permission.expireType,
      Ci.nsIPermissionManager.EXPIRE_POLICY,
      "Permission expireType is correct"
    );
  }
}

function checkAllPermissionsForType(type, typeSupportsDeny = true) {
  checkPermission("allow.com", "ALLOW", type);
  checkPermission("unknown.com", "UNKNOWN", type);
  checkPermission("pre-existing-deny.com", "ALLOW", type);

  if (typeSupportsDeny) {
    checkPermission("deny.com", "DENY", type);
    checkPermission("pre-existing-allow.com", "DENY", type);
  }
}

add_task(async function test_popups_policy() {
  checkAllPermissionsForType("popup", false);
});

add_task(async function test_webextensions_policy() {
  checkAllPermissionsForType("install", false);
});

add_task(async function test_cookies_policy() {
  checkAllPermissionsForType("cookie");
});

add_task(async function test_flash_policy() {
  checkAllPermissionsForType("plugin:flash");
});

add_task(async function test_change_permission() {
  // Checks that changing a permission will still retain the
  // value set through the engine.
  PermissionTestUtils.add(
    "https://www.allow.com",
    "cookie",
    Ci.nsIPermissionManager.DENY_ACTION,
    Ci.nsIPermissionManager.EXPIRE_SESSION
  );

  checkPermission("allow.com", "ALLOW", "cookie");

  // Also change one un-managed permission to make sure it doesn't
  // cause any problems to the policy engine or the permission manager.
  PermissionTestUtils.add(
    "https://www.unmanaged.com",
    "cookie",
    Ci.nsIPermissionManager.DENY_ACTION,
    Ci.nsIPermissionManager.EXPIRE_SESSION
  );
});
