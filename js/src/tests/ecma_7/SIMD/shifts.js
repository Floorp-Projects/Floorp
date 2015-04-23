// |reftest| skip-if(!this.hasOwnProperty("SIMD"))

/*
 * Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/
 */

var int32x4 = SIMD.int32x4;

function lsh(a, b) {
    return (b >>> 0) >= 32 ? 0 : (a << b) | 0;
}
function rsh(a, b) {
    return (a >> Math.min(b >>> 0, 31)) | 0;
}
function ursh(a, b) {
    return (b >>> 0) >= 32 ? 0 : (a >>> b) | 0;
}

function test() {
  function TestError() {};

  var good = {valueOf: () => 21};
  var bad = {valueOf: () => {throw new TestError(); }};

  for (var v of [
            int32x4(-1, 2, -3, 4),
            int32x4(INT32_MAX, INT32_MIN, INT32_MAX - 1, INT32_MIN + 1)
       ])
  {
      for (var bits = -2; bits < 36; bits++) {
          testBinaryScalarFunc(v, bits, int32x4.shiftLeftByScalar, lsh);
          testBinaryScalarFunc(v, bits, int32x4.shiftRightArithmeticByScalar, rsh);
          testBinaryScalarFunc(v, bits, int32x4.shiftRightLogicalByScalar, ursh);
      }
      // Test that the shift count is coerced to an int32.
      testBinaryScalarFunc(v, undefined, int32x4.shiftLeftByScalar, lsh);
      testBinaryScalarFunc(v, 3.5, int32x4.shiftLeftByScalar, lsh);
      testBinaryScalarFunc(v, good, int32x4.shiftLeftByScalar, lsh);
  }

  var v = SIMD.int32x4(1,2,3,4);
  assertThrowsInstanceOf(() => SIMD.int32x4.shiftLeftByScalar(v, bad), TestError);
  assertThrowsInstanceOf(() => SIMD.int32x4.shiftRightArithmeticByScalar(v, bad), TestError);
  assertThrowsInstanceOf(() => SIMD.int32x4.shiftRightLogicalByScalar(v, bad), TestError);

  if (typeof reportCompare === "function")
    reportCompare(true, true);
}

test();
