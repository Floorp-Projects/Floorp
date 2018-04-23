/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

function TestObjectReadWrite() {}
TestObjectReadWrite.prototype = {

  /* Boilerplate */
  QueryInterface: ChromeUtils.generateQI([Ci["nsIXPCTestObjectReadWrite"]]),
  contractID: "@mozilla.org/js/xpc/test/js/ObjectReadWrite;1",
  classID: Components.ID("{8ff41d9c-66e9-4453-924a-7d8de0a5e966}"),

  /* nsIXPCTestObjectReadWrite */
  stringProperty: "XPConnect Read-Writable String",
  booleanProperty: true,
  shortProperty: 32767,
  longProperty: 2147483647,
  floatProperty: 5.5,
  charProperty: "X",
  // timeProperty is PRTime and signed type.
  // So it has to allow negative value.
  timeProperty: -1
};


function TestObjectReadOnly() {}
TestObjectReadOnly.prototype = {

  /* Boilerplate */
  QueryInterface: ChromeUtils.generateQI([Ci["nsIXPCTestObjectReadOnly"]]),
  contractID: "@mozilla.org/js/xpc/test/js/ObjectReadOnly;1",
  classID: Components.ID("{916c4247-253d-4ed0-a425-adfedf53ecc8}"),

  /* nsIXPCTestObjectReadOnly */
  strReadOnly: "XPConnect Read-Only String",
  boolReadOnly: true,
  shortReadOnly: 32767,
  longReadOnly: 2147483647,
  floatReadOnly: 5.5,
  charReadOnly: "X",
  // timeProperty is PRTime and signed type.
  // So it has to allow negative value.
  timeReadOnly: -1
};


this.NSGetFactory = XPCOMUtils.generateNSGetFactory([TestObjectReadWrite, TestObjectReadOnly]);
