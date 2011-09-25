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

  // Helper test function - takes the name of test method and two values of
  // the given type.
  //
  // The optional comparator argument can be used for alternative notions of
  // equality. The comparator should return true on equality.
  function doTest(name, val1, val2, comparator) {
    var a = val1;
    var b = {value: val2};
    var rv = o[name].call(o, a, b);
    if (comparator) {
      do_check_true(comparator(rv, val2));
      do_check_true(comparator(val1, b.value));
    }
    else {
      do_check_eq(rv, val2);
      do_check_eq(val1, b.value);
    }
  };
  var fuzzComparator = function(a,b) {return Math.abs(a - b) < 0.1;};

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
  // TODO - Test nsIID in bug 687662
  doTestWorkaround("testDOMString", "Beware: ☠ s");
  doTestWorkaround("testAString", "Frosty the ☃ ;-)");
  doTestWorkaround("testAUTF8String", "We deliver 〠!");
  doTestWorkaround("testACString", "Just a regular C string.");

  // TODO: Uncomment when bug 604196 is fixed.
  // doTest("testJsval", {aprop: 12, bprop: "str"}, 4.22);
}
