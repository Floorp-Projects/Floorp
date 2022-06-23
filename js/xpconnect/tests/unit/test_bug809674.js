/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


var Bug809674 = {
  QueryInterface: ChromeUtils.generateQI(["nsIXPCTestBug809674"]),

  /* nsIXPCTestBug809674 */
  methodWithOptionalArgc() {},

  addArgs(x, y) {
    return x + y;
  },
  addSubMulArgs(x, y, subOut, mulOut) {
    subOut.value = x - y;
    mulOut.value = x * y;
    return x + y;
  },
  addVals(x, y) {
    return x + y;
  },
  addMany(x1, x2, x3, x4, x5, x6, x7, x8) {
    return x1 + x2 + x3 + x4 + x5 + x6 + x7 + x8;
  },

  methodNoArgs() {
    return 7;
  },
  methodNoArgsNoRetVal() {},

  valProperty: {value: 42},
  uintProperty: 123,
};

function run_test() {
  // XPConnect wrap the object
  var o = xpcWrap(Bug809674, Ci.nsIXPCTestBug809674);

  // Methods marked [implicit_jscontext].

  Assert.equal(o.addArgs(12, 34), 46);

  var subRes = {}, mulRes = {};
  Assert.equal(o.addSubMulArgs(9, 7, subRes, mulRes), 16);
  Assert.equal(subRes.value, 2);
  Assert.equal(mulRes.value, 63);

  Assert.equal(o.addVals("foo", "x"), "foox");
  Assert.equal(o.addVals("foo", 1.2), "foo1.2");
  Assert.equal(o.addVals(1234, "foo"), "1234foo");

  Assert.equal(o.addMany(1, 2, 4, 8, 16, 32, 64, 128), 255);

  Assert.equal(o.methodNoArgs(), 7);
  Assert.equal(o.methodNoArgsNoRetVal(), undefined);

  // Attributes marked [implicit_jscontext].

  Assert.equal(o.valProperty.value, 42);
  o.valProperty = o;
  Assert.equal(o.valProperty, o);

  Assert.equal(o.uintProperty, 123);
  o.uintProperty++;
  Assert.equal(o.uintProperty, 124);

  // [optional_argc] is not supported.
  try {
    o.methodWithOptionalArgc();
    Assert.ok(false);
  } catch (e) {
    Assert.ok(true);
    Assert.ok(/optional_argc/.test(e))
  }
}
