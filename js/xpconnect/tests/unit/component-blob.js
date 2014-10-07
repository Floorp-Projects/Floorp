/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");
Components.utils.importGlobalProperties(['Blob']);

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

function do_check_true(cond, text) {
  // we don't have the test harness' utilities in this scope, so we need this
  // little helper. In the failure case, the exception is propagated to the
  // caller in the main run_test() function, and the test fails.
  if (!cond)
    throw "Failed check: " + text;
}

function BlobComponent() {
  this.wrappedJSObject = this;
}
BlobComponent.prototype =
{
  doTest: function() {
    // throw if anything goes wrong
    let testContent = "<a id=\"a\"><b id=\"b\">hey!<\/b><\/a>";
    // should be able to construct a file
    var f1 = new Blob([testContent], {"type" : "text/xml"});

    // do some tests
    do_check_true(f1 instanceof Ci.nsIDOMBlob, "Should be a DOM Blob");

    do_check_true(!(f1 instanceof Ci.nsIDOMFile), "Should not be a DOM File");

    do_check_true(f1.type == "text/xml", "Wrong type");

    do_check_true(f1.size == testContent.length, "Wrong content size");

    var f2 = new Blob();
    do_check_true(f2.size == 0, "Wrong size");
    do_check_true(f2.type == "", "Wrong type");

    var threw = false;
    try {
      // Needs a valid ctor argument
      var f2 = new Blob(Date(132131532));
    } catch (e) {
      threw = true;
    }
    do_check_true(threw, "Passing a random object should fail");

    return true;
  },

  // nsIClassInfo + information for XPCOM registration code in XPCOMUtils.jsm
  classDescription: "Blob in components scope code",
  classID: Components.ID("{06215993-a3c2-41e3-bdfd-0a3a2cc0b65c}"),
  contractID: "@mozilla.org/tests/component-blob;1",

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

var gComponentsArray = [BlobComponent];
this.NSGetFactory = XPCOMUtils.generateNSGetFactory(gComponentsArray);
