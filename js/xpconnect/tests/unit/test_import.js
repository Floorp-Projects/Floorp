/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
 
function run_test() {   
  var scope = {};
  Components.utils.import("resource://gre/modules/XPCOMUtils.jsm", scope);
  do_check_eq(typeof(scope.XPCOMUtils), "object");
  do_check_eq(typeof(scope.XPCOMUtils.generateNSGetFactory), "function");
  
  // access module's global object directly without importing any
  // symbols
  var module = Components.utils.import("resource://gre/modules/XPCOMUtils.jsm",
                                       null);
  do_check_eq(typeof(XPCOMUtils), "undefined");
  do_check_eq(typeof(module), "object");
  do_check_eq(typeof(module.XPCOMUtils), "object");
  do_check_eq(typeof(module.XPCOMUtils.generateNSGetFactory), "function");
  do_check_true(scope.XPCOMUtils == module.XPCOMUtils);

  // import symbols to our global object
  do_check_eq(typeof(Components.utils.import), "function");
  Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");
  do_check_eq(typeof(XPCOMUtils), "object");
  do_check_eq(typeof(XPCOMUtils.generateNSGetFactory), "function");
  
  // try on a new object
  var scope2 = {};
  Components.utils.import("resource://gre/modules/XPCOMUtils.jsm", scope2);
  do_check_eq(typeof(scope2.XPCOMUtils), "object");
  do_check_eq(typeof(scope2.XPCOMUtils.generateNSGetFactory), "function");
  
  do_check_true(scope2.XPCOMUtils == scope.XPCOMUtils);

  // try on a new object using the resolved URL
  var res = Components.classes["@mozilla.org/network/protocol;1?name=resource"]
                      .getService(Components.interfaces.nsIResProtocolHandler);
  var resURI = res.newURI("resource://gre/modules/XPCOMUtils.jsm");
  dump("resURI: " + resURI + "\n");
  var filePath = res.resolveURI(resURI);
  var scope3 = {};
  Components.utils.import(filePath, scope3);
  do_check_eq(typeof(scope3.XPCOMUtils), "object");
  do_check_eq(typeof(scope3.XPCOMUtils.generateNSGetFactory), "function");
  
  do_check_true(scope3.XPCOMUtils == scope.XPCOMUtils);

  // make sure we throw when the second arg is bogus
  var didThrow = false;
  try {
      Components.utils.import("resource://gre/modules/XPCOMUtils.jsm", "wrong");
  } catch (ex) {
      print("exception (expected): " + ex);
      didThrow = true;
  }
  do_check_true(didThrow);
 
  // try to create a component
  do_load_manifest("component_import.manifest");
  const contractID = "@mozilla.org/tests/module-importer;";
  do_check_true((contractID + "1") in Components.classes);
  var foo = Components.classes[contractID + "1"]
                      .createInstance(Components.interfaces.nsIClassInfo);
  do_check_true(Boolean(foo));
  do_check_true(foo.contractID == contractID + "1");
  // XXX the following check succeeds only if the test component wasn't
  //     already registered. Need to figure out a way to force registration
  //     (to manually force it, delete compreg.dat before running the test)
  // do_check_true(foo.wrappedJSObject.postRegisterCalled);

  // Call getInterfaces to test line numbers in JS components.  But as long as
  // we're doing that, why not test what it returns too?
  // Kind of odd that this is not returning an array containing the
  // number... Or for that matter not returning an array containing an object?
  var interfaces = foo.getInterfaces({});
  do_check_eq(interfaces, Components.interfaces.nsIClassInfo.number);

  // try to create a component by CID
  const cid = "{6b933fe6-6eba-4622-ac86-e4f654f1b474}";
  do_check_true(cid in Components.classesByID);
  foo = Components.classesByID[cid]
                  .createInstance(Components.interfaces.nsIClassInfo);
  do_check_true(foo.contractID == contractID + "1");

  // try to create another component which doesn't directly implement QI
  do_check_true((contractID + "2") in Components.classes);
  var bar = Components.classes[contractID + "2"]
                      .createInstance(Components.interfaces.nsIClassInfo);
  do_check_true(Boolean(bar));
  do_check_true(bar.contractID == contractID + "2");
}
