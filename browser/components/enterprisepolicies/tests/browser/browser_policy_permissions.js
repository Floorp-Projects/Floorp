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

  Services.perms.add(URI("https://www.pre-existing-allow.com"),
                     "camera",
                     Ci.nsIPermissionManager.ALLOW_ACTION,
                     Ci.nsIPermissionManager.EXPIRE_SESSION);
  Services.perms.add(URI("https://www.pre-existing-allow.com"),
                     "microphone",
                     Ci.nsIPermissionManager.ALLOW_ACTION,
                     Ci.nsIPermissionManager.EXPIRE_SESSION);
  Services.perms.add(URI("https://www.pre-existing-allow.com"),
                     "geo",
                     Ci.nsIPermissionManager.ALLOW_ACTION,
                     Ci.nsIPermissionManager.EXPIRE_SESSION);
  Services.perms.add(URI("https://www.pre-existing-allow.com"),
                     "desktop-notification",
                     Ci.nsIPermissionManager.ALLOW_ACTION,
                     Ci.nsIPermissionManager.EXPIRE_SESSION);

  // Pre-existing DENY permissions that should be overriden
  // with ALLOW.

  Services.perms.add(URI("https://www.pre-existing-deny.com"),
                     "camera",
                     Ci.nsIPermissionManager.DENY_ACTION,
                     Ci.nsIPermissionManager.EXPIRE_SESSION);
  Services.perms.add(URI("https://www.pre-existing-deny.com"),
                     "microphone",
                     Ci.nsIPermissionManager.DENY_ACTION,
                     Ci.nsIPermissionManager.EXPIRE_SESSION);
  Services.perms.add(URI("https://www.pre-existing-deny.com"),
                     "geo",
                     Ci.nsIPermissionManager.DENY_ACTION,
                     Ci.nsIPermissionManager.EXPIRE_SESSION);
  Services.perms.add(URI("https://www.pre-existing-deny.com"),
                     "desktop-notification",
                     Ci.nsIPermissionManager.DENY_ACTION,
                     Ci.nsIPermissionManager.EXPIRE_SESSION);
});

add_task(async function test_setup_activate_policies() {
  await setupPolicyEngineWithJson({
    "policies": {
      "Permissions": {
        "Camera": {
          "Allow": [
            "https://www.allow.com",
            "https://www.pre-existing-deny.com"
          ],
          "Block": [
            "https://www.deny.com",
            "https://www.pre-existing-allow.com"
          ]
        },
        "Microphone": {
          "Allow": [
            "https://www.allow.com",
            "https://www.pre-existing-deny.com"
          ],
          "Block": [
            "https://www.deny.com",
            "https://www.pre-existing-allow.com"
          ]
        },
        "Location": {
          "Allow": [
            "https://www.allow.com",
            "https://www.pre-existing-deny.com"
          ],
          "Block": [
            "https://www.deny.com",
            "https://www.pre-existing-allow.com"
          ]
        },
        "Notifications": {
          "Allow": [
            "https://www.allow.com",
            "https://www.pre-existing-deny.com"
          ],
          "Block": [
            "https://www.deny.com",
            "https://www.pre-existing-allow.com"
          ]
        }
      }
    }
  });
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

function checkAllPermissionsForType(type, typeSupportsDeny = true) {
  checkPermission("allow.com", "ALLOW", type);
  checkPermission("unknown.com", "UNKNOWN", type);
  checkPermission("pre-existing-deny.com", "ALLOW", type);

  if (typeSupportsDeny) {
    checkPermission("deny.com", "DENY", type);
    checkPermission("pre-existing-allow.com", "DENY", type);
  }
}

add_task(async function test_camera_policy() {
  checkAllPermissionsForType("camera");
});

add_task(async function test_microphone_policy() {
  checkAllPermissionsForType("microphone");
});

add_task(async function test_location_policy() {
  checkAllPermissionsForType("geo");
});

add_task(async function test_notifications_policy() {
  checkAllPermissionsForType("desktop-notification");
});

add_task(async function test_change_permission() {
  // Checks that changing a permission will still retain the
  // value set through the engine.
  Services.perms.add(URI("https://www.allow.com"), "camera",
                     Ci.nsIPermissionManager.DENY_ACTION,
                     Ci.nsIPermissionManager.EXPIRE_SESSION);
  Services.perms.add(URI("https://www.allow.com"), "microphone",
                     Ci.nsIPermissionManager.DENY_ACTION,
                     Ci.nsIPermissionManager.EXPIRE_SESSION);
  Services.perms.add(URI("https://www.allow.com"), "geo",
                     Ci.nsIPermissionManager.DENY_ACTION,
                     Ci.nsIPermissionManager.EXPIRE_SESSION);
  Services.perms.add(URI("https://www.allow.com"), "desktop-notification",
                     Ci.nsIPermissionManager.DENY_ACTION,
                     Ci.nsIPermissionManager.EXPIRE_SESSION);

  checkPermission("allow.com", "ALLOW", "camera");
  checkPermission("allow.com", "ALLOW", "microphone");
  checkPermission("allow.com", "ALLOW", "geo");
  checkPermission("allow.com", "ALLOW", "desktop-notification");

  // Also change one un-managed permission to make sure it doesn't
  // cause any problems to the policy engine or the permission manager.
  Services.perms.add(URI("https://www.unmanaged.com"), "camera",
                   Ci.nsIPermissionManager.DENY_ACTION,
                   Ci.nsIPermissionManager.EXPIRE_SESSION);
  Services.perms.add(URI("https://www.unmanaged.com"), "microphone",
                   Ci.nsIPermissionManager.DENY_ACTION,
                   Ci.nsIPermissionManager.EXPIRE_SESSION);
  Services.perms.add(URI("https://www.unmanaged.com"), "geo",
                   Ci.nsIPermissionManager.DENY_ACTION,
                   Ci.nsIPermissionManager.EXPIRE_SESSION);
  Services.perms.add(URI("https://www.unmanaged.com"), "desktop-notification",
                   Ci.nsIPermissionManager.DENY_ACTION,
                   Ci.nsIPermissionManager.EXPIRE_SESSION);
});
