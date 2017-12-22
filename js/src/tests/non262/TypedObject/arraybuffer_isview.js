var BUGNUMBER = 896105;
var summary = 'ArrayBuffer.isView';

/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

function runTests() {
  assertEq(ArrayBuffer.isView(), false);
  assertEq(ArrayBuffer.isView(undefined), false);
  assertEq(ArrayBuffer.isView(null), false);
  assertEq(ArrayBuffer.isView("primitive"), false);
  assertEq(ArrayBuffer.isView({}), false);
  assertEq(ArrayBuffer.isView([]), false);
  assertEq(ArrayBuffer.isView(new ArrayBuffer(10)), false);
  assertEq(ArrayBuffer.isView(new Int8Array(10)), true);
  assertEq(ArrayBuffer.isView(new Int8Array(10).subarray(0, 3)), true);
  if (typeof SharedArrayBuffer != "undefined") {
    assertEq(ArrayBuffer.isView(new SharedArrayBuffer(10)), false);
    assertEq(ArrayBuffer.isView(new Int8Array(new SharedArrayBuffer(10))), true);
    // In the next case subarray should return an ArrayBuffer, so this is
    // similar to the subarray test above.
    assertEq(ArrayBuffer.isView(new Int8Array(new SharedArrayBuffer(10)).subarray(0, 3)),
  	     true);
  }

  if (typeof reportCompare !== 'undefined')
    reportCompare(true, true);
  print("Tests complete");
}

runTests();
