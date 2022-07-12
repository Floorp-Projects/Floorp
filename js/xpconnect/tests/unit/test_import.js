/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var AppConstants;
function run_test() {
  var scope = {};
  var exports = ChromeUtils.import("resource://gre/modules/AppConstants.jsm", scope);
  Assert.equal(typeof(scope.AppConstants), "object");
  Assert.equal(typeof(scope.AppConstants.isPlatformAndVersionAtLeast), "function");

  equal(scope.AppConstants, exports.AppConstants);
  deepEqual(Object.keys(scope), ["AppConstants"]);
  deepEqual(Object.keys(exports), ["AppConstants"]);

  exports = ChromeUtils.import("resource://gre/modules/AppConstants.jsm");
  equal(scope.AppConstants, exports.AppConstants);
  deepEqual(Object.keys(exports), ["AppConstants"]);

  // access module's global object directly without importing any
  // symbols
  Assert.throws(
    () => ChromeUtils.import("resource://gre/modules/AppConstants.jsm", null),
    TypeError
  );

  // import symbols to our global object
  Assert.equal(typeof(Cu.import), "function");
  ({AppConstants} = ChromeUtils.import("resource://gre/modules/AppConstants.jsm"));
  Assert.equal(typeof(AppConstants), "object");
  Assert.equal(typeof(AppConstants.isPlatformAndVersionAtLeast), "function");

  // try on a new object
  var scope2 = {};
  ChromeUtils.import("resource://gre/modules/AppConstants.jsm", scope2);
  Assert.equal(typeof(scope2.AppConstants), "object");
  Assert.equal(typeof(scope2.AppConstants.isPlatformAndVersionAtLeast), "function");

  Assert.ok(scope2.AppConstants == scope.AppConstants);

  // try on a new object using the resolved URL
  var res = Cc["@mozilla.org/network/protocol;1?name=resource"]
              .getService(Ci.nsIResProtocolHandler);
  var resURI = Cc["@mozilla.org/network/io-service;1"]
                 .getService(Ci.nsIIOService)
                 .newURI("resource://gre/modules/AppConstants.jsm");
  dump("resURI: " + resURI + "\n");
  var filePath = res.resolveURI(resURI);
  var scope3 = {};
  Assert.throws(
    () => ChromeUtils.import(filePath, scope3),
    /SecurityError/, "Expecting file URI not to be imported"
  );

  // make sure we throw when the second arg is bogus
  var didThrow = false;
  try {
      ChromeUtils.import("resource://gre/modules/AppConstants.jsm", "wrong");
  } catch (ex) {
      print("exception (expected): " + ex);
      didThrow = true;
  }
  Assert.ok(didThrow);

  // make sure we throw when the URL scheme is not known
  var scope4 = {};
  const wrongScheme = "data:text/javascript,var a = {a:1}";
  Assert.throws(
    () => ChromeUtils.import(wrongScheme, scope4),
    /SecurityError/, "Expecting data URI not to be imported"
  );
}
