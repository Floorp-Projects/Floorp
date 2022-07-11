/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

Services.prefs.setBoolPref("security.allow_eval_with_system_principal", true);
registerCleanupFunction(() => {
  Services.prefs.clearUserPref("security.allow_eval_with_system_principal");
});

var TestUtils = {
  QueryInterface: ChromeUtils.generateQI(["nsIXPCTestUtils"]),
  doubleWrapFunction(fun) { return fun }
};

function run_test() {
  // Generate a CCW to a function.
  var sb = new Cu.Sandbox(this);
  sb.eval('function fun(x) { return x; }');
  Assert.equal(sb.fun("foo"), "foo");

  // Double-wrap the CCW.
  var utils = xpcWrap(TestUtils, Ci.nsIXPCTestUtils);
  var doubleWrapped = utils.doubleWrapFunction(sb.fun);
  Assert.equal(doubleWrapped.echo("foo"), "foo");

  // GC.
  Cu.forceGC();

  // Make sure it still works.
  Assert.equal(doubleWrapped.echo("foo"), "foo");
}
