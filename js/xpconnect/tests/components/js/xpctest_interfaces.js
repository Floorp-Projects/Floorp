/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

function TestInterfaceA() {}
TestInterfaceA.prototype = {

  /* Boilerplate */
  QueryInterface: ChromeUtils.generateQI([Ci["nsIXPCTestInterfaceA"]]),
  contractID: "@mozilla.org/js/xpc/test/js/TestInterfaceA;1",
  classID: Components.ID("{3c8fd2f5-970c-42c6-b5dd-cda1c16dcfd8}"),

  /* nsIXPCTestInterfaceA */
  name: "TestInterfaceADefaultName"
};

function TestInterfaceB() {}
TestInterfaceB.prototype = {

  /* Boilerplate */
  QueryInterface: ChromeUtils.generateQI([Ci["nsIXPCTestInterfaceB"]]),
  contractID: "@mozilla.org/js/xpc/test/js/TestInterfaceB;1",
  classID: Components.ID("{ff528c3a-2410-46de-acaa-449aa6403a33}"),

  /* nsIXPCTestInterfaceA */
  name: "TestInterfaceADefaultName"
};

function TestInterfaceAll() {}
TestInterfaceAll.prototype = {

  /* Boilerplate */
  QueryInterface: ChromeUtils.generateQI([Ci["nsIXPCTestInterfaceA"],
                                          Ci["nsIXPCTestInterfaceB"],
                                          Ci["nsIXPCTestInterfaceC"]]),
  contractID: "@mozilla.org/js/xpc/test/js/TestInterfaceAll;1",
  classID: Components.ID("{90ec5c9e-f6da-406b-9a38-14d00f59db76}"),

  /* nsIXPCTestInterfaceA / nsIXPCTestInterfaceB */
  name: "TestInterfaceAllDefaultName",

  /* nsIXPCTestInterfaceC */
  someInteger: 42
};

var NSGetFactory = XPCOMUtils.generateNSGetFactory([TestInterfaceA, TestInterfaceB, TestInterfaceAll]);

