/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function run_test() {

  // Load the component manifest.
  Components.manager.autoRegister(do_get_file('../components/js/xpctest.manifest'));

  // Instantiate the object.
  var o = Cc["@mozilla.org/js/xpc/test/js/Bug809674;1"].createInstance(Ci["nsIXPCTestBug809674"]);

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
