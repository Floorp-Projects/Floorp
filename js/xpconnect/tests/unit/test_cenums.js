/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
function run_test() {

  // Load the component manifests.
  registerXPCTestComponents();
  registerAppManifest(do_get_file('../components/js/xpctest.manifest'));

  // Test for each component.
  test_interface_consts();
  test_component("@mozilla.org/js/xpc/test/native/CEnums;1");
  test_component("@mozilla.org/js/xpc/test/js/CEnums;1");
}

function test_interface_consts() {
  Assert.equal(Ci.nsIXPCTestCEnums.testConst, 1);
  Assert.equal(Ci.nsIXPCTestCEnums.shouldBe1Explicit, 1);
  Assert.equal(Ci.nsIXPCTestCEnums.shouldBe2Explicit, 2);
  Assert.equal(Ci.nsIXPCTestCEnums.shouldBe4Explicit, 4);
  Assert.equal(Ci.nsIXPCTestCEnums.shouldBe8Explicit, 8);
  Assert.equal(Ci.nsIXPCTestCEnums.shouldBe12Explicit, 12);
  Assert.equal(Ci.nsIXPCTestCEnums.shouldBe1Implicit, 1);
  Assert.equal(Ci.nsIXPCTestCEnums.shouldBe2Implicit, 2);
  Assert.equal(Ci.nsIXPCTestCEnums.shouldBe3Implicit, 3);
  Assert.equal(Ci.nsIXPCTestCEnums.shouldBe5Implicit, 5);
  Assert.equal(Ci.nsIXPCTestCEnums.shouldBe6Implicit, 6);
  Assert.equal(Ci.nsIXPCTestCEnums.shouldBe2AgainImplicit, 2);
  Assert.equal(Ci.nsIXPCTestCEnums.shouldBe3AgainImplicit, 3);
}

function test_component(contractid) {

  // Instantiate the object.
  var o = Cc[contractid].createInstance(Ci["nsIXPCTestCEnums"]);
  o.testCEnumInput(Ci.nsIXPCTestCEnums.shouldBe12Explicit);
  o.testCEnumInput(Ci.nsIXPCTestCEnums.shouldBe8Explicit | Ci.nsIXPCTestCEnums.shouldBe4Explicit);
  var a = o.testCEnumOutput();
  Assert.equal(a, Ci.nsIXPCTestCEnums.shouldBe8Explicit);
}

