/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function TestCEnums() {
}

TestCEnums.prototype = {
  /* Boilerplate */
  QueryInterface: ChromeUtils.generateQI(["nsIXPCTestCEnums"]),

  testCEnumInput: function(input) {
    if (input != Ci.nsIXPCTestCEnums.shouldBe12Explicit)
    {
      throw new Error("Enum values do not match expected value");
    }
  },

  testCEnumOutput: function() {
    return Ci.nsIXPCTestCEnums.shouldBe8Explicit;
  },
};


function run_test() {
  // Load the component manifests.
  registerXPCTestComponents();

  // Test for each component.
  test_interface_consts();
  test_component(Cc["@mozilla.org/js/xpc/test/native/CEnums;1"].createInstance());
  test_component(xpcWrap(new TestCEnums()));
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

function test_component(obj) {
  var o = obj.QueryInterface(Ci.nsIXPCTestCEnums);
  o.testCEnumInput(Ci.nsIXPCTestCEnums.shouldBe12Explicit);
  o.testCEnumInput(Ci.nsIXPCTestCEnums.shouldBe8Explicit | Ci.nsIXPCTestCEnums.shouldBe4Explicit);
  var a = o.testCEnumOutput();
  Assert.equal(a, Ci.nsIXPCTestCEnums.shouldBe8Explicit);
}

