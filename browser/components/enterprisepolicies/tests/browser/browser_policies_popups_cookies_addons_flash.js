/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

function URI(str) {
  return Services.io.newURI(str);
}

add_task(async function test_start_with_disabled_engine() {
  await startWithCleanSlate();
});

add_task(async function test_setup_preexisting_permissions() {
  // Pre-existing ALLOW permissions that should be overriden
  // with DENY.
  Services.perms.add(URI("https://www.pre-existing-allow.com"),
                     "popup",
                     Ci.nsIPermissionManager.ALLOW_ACTION,
                     Ci.nsIPermissionManager.EXPIRE_SESSION);

  Services.perms.add(URI("https://www.pre-existing-allow.com"),
                     "install",
                     Ci.nsIPermissionManager.ALLOW_ACTION,
                     Ci.nsIPermissionManager.EXPIRE_SESSION);

  Services.perms.add(URI("https://www.pre-existing-allow.com"),
                     "cookie",
                     Ci.nsIPermissionManager.ALLOW_ACTION,
                     Ci.nsIPermissionManager.EXPIRE_SESSION);

  Services.perms.add(URI("https://www.pre-existing-allow.com"),
                     "plugin:flash",
                     Ci.nsIPermissionManager.ALLOW_ACTION,
                     Ci.nsIPermissionManager.EXPIRE_SESSION);

  // Pre-existing DENY permissions that should be overriden
  // with ALLOW.
  Services.perms.add(URI("https://www.pre-existing-deny.com"),
                     "popup",
                     Ci.nsIPermissionManager.DENY_ACTION,
                     Ci.nsIPermissionManager.EXPIRE_SESSION);

  Services.perms.add(URI("https://www.pre-existing-deny.com"),
                     "install",
                     Ci.nsIPermissionManager.DENY_ACTION,
                     Ci.nsIPermissionManager.EXPIRE_SESSION);

  Services.perms.add(URI("https://www.pre-existing-deny.com"),
                     "cookie",
                     Ci.nsIPermissionManager.DENY_ACTION,
                     Ci.nsIPermissionManager.EXPIRE_SESSION);

  Services.perms.add(URI("https://www.pre-existing-deny.com"),
                     "plugin:flash",
                     Ci.nsIPermissionManager.DENY_ACTION,
                     Ci.nsIPermissionManager.EXPIRE_SESSION);
});

add_task(async function test_setup_activate_policies() {
  await setupPolicyEngineWithJson("config_popups_cookies_addons_flash.json");
  is(Services.policies.status, Ci.nsIEnterprisePolicies.ACTIVE, "Engine is active");
});

function checkPermission(url, expected, permissionName) {
  let expectedValue = Ci.nsIPermissionManager[`${expected}_ACTION`];
  let uri = Services.io.newURI(`https://www.${url}`);

  is(Services.perms.testPermission(uri, permissionName),
    expectedValue,
    `Correct (${permissionName}=${expected}) for URL ${url}`);

  if (expected != "UNKNOWN") {
    let permission = Services.perms.getPermissionObjectForURI(
      uri, permissionName, true);
    ok(permission, "Permission object exists");
    is(permission.expireType, Ci.nsIPermissionManager.EXPIRE_POLICY,
       "Permission expireType is correct");
  }
}

function checkAllPermissionsForType(type) {
  checkPermission("allow.com", "ALLOW", type);
  checkPermission("deny.com", "DENY", type);
  checkPermission("unknown.com", "UNKNOWN", type);
  checkPermission("pre-existing-allow.com", "DENY", type);
  checkPermission("pre-existing-deny.com", "ALLOW", type);
}

add_task(async function test_popups_policy() {
  checkAllPermissionsForType("popup");
});

add_task(async function test_webextensions_policy() {
  checkAllPermissionsForType("install");
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
  Services.perms.add(URI("https://www.allow.com"), "popup",
                     Ci.nsIPermissionManager.DENY_ACTION,
                     Ci.nsIPermissionManager.EXPIRE_SESSION);

  checkPermission("allow.com", "ALLOW", "popup");

  // Also change one un-managed permission to make sure it doesn't
  // cause any problems to the policy engine or the permission manager.
  Services.perms.add(URI("https://www.unmanaged.com"), "popup",
                   Ci.nsIPermissionManager.DENY_ACTION,
                   Ci.nsIPermissionManager.EXPIRE_SESSION);
});
