/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

function TestUtils() {}
TestUtils.prototype = {

  /* Boilerplate */
  QueryInterface: ChromeUtils.generateQI([Ci["nsIXPCTestUtils"]]),
  contractID: "@mozilla.org/js/xpc/test/js/TestUtils;1",
  classID: Components.ID("{e86573c4-a384-441a-8c92-7b99e8575b28}"),
  doubleWrapFunction(fun) { return fun }
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([TestUtils]);
