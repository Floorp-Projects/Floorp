/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const {ComponentUtils} = ChromeUtils.import("resource://gre/modules/ComponentUtils.jsm");
Cu.importGlobalProperties(['Blob', 'File']);

const Assert = {
  ok(cond, text) {
    // we don't have the test harness' utilities in this scope, so we need this
    // little helper. In the failure case, the exception is propagated to the
    // caller in the main run_test() function, and the test fails.
    if (!cond)
      throw "Failed check: " + text;
  }
};

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
    Assert.ok(f1 instanceof Blob, "Should be a DOM Blob");

    Assert.ok(!(f1 instanceof File), "Should not be a DOM File");

    Assert.ok(f1.type == "text/xml", "Wrong type");

    Assert.ok(f1.size == testContent.length, "Wrong content size");

    var f2 = new Blob();
    Assert.ok(f2.size == 0, "Wrong size");
    Assert.ok(f2.type == "", "Wrong type");

    var threw = false;
    try {
      // Needs a valid ctor argument
      var f2 = new Blob(Date(132131532));
    } catch (e) {
      threw = true;
    }
    Assert.ok(threw, "Passing a random object should fail");

    return true;
  },

  // nsIClassInfo + information for XPCOM registration code in
  // ComponentUtils.jsm
  classDescription: "Blob in components scope code",
  classID: Components.ID("{06215993-a3c2-41e3-bdfd-0a3a2cc0b65c}"),
  contractID: "@mozilla.org/tests/component-blob;1",

  // nsIClassInfo
  flags: 0,

  interfaces: [Ci.nsIClassInfo],

  getScriptableHelper: function getScriptableHelper() {
    return null;
  },

  // nsISupports
  QueryInterface: ChromeUtils.generateQI(["nsIClassInfo"])
};

var gComponentsArray = [BlobComponent];
this.NSGetFactory = ComponentUtils.generateNSGetFactory(gComponentsArray);
