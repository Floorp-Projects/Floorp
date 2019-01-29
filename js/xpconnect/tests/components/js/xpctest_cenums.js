/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
const {XPCOMUtils} = ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

function TestCEnums() {
}

TestCEnums.prototype = {
  /* Boilerplate */
  QueryInterface: ChromeUtils.generateQI([Ci["nsIXPCTestCEnums"]]),
  contractID: "@mozilla.org/js/xpc/test/js/CEnums;1",
  classID: Components.ID("{43929c74-dc70-11e8-b6f9-8fce71a2796a}"),

  testCEnumInput: function(input) {
    if (input != Ci.nsIXPCTestCEnums.shouldBe12Explicit)
    {
      throw new Error("Enum values do not match expected value");
    }
  },

  testCEnumOutput: function() {
    return Ci.nsIXPCTestCEnums.shouldBe8Explicit;
  },
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([TestCEnums]);
