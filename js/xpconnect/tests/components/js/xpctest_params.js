/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
Components.utils.import("resource:///modules/XPCOMUtils.jsm");

function TestParams() {
}

/* For once I'm happy that JS is weakly typed. */
function f(a, b) {
    var rv = b.value;
    b.value = a;
    return rv;
};

/* Implementation for size_is and iid_is methods. */
function f_is(aIs, a, bIs, b, rvIs) {

    // Set up the return value and its 'is' parameter.
    var rv = b.value;
    rvIs.value = bIs.value;

    // Set up b and its 'is' parameter.
    b.value = a;
    bIs.value = aIs;

    return rv;
}

function f_size_and_iid(aSize, aIID, a, bSize, bIID, b, rvSize, rvIID) {

    // Copy the iids.
    rvIID.value = bIID.value;
    bIID.value = aIID;

    // Now that we've reduced the problem to one dependent variable, use f_is.
    return f_is(aSize, a, bSize, b, rvSize);
}

TestParams.prototype = {

  /* Boilerplate */
  QueryInterface: XPCOMUtils.generateQI([Components.interfaces["nsIXPCTestParams"]]),
  contractID: "@mozilla.org/js/xpc/test/js/Params;1",
  classID: Components.ID("{e3b86f4e-49c0-487c-a2b0-3a986720a044}"),

  /* nsIXPCTestParams */
  testBoolean: f,
  testOctet: f,
  testShort: f,
  testLong: f,
  testLongLong: f,
  testUnsignedShort: f,
  testUnsignedLong: f,
  testUnsignedLongLong: f,
  testFloat: f,
  testDouble: f,
  testChar: f,
  testString: f,
  testWchar: f,
  testWstring: f,
  testDOMString: f,
  testAString: f,
  testAUTF8String: f,
  testACString: f,
  testJsval: f,
  testShortArray: f_is,
  testDoubleArray: f_is,
  testStringArray: f_is,
  testWstringArray: f_is,
  testInterfaceArray: f_is,
  testSizedString: f_is,
  testSizedWstring: f_is,
  testInterfaceIs: f_is,
  testInterfaceIsArray: f_size_and_iid,
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([TestParams]);
