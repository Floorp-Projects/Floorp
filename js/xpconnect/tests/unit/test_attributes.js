/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Cc = Components.classes;
const Ci = Components.interfaces;

function run_test() {

  // Load the component manifests.
  registerAppManifest(do_get_file('../components/native/xpctest.manifest'));
  registerAppManifest(do_get_file('../components/js/xpctest.manifest'));

  // Test for each component.
  test_component_readwrite("@mozilla.org/js/xpc/test/native/ObjectReadWrite;1");
  test_component_readwrite("@mozilla.org/js/xpc/test/js/ObjectReadWrite;1");
  test_component_readonly("@mozilla.org/js/xpc/test/native/ObjectReadOnly;1");
  test_component_readonly("@mozilla.org/js/xpc/test/js/ObjectReadOnly;1");
}

function test_component_readwrite(contractid) {

  // Instantiate the object.
  var o = Cc[contractid].createInstance(Ci["nsIXPCTestObjectReadWrite"]);

  // Test the initial values.
  do_check_eq("XPConnect Read-Writable String", o.stringProperty);
  do_check_eq(true, o.booleanProperty);
  do_check_eq(32767, o.shortProperty);
  do_check_eq(2147483647, o.longProperty);
  do_check_true(5.25 < o.floatProperty && 5.75 > o.floatProperty);
  do_check_eq("X", o.charProperty);
  do_check_eq(-1, o.timeProperty);

  // Write new values.
  o.stringProperty = "another string";
  o.booleanProperty = false;
  o.shortProperty = -12345;
  o.longProperty = 1234567890;
  o.floatProperty = 10.2;
  o.charProperty = "Z";
  o.timeProperty = 1;

  // Test the new values.
  do_check_eq("another string", o.stringProperty);
  do_check_eq(false, o.booleanProperty);
  do_check_eq(-12345, o.shortProperty);
  do_check_eq(1234567890, o.longProperty);
  do_check_true(10.15 < o.floatProperty && 10.25 > o.floatProperty);
  do_check_eq("Z", o.charProperty);
  do_check_eq(1, o.timeProperty);

  // Assign values that differ from the expected type to verify conversion.

  function SetAndTestBooleanProperty(newValue, expectedValue) {
    o.booleanProperty = newValue;
    do_check_eq(expectedValue, o.booleanProperty);
  };
  SetAndTestBooleanProperty(false, false);
  SetAndTestBooleanProperty(1, true);
  SetAndTestBooleanProperty(null, false);
  SetAndTestBooleanProperty("A", true);
  SetAndTestBooleanProperty(undefined, false);
  SetAndTestBooleanProperty([], true);
  SetAndTestBooleanProperty({}, true);
}

function test_component_readonly(contractid) {

  // Instantiate the object.
  var o = Cc[contractid].createInstance(Ci["nsIXPCTestObjectReadOnly"]);

  // Test the initial values.
  do_check_eq("XPConnect Read-Only String", o.strReadOnly);
  do_check_eq(true, o.boolReadOnly);
  do_check_eq(32767, o.shortReadOnly);
  do_check_eq(2147483647, o.longReadOnly);
  do_check_true(5.25 < o.floatReadOnly && 5.75 > o.floatReadOnly);
  do_check_eq("X", o.charReadOnly);
  do_check_eq(-1, o.timeReadOnly);
}
