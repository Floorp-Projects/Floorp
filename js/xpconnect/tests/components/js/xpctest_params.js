/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is XPConnect Test Code.
 *
 * The Initial Developer of the Original Code is The Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Bobby Holley <bobbyholley@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
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

var NSGetFactory = XPCOMUtils.generateNSGetFactory([TestParams]);
