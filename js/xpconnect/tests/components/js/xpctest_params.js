/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
const {ComponentUtils} = ChromeUtils.import("resource://gre/modules/ComponentUtils.jsm");

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
  QueryInterface: ChromeUtils.generateQI(["nsIXPCTestParams"]),
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
  testAString: f,
  testAUTF8String: f,
  testACString: f,
  testJsval: f,
  testShortSequence: f,
  testDoubleSequence: f,
  testAStringSequence: f,
  testACStringSequence: f,
  testInterfaceSequence: f,
  testJsvalSequence: f,
  testInterfaceIsSequence: f_is,
  testOptionalSequence: function (arr) { return arr; },
  testShortArray: f_is,
  testDoubleArray: f_is,
  testStringArray: f_is,
  testByteArrayOptionalLength(arr) { return arr.length; },
  testWstringArray: f_is,
  testInterfaceArray: f_is,
  testJsvalArray: f_is,
  testSizedString: f_is,
  testSizedWstring: f_is,
  testInterfaceIs: f_is,
  testInterfaceIsArray: f_size_and_iid,
  testOutAString: function(o) { o.value = "out"; },
  testStringArrayOptionalSize: function(arr, size) {
    if (arr.length != size) { throw "bad size passed to test method"; }
    var rv = "";
    arr.forEach((x) => rv += x);
    return rv;
  },
  testOmittedOptionalOut(o) {
    if (typeof o != "object" || o.value !== undefined) {
      throw new Components.Exception(
        "unexpected value",
        Cr.NS_ERROR_ILLEGAL_VALUE
      );
    }
    o.value = Cc["@mozilla.org/network/io-service;1"]
      .getService(Ci.nsIIOService)
      .newURI("http://example.com/");
  }
};

this.NSGetFactory = ComponentUtils.generateNSGetFactory([TestParams]);
