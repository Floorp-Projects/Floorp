/*
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *    Robert Sayre <sayrer@gmail.com> (original author)
 *    Alexander J. Vincent <ajvincent@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
 
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

  // try on a new object using a file URL
  var res = Components.classes["@mozilla.org/network/protocol;1?name=resource"]
                      .getService(Components.interfaces.nsIResProtocolHandler);
  var resURI = res.newURI("resource://gre/modules/XPCOMUtils.jsm", null, null);
  dump("resURI: " + resURI + "\n");
  var filePath = res.resolveURI(resURI);
  do_check_eq(filePath.indexOf("file://"), 0);
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
