/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function run_test() {

  // Load the component manifests.
  registerXPCTestComponents();
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
  Assert.equal("XPConnect Read-Writable String", o.stringProperty);
  Assert.equal(true, o.booleanProperty);
  Assert.equal(32767, o.shortProperty);
  Assert.equal(2147483647, o.longProperty);
  Assert.ok(5.25 < o.floatProperty && 5.75 > o.floatProperty);
  Assert.equal("X", o.charProperty);
  Assert.equal(-1, o.timeProperty);

  // Write new values.
  o.stringProperty = "another string";
  o.booleanProperty = false;
  o.shortProperty = -12345;
  o.longProperty = 1234567890;
  o.floatProperty = 10.2;
  o.charProperty = "Z";
  o.timeProperty = 1;

  // Test the new values.
  Assert.equal("another string", o.stringProperty);
  Assert.equal(false, o.booleanProperty);
  Assert.equal(-12345, o.shortProperty);
  Assert.equal(1234567890, o.longProperty);
  Assert.ok(10.15 < o.floatProperty && 10.25 > o.floatProperty);
  Assert.equal("Z", o.charProperty);
  Assert.equal(1, o.timeProperty);

  // Assign values that differ from the expected type to verify conversion.

  function SetAndTestBooleanProperty(newValue, expectedValue) {
    o.booleanProperty = newValue;
    Assert.equal(expectedValue, o.booleanProperty);
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
  Assert.equal("XPConnect Read-Only String", o.strReadOnly);
  Assert.equal(true, o.boolReadOnly);
  Assert.equal(32767, o.shortReadOnly);
  Assert.equal(2147483647, o.longReadOnly);
  Assert.ok(5.25 < o.floatReadOnly && 5.75 > o.floatReadOnly);
  Assert.equal("X", o.charReadOnly);
  Assert.equal(-1, o.timeReadOnly);
}
