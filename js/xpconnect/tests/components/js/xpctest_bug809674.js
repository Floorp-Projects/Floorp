/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

function TestBug809674() {}
TestBug809674.prototype = {

  /* Boilerplate */
  QueryInterface: ChromeUtils.generateQI([Ci["nsIXPCTestBug809674"]]),
  contractID: "@mozilla.org/js/xpc/test/js/Bug809674;1",
  classID: Components.ID("{2df46559-da21-49bf-b863-0d7b7bbcbc73}"),

  /* nsIXPCTestBug809674 */
  jsvalProperty: {},
};


this.NSGetFactory = XPCOMUtils.generateNSGetFactory([TestBug809674]);
