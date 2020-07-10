/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
const {ComponentUtils} = ChromeUtils.import("resource://gre/modules/ComponentUtils.jsm");

function TestBug809674() {}
TestBug809674.prototype = {

  /* Boilerplate */
  QueryInterface: ChromeUtils.generateQI(["nsIXPCTestBug809674"]),
  contractID: "@mozilla.org/js/xpc/test/js/Bug809674;1",
  classID: Components.ID("{2df46559-da21-49bf-b863-0d7b7bbcbc73}"),

  /* nsIXPCTestBug809674 */
  methodWithOptionalArgc() {},

  addArgs(x, y) {
    return x + y;
  },
  addSubMulArgs(x, y, subOut, mulOut) {
    subOut.value = x - y;
    mulOut.value = x * y;
    return x + y;
  },
  addVals(x, y) {
    return x + y;
  },
  addMany(x1, x2, x3, x4, x5, x6, x7, x8) {
    return x1 + x2 + x3 + x4 + x5 + x6 + x7 + x8;
  },

  methodNoArgs() {
    return 7;
  },
  methodNoArgsNoRetVal() {},

  valProperty: {value: 42},
  uintProperty: 123,
};


this.NSGetFactory = ComponentUtils.generateNSGetFactory([TestBug809674]);
