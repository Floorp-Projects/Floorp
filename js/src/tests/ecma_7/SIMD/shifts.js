// |reftest| skip-if(!this.hasOwnProperty("SIMD"))

/*
 * Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/
 */

var Int8x16 = SIMD.Int8x16;
var Int16x8 = SIMD.Int16x8;
var Int32x4 = SIMD.Int32x4;
var Uint8x16 = SIMD.Uint8x16;
var Uint16x8 = SIMD.Uint16x8;
var Uint32x4 = SIMD.Uint32x4;

// Int8 shifts.
function lsh8(a, b) {
    return (b >>> 0) >= 8 ? 0 : (a << b) << 24 >> 24;
}
function rsha8(a, b) {
    return (a >> Math.min(b >>> 0, 7)) << 24 >> 24;
}
function rshl8(a, b) {
    return (b >>> 0) >= 8 ? 0 : (a >>> b) << 24 >> 24;
}

// Int16 shifts.
function lsh16(a, b) {
    return (b >>> 0) >= 16 ? 0 : (a << b) << 16 >> 16;
}
function rsha16(a, b) {
    return (a >> Math.min(b >>> 0, 15)) << 16 >> 16;
}
function rshl16(a, b) {
    return (b >>> 0) >= 16 ? 0 : (a >>> b) << 16 >> 16;
}

// Int32 shifts.
function lsh32(a, b) {
    return (b >>> 0) >= 32 ? 0 : (a << b) | 0;
}
function rsha32(a, b) {
    return (a >> Math.min(b >>> 0, 31)) | 0;
}
function rshl32(a, b) {
    return (b >>> 0) >= 32 ? 0 : (a >>> b) | 0;
}

// Uint8 shifts.
function ulsh8(a, b) {
    return (b >>> 0) >= 8 ? 0 : (a << b) << 24 >>> 24;
}
function ursha8(a, b) {
    return ((a << 24 >> 24) >> Math.min(b >>> 0, 7)) << 24 >>> 24;
}
function urshl8(a, b) {
    return (b >>> 0) >= 8 ? 0 : (a >>> b) << 24 >>> 24;
}

// Uint16 shifts.
function ulsh16(a, b) {
    return (b >>> 0) >= 16 ? 0 : (a << b) << 16 >>> 16;
}
function ursha16(a, b) {
    return ((a << 16 >> 16) >> Math.min(b >>> 0, 15)) << 16 >>> 16;
}
function urshl16(a, b) {
    return (b >>> 0) >= 16 ? 0 : (a >>> b) << 16 >>> 16;
}

// Uint32 shifts.
function ulsh32(a, b) {
    return (b >>> 0) >= 32 ? 0 : (a << b) >>> 0;
}
function ursha32(a, b) {
    return ((a | 0) >> Math.min(b >>> 0, 31)) >>> 0;
}
function urshl32(a, b) {
    return (b >>> 0) >= 32 ? 0 : (a >>> b) >>> 0;
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
          testBinaryScalarFunc(v, bits, Int8x16.shiftRightArithmeticByScalar, rsha8);
          testBinaryScalarFunc(v, bits, Int8x16.shiftRightLogicalByScalar, rshl8);
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
          testBinaryScalarFunc(v, bits, Int16x8.shiftRightArithmeticByScalar, rsha16);
          testBinaryScalarFunc(v, bits, Int16x8.shiftRightLogicalByScalar, rshl16);
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
          testBinaryScalarFunc(v, bits, Int32x4.shiftRightArithmeticByScalar, rsha32);
          testBinaryScalarFunc(v, bits, Int32x4.shiftRightLogicalByScalar, rshl32);
      }
      // Test that the shift count is coerced to an int32.
      testBinaryScalarFunc(v, undefined, Int32x4.shiftLeftByScalar, lsh32);
      testBinaryScalarFunc(v, 3.5, Int32x4.shiftLeftByScalar, lsh32);
      testBinaryScalarFunc(v, good, Int32x4.shiftLeftByScalar, lsh32);
  }

  for (var v of [
            Uint8x16(-1, 2, -3, 4, -5, 6, -7, 8, -9, 10, -11, 12, -13, 14, -15, 16),
            Uint8x16(INT8_MAX, INT8_MIN, INT8_MAX - 1, INT8_MIN + 1, UINT8_MAX, UINT8_MAX - 1)
       ])
  {
      for (var bits = -2; bits < 12; bits++) {
          testBinaryScalarFunc(v, bits, Uint8x16.shiftLeftByScalar, ulsh8);
          testBinaryScalarFunc(v, bits, Uint8x16.shiftRightArithmeticByScalar, ursha8);
          testBinaryScalarFunc(v, bits, Uint8x16.shiftRightLogicalByScalar, urshl8);
      }
      // Test that the shift count is coerced to an int32.
      testBinaryScalarFunc(v, undefined, Uint8x16.shiftLeftByScalar, ulsh8);
      testBinaryScalarFunc(v, 3.5, Uint8x16.shiftLeftByScalar, ulsh8);
      testBinaryScalarFunc(v, good, Uint8x16.shiftLeftByScalar, ulsh8);
  }
  for (var v of [
            Uint16x8(-1, 2, -3, 4, -5, 6, -7, 8),
            Uint16x8(INT16_MAX, INT16_MIN, INT16_MAX - 1, INT16_MIN + 1, UINT16_MAX, UINT16_MAX - 1)
       ])
  {
      for (var bits = -2; bits < 20; bits++) {
          testBinaryScalarFunc(v, bits, Uint16x8.shiftLeftByScalar, ulsh16);
          testBinaryScalarFunc(v, bits, Uint16x8.shiftRightArithmeticByScalar, ursha16);
          testBinaryScalarFunc(v, bits, Uint16x8.shiftRightLogicalByScalar, urshl16);
      }
      // Test that the shift count is coerced to an int32.
      testBinaryScalarFunc(v, undefined, Uint16x8.shiftLeftByScalar, ulsh16);
      testBinaryScalarFunc(v, 3.5, Uint16x8.shiftLeftByScalar, ulsh16);
      testBinaryScalarFunc(v, good, Uint16x8.shiftLeftByScalar, ulsh16);
  }
  for (var v of [
            Uint32x4(-1, 2, -3, 4),
            Uint32x4(UINT32_MAX, UINT32_MAX - 1, 0, 1),
            Uint32x4(INT32_MAX, INT32_MIN, INT32_MAX - 1, INT32_MIN + 1)
       ])
  {
      for (var bits = -2; bits < 36; bits++) {
          testBinaryScalarFunc(v, bits, Uint32x4.shiftLeftByScalar, ulsh32);
          testBinaryScalarFunc(v, bits, Uint32x4.shiftRightArithmeticByScalar, ursha32);
          testBinaryScalarFunc(v, bits, Uint32x4.shiftRightLogicalByScalar, urshl32);
      }
      // Test that the shift count is coerced to an int32.
      testBinaryScalarFunc(v, undefined, Uint32x4.shiftLeftByScalar, ulsh32);
      testBinaryScalarFunc(v, 3.5, Uint32x4.shiftLeftByScalar, ulsh32);
      testBinaryScalarFunc(v, good, Uint32x4.shiftLeftByScalar, ulsh32);
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

  var v = SIMD.Uint8x16(1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16);
  assertThrowsInstanceOf(() => SIMD.Uint8x16.shiftLeftByScalar(v, bad), TestError);
  assertThrowsInstanceOf(() => SIMD.Uint8x16.shiftRightArithmeticByScalar(v, bad), TestError);
  assertThrowsInstanceOf(() => SIMD.Uint8x16.shiftRightLogicalByScalar(v, bad), TestError);

  var v = SIMD.Uint16x8(1,2,3,4,5,6,7,8);
  assertThrowsInstanceOf(() => SIMD.Uint16x8.shiftLeftByScalar(v, bad), TestError);
  assertThrowsInstanceOf(() => SIMD.Uint16x8.shiftRightArithmeticByScalar(v, bad), TestError);
  assertThrowsInstanceOf(() => SIMD.Uint16x8.shiftRightLogicalByScalar(v, bad), TestError);

  var v = SIMD.Uint32x4(1,2,3,4);
  assertThrowsInstanceOf(() => SIMD.Uint32x4.shiftLeftByScalar(v, bad), TestError);
  assertThrowsInstanceOf(() => SIMD.Uint32x4.shiftRightArithmeticByScalar(v, bad), TestError);
  assertThrowsInstanceOf(() => SIMD.Uint32x4.shiftRightLogicalByScalar(v, bad), TestError);

  if (typeof reportCompare === "function")
    reportCompare(true, true);
}

test();
