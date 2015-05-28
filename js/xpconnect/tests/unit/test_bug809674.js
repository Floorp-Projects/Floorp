/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Cc = Components.classes;
const Ci = Components.interfaces;

function run_test() {

  // Load the component manifest.
  Components.manager.autoRegister(do_get_file('../components/js/xpctest.manifest'));

  // Test for each component.
  test_property_throws("@mozilla.org/js/xpc/test/js/Bug809674;1");
}

function test_property_throws(contractid) {

  // Instantiate the object.
  var o = Cc[contractid].createInstance(Ci["nsIXPCTestBug809674"]);

  // Test the initial values.
  try {
    o.jsvalProperty;
    do_check_true(false);
  } catch (e) {
    do_check_true(true);
    do_check_true(/implicit_jscontext/.test(e))
  }

}
