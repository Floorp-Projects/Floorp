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

const Cc = Components.classes;
const Ci = Components.interfaces;

function run_test() {

  // Load the component manifests.
  Components.manager.autoRegister(do_get_file('../components/native/xpctest.manifest'));
  Components.manager.autoRegister(do_get_file('../components/js/xpctest.manifest'));

  // Test for each component.
  test_component("@mozilla.org/js/xpc/test/native/Params;1");
  test_component("@mozilla.org/js/xpc/test/js/Params;1");
}

function test_component(contractid) {

  // Instantiate the object.
  var o = Cc[contractid].createInstance(Ci["nsIXPCTestParams"]);

  // Possible comparator functions.
  var standardComparator = function(a,b) {return a == b;};
  var dotEqualsComparator = function(a,b) {return a.equals(b); }
  var fuzzComparator = function(a,b) {return Math.abs(a - b) < 0.1;};
  var interfaceComparator = function(a,b) {return a.name == b.name; }
  var arrayComparator = function(innerComparator) {
    return function(a,b) {
      if (a.length != b.length)
        return false;
      for (var i = 0; i < a.length; ++i)
        if (!innerComparator(a[i], b[i]))
          return false;
      return true;
    };
  };

  // Helper test function - takes the name of test method and two values of
  // the given type.
  //
  // The optional comparator argument can be used for alternative notions of
  // equality. The comparator should return true on equality.
  function doTest(name, val1, val2, comparator) {
    if (!comparator)
      comparator = standardComparator;
    var a = val1;
    var b = {value: val2};
    var rv = o[name].call(o, a, b);
    do_check_true(comparator(rv, val2));
    do_check_true(comparator(val1, b.value));
  };

  function doIsTest(name, val1, val1Is, val2, val2Is, valComparator, isComparator) {
    if (!isComparator)
      isComparator = standardComparator;
    var a = val1;
    var aIs = val1Is;
    var b = {value: val2};
    var bIs = {value: val2Is};
    var rvIs = {};
    var rv = o[name].call(o, aIs, a, bIs, b, rvIs);
    do_check_true(valComparator(rv, val2));
    do_check_true(isComparator(rvIs.value, val2Is));
    do_check_true(valComparator(val1, b.value));
    do_check_true(isComparator(val1Is, bIs.value));
  }

  // Special-purpose function for testing arrays of iid_is interfaces, where we
  // have 2 distinct sets of dependent parameters.
  function doIs2Test(name, val1, val1Size, val1IID, val2, val2Size, val2IID) {
    var a = val1;
    var aSize = val1Size;
    var aIID = val1IID;
    var b = {value: val2};
    var bSize = {value: val2Size};
    var bIID = {value: val2IID};
    var rvSize = {};
    var rvIID = {};
    var rv = o[name].call(o, aSize, aIID, a, bSize, bIID, b, rvSize, rvIID);
    do_check_true(arrayComparator(interfaceComparator)(rv, val2));
    do_check_true(standardComparator(rvSize.value, val2Size));
    do_check_true(dotEqualsComparator(rvIID.value, val2IID));
    do_check_true(arrayComparator(interfaceComparator)(val1, b.value));
    do_check_true(standardComparator(val1Size, bSize.value));
    do_check_true(dotEqualsComparator(val1IID, bIID.value));
  }

  // Workaround for bug 687612 (inout parameters broken for dipper types).
  // We do a simple test of copying a into b, and ignore the rv.
  function doTestWorkaround(name, val1) {
    var a = val1;
    var b = {value: ""};
    o[name].call(o, a, b);
    do_check_eq(val1, b.value);
  }

  // Test all the different types
  doTest("testBoolean", true, false);
  doTest("testOctet", 4, 156);
  doTest("testShort", -456, 1299);
  doTest("testLong", 50060, -12121212);
  doTest("testLongLong", 12345, -10000000000);
  doTest("testUnsignedShort", 1532, 65000);
  doTest("testUnsignedLong", 0, 4000000000);
  doTest("testUnsignedLongLong", 215435, 3453492580348535809);
  doTest("testFloat", 4.9, -11.2, fuzzComparator);
  doTest("testDouble", -80.5, 15000.2, fuzzComparator);
  doTest("testChar", "a", "2");
  doTest("testString", "someString", "another string");
  // TODO: Fix bug 687679 and use the second argument listed below
  doTest("testWchar", "z", "q");// "ア");
  doTestWorkaround("testDOMString", "Beware: ☠ s");
  doTestWorkaround("testAString", "Frosty the ☃ ;-)");
  doTestWorkaround("testAUTF8String", "We deliver 〠!");
  doTestWorkaround("testACString", "Just a regular C string.");
  doTest("testJsval", {aprop: 12, bprop: "str"}, 4.22);

  // Helpers to instantiate various test XPCOM objects.
  var numAsMade = 0;
  function makeA() {
    var a = Cc["@mozilla.org/js/xpc/test/js/InterfaceA;1"].createInstance(Ci['nsIXPCTestInterfaceA']);
    a.name = 'testA' + numAsMade++;
    return a;
  };
  var numBsMade = 0;
  function makeB() {
    var b = Cc["@mozilla.org/js/xpc/test/js/InterfaceB;1"].createInstance(Ci['nsIXPCTestInterfaceB']);
    b.name = 'testB' + numBsMade++;
    return b;
  };

  // Test arrays.
  doIsTest("testShortArray", [2, 4, 6], 3, [1, 3, 5, 7], 4, arrayComparator(standardComparator));
  doIsTest("testLongLongArray", [-10000000000], 1, [1, 3, 1234511234551], 3, arrayComparator(standardComparator));
  doIsTest("testStringArray", ["mary", "hat", "hey", "lid", "tell", "lam"], 6,
                              ["ids", "fleas", "woes", "wide", "has", "know", "!"], 7, arrayComparator(standardComparator));
  doIsTest("testWstringArray", ["沒有語言", "的偉大嗎?]"], 2,
                               ["we", "are", "being", "sooo", "international", "right", "now"], 7, arrayComparator(standardComparator));
  doIsTest("testInterfaceArray", [makeA(), makeA()], 2,
                                 [makeA(), makeA(), makeA(), makeA(), makeA(), makeA()], 6, arrayComparator(interfaceComparator));

  // Test sized strings.
  var ssTests = ["Tis not possible, I muttered", "give me back my free hardcore!", "quoth the server:", "4〠4"];
  doIsTest("testSizedString", ssTests[0], ssTests[0].length, ssTests[1], ssTests[1].length, standardComparator);
  doIsTest("testSizedWstring", ssTests[2], ssTests[2].length, ssTests[3], ssTests[3].length, standardComparator);

  // Test iid_is.
  doIsTest("testInterfaceIs", makeA(), Ci['nsIXPCTestInterfaceA'],
                              makeB(), Ci['nsIXPCTestInterfaceB'],
                              interfaceComparator, dotEqualsComparator);

  // Test arrays of iids.
  doIs2Test("testInterfaceIsArray", [makeA(), makeA(), makeA(), makeA(), makeA()], 5, Ci['nsIXPCTestInterfaceA'],
                                    [makeB(), makeB(), makeB()], 3, Ci['nsIXPCTestInterfaceB']);
}
