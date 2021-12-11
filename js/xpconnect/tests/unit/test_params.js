/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function run_test() {

  // Load the component manifests.
  registerXPCTestComponents();
  registerAppManifest(do_get_file('../components/js/xpctest.manifest'));

  // Test for each component.
  test_component("@mozilla.org/js/xpc/test/native/Params;1");
  test_component("@mozilla.org/js/xpc/test/js/Params;1");
}

function test_component(contractid) {

  // Instantiate the object.
  info("Testing " + contractid);
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
    Assert.ok(comparator(rv, val2));
    Assert.ok(comparator(val1, b.value));
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
    Assert.ok(valComparator(rv, val2));
    Assert.ok(isComparator(rvIs.value, val2Is));
    Assert.ok(valComparator(val1, b.value));
    Assert.ok(isComparator(val1Is, bIs.value));
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
    Assert.ok(arrayComparator(interfaceComparator)(rv, val2));
    Assert.ok(standardComparator(rvSize.value, val2Size));
    Assert.ok(dotEqualsComparator(rvIID.value, val2IID));
    Assert.ok(arrayComparator(interfaceComparator)(val1, b.value));
    Assert.ok(standardComparator(val1Size, bSize.value));
    Assert.ok(dotEqualsComparator(val1IID, bIID.value));
  }

  // Check that the given call (type mismatch) results in an exception being thrown.
  function doTypedArrayMismatchTest(name, val1, val1Size, val2, val2Size) {
    var comparator = arrayComparator(standardComparator);
    var error = false;
    try {
      doIsTest(name, val1, val1Size, val2, val2Size, comparator);
      
      // An exception was not thrown as would have been expected.
      Assert.ok(false);
    }
    catch (e) {
      // An exception was thrown as expected.
      Assert.ok(true);
    }
  }

  // Workaround for bug 687612 (inout parameters broken for dipper types).
  // We do a simple test of copying a into b, and ignore the rv.
  function doTestWorkaround(name, val1) {
    var a = val1;
    var b = {value: ""};
    o[name].call(o, a, b);
    Assert.equal(val1, b.value);
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
  doTest("testWstring", "Why wasnt this", "turned on before? ಠ_ಠ");
  doTest("testWchar", "z", "ア");
  doTestWorkaround("testAString", "Frosty the ☃ ;-)");
  doTestWorkaround("testAUTF8String", "We deliver 〠!");
  doTestWorkaround("testACString", "Just a regular C string.");
  doTest("testJsval", {aprop: 12, bprop: "str"}, 4.22);

  // Test out dipper parameters, since they're special and we can't really test
  // inouts.
  let outAString = {};
  o.testOutAString(outAString);
  Assert.equal(outAString.value, "out");
  try { o.testOutAString(undefined); } catch (e) {} // Don't crash
  try { o.testOutAString(null); } catch (e) {} // Don't crash
  try { o.testOutAString("string"); } catch (e) {} // Don't crash

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
  doIsTest("testDoubleArray", [-10, -0.5], 2, [1, 3, 1e11, -8e-5 ], 4, arrayComparator(fuzzComparator));

  doIsTest("testStringArray", ["mary", "hat", "hey", "lid", "tell", "lam"], 6,
                              ["ids", "fleas", "woes", "wide", "has", "know", "!"], 7, arrayComparator(standardComparator));
  doIsTest("testWstringArray", ["沒有語言", "的偉大嗎?]"], 2,
                               ["we", "are", "being", "sooo", "international", "right", "now"], 7, arrayComparator(standardComparator));
  doIsTest("testInterfaceArray", [makeA(), makeA()], 2,
                                 [makeA(), makeA(), makeA(), makeA(), makeA(), makeA()], 6, arrayComparator(interfaceComparator));
  doIsTest("testJsvalArray", [{ cheese: 'whiz', apple: 8 }, [1, 5, '3'], /regex/], 3,
                             ['apple', 2.2e10, 3.3e30, { only: "wheedle", except: {} }], 4, arrayComparator(standardComparator));

  // Test typed arrays and ArrayBuffer aliasing.
  var arrayBuffer = new ArrayBuffer(16);
  var int16Array = new Int16Array(arrayBuffer, 2, 3);
  int16Array.set([-32768, 0, 32767]);
  doIsTest("testShortArray", int16Array, 3, new Int16Array([1773, -32768, 32767, 7]), 4, arrayComparator(standardComparator));
  doIsTest("testDoubleArray", new Float64Array([-10, -0.5]), 2, new Float64Array([0, 3.2, 1.0e10, -8.33 ]), 4, arrayComparator(fuzzComparator));

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

  // Test optional array size.
  Assert.equal(o.testStringArrayOptionalSize(["some", "string", "array"]), "somestringarray");

  // Test incorrect (too big) array size parameter; this should throw NOT_ENOUGH_ELEMENTS.
  doTypedArrayMismatchTest("testShortArray", new Int16Array([-3, 7, 4]), 4,
                                             new Int16Array([1, -32, 6]), 3);

  // Test type mismatch (int16 <-> uint16); this should throw BAD_CONVERT_JS.
  doTypedArrayMismatchTest("testShortArray", new Uint16Array([0, 7, 4, 3]), 4,
                                             new Uint16Array([1, 5, 6]), 3);

  // Test Sequence<T> types.
  doTest("testShortSequence", [2, 4, 6], [1, 3, 5, 7], arrayComparator(standardComparator));
  doTest("testDoubleSequence", [-10, -0.5], [1, 3, 1e11, -8e-5 ], arrayComparator(fuzzComparator));
  doTest("testACStringSequence", ["mary", "hat", "hey", "lid", "tell", "lam"],
                                 ["ids", "fleas", "woes", "wide", "has", "know", "!"],
                                 arrayComparator(standardComparator));
  doTest("testAStringSequence", ["沒有語言", "的偉大嗎?]"],
                                ["we", "are", "being", "sooo", "international", "right", "now"],
                                arrayComparator(standardComparator));

  doTest("testInterfaceSequence", [makeA(), makeA()],
                                  [makeA(), makeA(), makeA(), makeA(), makeA(), makeA()], arrayComparator(interfaceComparator));

  doTest("testJsvalSequence", [{ cheese: 'whiz', apple: 8 }, [1, 5, '3'], /regex/],
                              ['apple', 2.2e10, 3.3e30, { only: "wheedle", except: {} }], arrayComparator(standardComparator));

  doIsTest("testInterfaceIsSequence", [makeA(), makeA(), makeA(), makeA(), makeA()], Ci['nsIXPCTestInterfaceA'],
                                      [makeB(), makeB(), makeB()], Ci['nsIXPCTestInterfaceB'],
                                      arrayComparator(interfaceComparator), dotEqualsComparator);

  var ret = o.testOptionalSequence();
  Assert.ok(Array.isArray(ret));
  Assert.equal(ret.length, 0);

  ret = o.testOptionalSequence([]);
  Assert.ok(Array.isArray(ret));
  Assert.equal(ret.length, 0);

  ret = o.testOptionalSequence([1, 2, 3]);
  Assert.ok(Array.isArray(ret));
  Assert.equal(ret.length, 3);

  o.testOmittedOptionalOut();
  ret = {};
  o.testOmittedOptionalOut(ret);
  Assert.equal(ret.value.spec, "http://example.com/")

  // Tests for large ArrayBuffers.
  var ab = null;
  try {
    ab = new ArrayBuffer(4.5 * 1024 * 1024 * 1024); // 4.5 GB.
  } catch (e) {
    // Large ArrayBuffers not available (32-bit or disabled).
  }
  if (ab) {
    var uint8 = new Uint8Array(ab);

    // Test length check in JSArray2Native.
    var ex = null;
    try {
      o.testOptionalSequence(uint8);
    } catch (e) {
      ex = e;
    }
    Assert.ok(ex.message.includes("Could not convert JavaScript argument arg 0"));

    // Test length check for optional length argument in GetArraySizeFromParam.
    ex = null;
    try {
      o.testByteArrayOptionalLength(uint8);
    } catch (e) {
      ex = e;
    }
    Assert.ok(ex.message.includes("Cannot convert JavaScript object into an array"));

    // Smaller array views on the buffer are fine.
    uint8 = new Uint8Array(ab, ab.byteLength - 3);
    uint8[0] = 123;
    Assert.equal(uint8.byteLength, 3);
    Assert.equal(o.testOptionalSequence(uint8).toString(), "123,0,0");
    Assert.equal(o.testByteArrayOptionalLength(uint8), 3);
  }
}
