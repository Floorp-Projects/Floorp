/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function run_test() {
  let uri = Services.io.newURI("https://example.org");

  // Check that without a pref the default return value is UNKNOWN.
  do_check_eq(Services.perms.testPermission(uri, "camera"), Services.perms.UNKNOWN_ACTION);

  // Check that the default return value changed after setting the pref.
  Services.prefs.setIntPref("permissions.default.camera", Services.perms.DENY_ACTION);
  do_check_eq(Services.perms.testPermission(uri, "camera"), Services.perms.DENY_ACTION);

  // Check that functions that do not directly return a permission value still
  // consider the permission as being set to its default.
  do_check_null(Services.perms.getPermissionObjectForURI(uri, "camera", false));

  // Check that other permissions still return UNKNOWN.
  do_check_eq(Services.perms.testPermission(uri, "geo"), Services.perms.UNKNOWN_ACTION);

  // Check that the default return value changed after changing the pref.
  Services.prefs.setIntPref("permissions.default.camera", Services.perms.ALLOW_ACTION);
  do_check_eq(Services.perms.testPermission(uri, "camera"), Services.perms.ALLOW_ACTION);

  // Check that the preference is ignored if there is a value.
  Services.perms.add(uri, "camera", Services.perms.DENY_ACTION);
  do_check_eq(Services.perms.testPermission(uri, "camera"), Services.perms.DENY_ACTION);
  do_check_true(Services.perms.getPermissionObjectForURI(uri, "camera", false) != null);

  // The preference should be honored again, after resetting the permissions.
  Services.perms.removeAll();
  do_check_eq(Services.perms.testPermission(uri, "camera"), Services.perms.ALLOW_ACTION);

  // Should be UNKNOWN after clearing the pref.
  Services.prefs.clearUserPref("permissions.default.camera");
  do_check_eq(Services.perms.testPermission(uri, "camera"), Services.perms.UNKNOWN_ACTION);
}
