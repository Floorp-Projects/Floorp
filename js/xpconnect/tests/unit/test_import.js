/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var TestFile;
function run_test() {
  var scope = {};
  var exports = ChromeUtils.import("resource://test/TestFile.jsm", scope);
  Assert.equal(typeof(scope.TestFile), "object");
  Assert.equal(typeof(scope.TestFile.doTest), "function");

  equal(scope.TestFile, exports.TestFile);
  deepEqual(Object.keys(scope), ["TestFile"]);
  deepEqual(Object.keys(exports), ["TestFile"]);

  exports = ChromeUtils.import("resource://test/TestFile.jsm");
  equal(scope.TestFile, exports.TestFile);
  deepEqual(Object.keys(exports), ["TestFile"]);

  // access module's global object directly without importing any
  // symbols
  Assert.throws(
    () => ChromeUtils.import("resource://test/TestFile.jsm", null),
    TypeError
  );

  // import symbols to our global object
  Assert.equal(typeof(Cu.import), "function");
  ({TestFile} = ChromeUtils.import("resource://test/TestFile.jsm"));
  Assert.equal(typeof(TestFile), "object");
  Assert.equal(typeof(TestFile.doTest), "function");

  // try on a new object
  var scope2 = {};
  ChromeUtils.import("resource://test/TestFile.jsm", scope2);
  Assert.equal(typeof(scope2.TestFile), "object");
  Assert.equal(typeof(scope2.TestFile.doTest), "function");

  Assert.ok(scope2.TestFile == scope.TestFile);

  // try on a new object using the resolved URL
  var res = Cc["@mozilla.org/network/protocol;1?name=resource"]
              .getService(Ci.nsIResProtocolHandler);
  var resURI = Cc["@mozilla.org/network/io-service;1"]
                 .getService(Ci.nsIIOService)
                 .newURI("resource://test/TestFile.jsm");
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
      ChromeUtils.import("resource://test/TestFile.jsm", "wrong");
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
