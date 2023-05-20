/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function run_test() {
  let principal =
    Services.scriptSecurityManager.createContentPrincipalFromOrigin(
      "https://example.org"
    );

  // Check that without a pref the default return value is UNKNOWN.
  Assert.equal(
    Services.perms.testPermissionFromPrincipal(principal, "camera"),
    Services.perms.UNKNOWN_ACTION
  );

  // Check that the default return value changed after setting the pref.
  Services.prefs.setIntPref(
    "permissions.default.camera",
    Services.perms.DENY_ACTION
  );
  Assert.equal(
    Services.perms.testPermissionFromPrincipal(principal, "camera"),
    Services.perms.DENY_ACTION
  );

  // Check that functions that do not directly return a permission value still
  // consider the permission as being set to its default.
  Assert.equal(
    null,
    Services.perms.getPermissionObject(principal, "camera", false)
  );

  // Check that other permissions still return UNKNOWN.
  Assert.equal(
    Services.perms.testPermissionFromPrincipal(principal, "geo"),
    Services.perms.UNKNOWN_ACTION
  );

  // Check that the default return value changed after changing the pref.
  Services.prefs.setIntPref(
    "permissions.default.camera",
    Services.perms.ALLOW_ACTION
  );
  Assert.equal(
    Services.perms.testPermissionFromPrincipal(principal, "camera"),
    Services.perms.ALLOW_ACTION
  );

  // Check that the preference is ignored if there is a value.
  Services.perms.addFromPrincipal(
    principal,
    "camera",
    Services.perms.DENY_ACTION
  );
  Assert.equal(
    Services.perms.testPermissionFromPrincipal(principal, "camera"),
    Services.perms.DENY_ACTION
  );
  Assert.ok(
    Services.perms.getPermissionObject(principal, "camera", false) != null
  );

  // The preference should be honored again, after resetting the permissions.
  Services.perms.removeAll();
  Assert.equal(
    Services.perms.testPermissionFromPrincipal(principal, "camera"),
    Services.perms.ALLOW_ACTION
  );

  // Should be UNKNOWN after clearing the pref.
  Services.prefs.clearUserPref("permissions.default.camera");
  Assert.equal(
    Services.perms.testPermissionFromPrincipal(principal, "camera"),
    Services.perms.UNKNOWN_ACTION
  );
}
