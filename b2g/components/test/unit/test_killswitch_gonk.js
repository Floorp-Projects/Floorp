/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

Cu.import("resource://gre/modules/NetUtil.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyGetter(this, "libcutils", function () {
  Cu.import("resource://gre/modules/systemlibs.js");
  return libcutils;
});

// Trivial test just to make sure we have no syntax error
add_test(function test_ksm_ok() {
  ok(KillSwitchMain, "KillSwitchMain object exists");

  run_next_test();
});

add_test(function test_has_libcutils() {
  let rv = KillSwitchMain.checkLibcUtils();
  strictEqual(rv, true);
  run_next_test();
});

add_test(function test_libcutils_works() {
  KillSwitchMain._libcutils.property_set("ro.moz.ks_test", "wesh");
  let rv_ks_get  = KillSwitchMain._libcutils.property_get("ro.moz.ks_test");
  strictEqual(rv_ks_get, "wesh")
  let rv_sys_get = libcutils.property_get("ro.moz.ks_test")
  strictEqual(rv_sys_get, "wesh")

  KillSwitchMain._libcutils.property_set("ro.moz.ks_test2", "123456789");
  rv_ks_get  = KillSwitchMain._libcutils.property_get("ro.moz.ks_test2");
  strictEqual(rv_ks_get, "123456789")
  rv_sys_get = libcutils.property_get("ro.moz.ks_test2")
  strictEqual(rv_sys_get, "123456789")

  run_next_test();
});

install_common_tests();
