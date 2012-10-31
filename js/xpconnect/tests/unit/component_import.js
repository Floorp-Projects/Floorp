/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

function FooComponent() {
  this.wrappedJSObject = this;
}
FooComponent.prototype =
{
  // nsIClassInfo + information for XPCOM registration code in XPCOMUtils.jsm
  classDescription:  "Foo Component",
  classID:           Components.ID("{6b933fe6-6eba-4622-ac86-e4f654f1b474}"),
  contractID:       "@mozilla.org/tests/module-importer;1",

  // nsIClassInfo
  implementationLanguage: Components.interfaces.nsIProgrammingLanguage.JAVASCRIPT,
  flags: 0,

  getInterfaces: function getInterfaces(aCount) {
    var interfaces = [Components.interfaces.nsIClassInfo];
    aCount.value = interfaces.length;

    // Guerilla test for line numbers hiding in this method
    var threw = true;
    try {
      thereIsNoSuchIdentifier;
      threw = false;
    } catch (ex) {
      do_check_true(ex.lineNumber == 28);
    }
    do_check_true(threw);
    
    return interfaces;
  },

  getHelperForLanguage: function getHelperForLanguage(aLanguage) {
    return null;
  },

  // nsISupports
  QueryInterface: function QueryInterface(aIID) {
    if (aIID.equals(Components.interfaces.nsIClassInfo) ||
        aIID.equals(Components.interfaces.nsISupports))
      return this;

    throw Components.results.NS_ERROR_NO_INTERFACE;
  }
};

function BarComponent() {
}
BarComponent.prototype =
{
  // nsIClassInfo + information for XPCOM registration code in XPCOMUtils.jsm
  classDescription: "Module importer test 2",
  classID: Components.ID("{708a896a-b48d-4bff-906e-fc2fd134b296}"),
  contractID: "@mozilla.org/tests/module-importer;2",

  // nsIClassInfo
  implementationLanguage: Components.interfaces.nsIProgrammingLanguage.JAVASCRIPT,
  flags: 0,

  getInterfaces: function getInterfaces(aCount) {
    var interfaces = [Components.interfaces.nsIClassInfo];
    aCount.value = interfaces.length;
    return interfaces;
  },

  getHelperForLanguage: function getHelperForLanguage(aLanguage) {
    return null;
  },

  // nsISupports
  QueryInterface: XPCOMUtils.generateQI([Components.interfaces.nsIClassInfo])
};

function do_check_true(cond, text) {
  // we don't have the test harness' utilities in this scope, so we need this
  // little helper. In the failure case, the exception is propagated to the
  // caller in the main run_test() function, and the test fails.
  if (!cond)
    throw "Failed check: " + text;
}

var gComponentsArray = [FooComponent, BarComponent];
this.NSGetFactory = XPCOMUtils.generateNSGetFactory(gComponentsArray);
