/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
Components.utils.import("resource:///modules/XPCOMUtils.jsm");

function TestInterfaceA() {}
TestInterfaceA.prototype = {

  /* Boilerplate */
  QueryInterface: XPCOMUtils.generateQI([Components.interfaces["nsIXPCTestInterfaceA"]]),
  contractID: "@mozilla.org/js/xpc/test/js/TestInterfaceA;1",
  classID: Components.ID("{3c8fd2f5-970c-42c6-b5dd-cda1c16dcfd8}"),

  /* nsIXPCTestInterfaceA */
  name: "TestInterfaceADefaultName"
};

function TestInterfaceB() {}
TestInterfaceB.prototype = {

  /* Boilerplate */
  QueryInterface: XPCOMUtils.generateQI([Components.interfaces["nsIXPCTestInterfaceB"]]),
  contractID: "@mozilla.org/js/xpc/test/js/TestInterfaceB;1",
  classID: Components.ID("{ff528c3a-2410-46de-acaa-449aa6403a33}"),

  /* nsIXPCTestInterfaceA */
  name: "TestInterfaceADefaultName"
};


var NSGetFactory = XPCOMUtils.generateNSGetFactory([TestInterfaceA, TestInterfaceB]);
