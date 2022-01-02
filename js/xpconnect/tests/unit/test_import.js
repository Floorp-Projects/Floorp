/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var XPCOMUtils;
function run_test() {
  var scope = {};
  var exports = ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm", scope);
  Assert.equal(typeof(scope.XPCOMUtils), "object");
  Assert.equal(typeof(scope.XPCOMUtils.defineLazyGetter), "function");

  equal(scope.XPCOMUtils, exports.XPCOMUtils);
  deepEqual(Object.keys(scope), ["XPCOMUtils"]);
  deepEqual(Object.keys(exports), ["XPCOMUtils"]);

  exports = ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
  equal(scope.XPCOMUtils, exports.XPCOMUtils);
  deepEqual(Object.keys(exports), ["XPCOMUtils"]);

  // access module's global object directly without importing any
  // symbols
  var module = ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm", null);
  Assert.equal(typeof(XPCOMUtils), "undefined");
  Assert.equal(typeof(module), "object");
  Assert.equal(typeof(module.XPCOMUtils), "object");
  Assert.equal(typeof(module.XPCOMUtils.defineLazyGetter), "function");
  Assert.ok(scope.XPCOMUtils == module.XPCOMUtils);

  // import symbols to our global object
  Assert.equal(typeof(Cu.import), "function");
  ({XPCOMUtils} = ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm"));
  Assert.equal(typeof(XPCOMUtils), "object");
  Assert.equal(typeof(XPCOMUtils.defineLazyGetter), "function");

  // try on a new object
  var scope2 = {};
  ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm", scope2);
  Assert.equal(typeof(scope2.XPCOMUtils), "object");
  Assert.equal(typeof(scope2.XPCOMUtils.defineLazyGetter), "function");

  Assert.ok(scope2.XPCOMUtils == scope.XPCOMUtils);

  // try on a new object using the resolved URL
  var res = Cc["@mozilla.org/network/protocol;1?name=resource"]
              .getService(Ci.nsIResProtocolHandler);
  var resURI = Cc["@mozilla.org/network/io-service;1"]
                 .getService(Ci.nsIIOService)
                 .newURI("resource://gre/modules/XPCOMUtils.jsm");
  dump("resURI: " + resURI + "\n");
  var filePath = res.resolveURI(resURI);
  var scope3 = {};
  Assert.throws(() => ChromeUtils.import(filePath, scope3),
                /NS_ERROR_UNEXPECTED/);

  // make sure we throw when the second arg is bogus
  var didThrow = false;
  try {
      ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm", "wrong");
  } catch (ex) {
      print("exception (expected): " + ex);
      didThrow = true;
  }
  Assert.ok(didThrow);

  // try to create a component
  do_load_manifest("component_import.manifest");
  const contractID = "@mozilla.org/tests/module-importer;";
  Assert.ok((contractID + "1") in Cc);
  var foo = Cc[contractID + "1"]
              .createInstance(Ci.nsIClassInfo);
  Assert.ok(Boolean(foo));
  Assert.ok(foo.contractID == contractID + "1");
  // XXX the following check succeeds only if the test component wasn't
  //     already registered. Need to figure out a way to force registration
  //     (to manually force it, delete compreg.dat before running the test)
  // do_check_true(foo.wrappedJSObject.postRegisterCalled);

  // Call getInterfaces to test line numbers in JS components.  But as long as
  // we're doing that, why not test what it returns too?
  var interfaces = foo.interfaces;
  Assert.ok(Array.isArray(interfaces));
  Assert.equal(interfaces.length, 1);
  Assert.ok(interfaces[0].equals(Ci.nsIClassInfo))

  // try to create another component which doesn't directly implement QI
  Assert.ok((contractID + "2") in Cc);
  var bar = Cc[contractID + "2"]
              .createInstance(Ci.nsIClassInfo);
  Assert.ok(Boolean(bar));
  Assert.ok(bar.contractID == contractID + "2");
}
