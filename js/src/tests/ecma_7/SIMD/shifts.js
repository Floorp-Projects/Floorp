// |reftest| skip-if(!this.hasOwnProperty("SIMD"))

/*
 * Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/
 */

var Int8x16 = SIMD.Int8x16;
var Int16x8 = SIMD.Int16x8;
var Int32x4 = SIMD.Int32x4;

function lsh8(a, b) {
    return (b >>> 0) >= 8 ? 0 : (a << b) << 24 >> 24;
}
function rsh8(a, b) {
    return (a >> Math.min(b >>> 0, 7)) << 24 >> 24;
}
function ursh8(a, b) {
    return (b >>> 0) >= 8 ? 0 : (a >>> b) << 24 >> 24;
}

function lsh16(a, b) {
    return (b >>> 0) >= 16 ? 0 : (a << b) << 16 >> 16;
}
function rsh16(a, b) {
    return (a >> Math.min(b >>> 0, 15)) << 16 >> 16;
}
function ursh16(a, b) {
    return (b >>> 0) >= 16 ? 0 : (a >>> b) << 16 >> 16;
}

function lsh32(a, b) {
    return (b >>> 0) >= 32 ? 0 : (a << b) | 0;
}
function rsh32(a, b) {
    return (a >> Math.min(b >>> 0, 31)) | 0;
}
function ursh32(a, b) {
    return (b >>> 0) >= 32 ? 0 : (a >>> b) | 0;
}

function test() {
  function TestError() {};

  var good = {valueOf: () => 21};
  var bad = {valueOf: () => {throw new TestError(); }};

  for (var v of [
            Int8x16(-1, 2, -3, 4, -5, 6, -7, 8, -9, 10, -11, 12, -13, 14, -15, 16),
            Int8x16(INT8_MAX, INT8_MIN, INT8_MAX - 1, INT8_MIN + 1)
       ])
  {
      for (var bits = -2; bits < 12; bits++) {
          testBinaryScalarFunc(v, bits, Int8x16.shiftLeftByScalar, lsh8);
          testBinaryScalarFunc(v, bits, Int8x16.shiftRightArithmeticByScalar, rsh8);
          testBinaryScalarFunc(v, bits, Int8x16.shiftRightLogicalByScalar, ursh8);
      }
      // Test that the shift count is coerced to an int32.
      testBinaryScalarFunc(v, undefined, Int8x16.shiftLeftByScalar, lsh8);
      testBinaryScalarFunc(v, 3.5, Int8x16.shiftLeftByScalar, lsh8);
      testBinaryScalarFunc(v, good, Int8x16.shiftLeftByScalar, lsh8);
  }
  for (var v of [
            Int16x8(-1, 2, -3, 4, -5, 6, -7, 8),
            Int16x8(INT16_MAX, INT16_MIN, INT16_MAX - 1, INT16_MIN + 1)
       ])
  {
      for (var bits = -2; bits < 20; bits++) {
          testBinaryScalarFunc(v, bits, Int16x8.shiftLeftByScalar, lsh16);
          testBinaryScalarFunc(v, bits, Int16x8.shiftRightArithmeticByScalar, rsh16);
          testBinaryScalarFunc(v, bits, Int16x8.shiftRightLogicalByScalar, ursh16);
      }
      // Test that the shift count is coerced to an int32.
      testBinaryScalarFunc(v, undefined, Int16x8.shiftLeftByScalar, lsh16);
      testBinaryScalarFunc(v, 3.5, Int16x8.shiftLeftByScalar, lsh16);
      testBinaryScalarFunc(v, good, Int16x8.shiftLeftByScalar, lsh16);
  }
  for (var v of [
            Int32x4(-1, 2, -3, 4),
            Int32x4(INT32_MAX, INT32_MIN, INT32_MAX - 1, INT32_MIN + 1)
       ])
  {
      for (var bits = -2; bits < 36; bits++) {
          testBinaryScalarFunc(v, bits, Int32x4.shiftLeftByScalar, lsh32);
          testBinaryScalarFunc(v, bits, Int32x4.shiftRightArithmeticByScalar, rsh32);
          testBinaryScalarFunc(v, bits, Int32x4.shiftRightLogicalByScalar, ursh32);
      }
      // Test that the shift count is coerced to an int32.
      testBinaryScalarFunc(v, undefined, Int32x4.shiftLeftByScalar, lsh32);
      testBinaryScalarFunc(v, 3.5, Int32x4.shiftLeftByScalar, lsh32);
      testBinaryScalarFunc(v, good, Int32x4.shiftLeftByScalar, lsh32);
  }

  var v = SIMD.Int8x16(1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16);
  assertThrowsInstanceOf(() => SIMD.Int8x16.shiftLeftByScalar(v, bad), TestError);
  assertThrowsInstanceOf(() => SIMD.Int8x16.shiftRightArithmeticByScalar(v, bad), TestError);
  assertThrowsInstanceOf(() => SIMD.Int8x16.shiftRightLogicalByScalar(v, bad), TestError);

  var v = SIMD.Int16x8(1,2,3,4,5,6,7,8);
  assertThrowsInstanceOf(() => SIMD.Int16x8.shiftLeftByScalar(v, bad), TestError);
  assertThrowsInstanceOf(() => SIMD.Int16x8.shiftRightArithmeticByScalar(v, bad), TestError);
  assertThrowsInstanceOf(() => SIMD.Int16x8.shiftRightLogicalByScalar(v, bad), TestError);

  var v = SIMD.Int32x4(1,2,3,4);
  assertThrowsInstanceOf(() => SIMD.Int32x4.shiftLeftByScalar(v, bad), TestError);
  assertThrowsInstanceOf(() => SIMD.Int32x4.shiftRightArithmeticByScalar(v, bad), TestError);
  assertThrowsInstanceOf(() => SIMD.Int32x4.shiftRightLogicalByScalar(v, bad), TestError);

  if (typeof reportCompare === "function")
    reportCompare(true, true);
}

test();
