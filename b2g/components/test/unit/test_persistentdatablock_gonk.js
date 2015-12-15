/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

Cu.import("resource://gre/modules/NetUtil.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyGetter(this, "libcutils", function () {
  Cu.import("resource://gre/modules/systemlibs.js");
  return libcutils;
});

function run_test() {
  do_get_profile();
  Cu.import("resource://gre/modules/PersistentDataBlock.jsm");
  // We need to point to a valid partition for some of the tests. This is the /cache
  // partition in the emulator (x86-KitaKat).
  run_next_test();
}

_installTests();
