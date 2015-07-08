// |reftest| skip-if(!this.hasOwnProperty("SIMD"))

/*
 * Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/
 */

var float32x4 = SIMD.float32x4;
var float64x2 = SIMD.float64x2;
var int8x16 = SIMD.int8x16;
var int16x8 = SIMD.int16x8;
var int32x4 = SIMD.int32x4;

var fround = Math.fround;

function boolToSimdLogical(b) {
    return b ? 0xFFFFFFFF | 0 : 0x0;
}

function testEqualFloat32x4(v, w) {
    testBinaryCompare(v, w, float32x4.equal, (x, y) => boolToSimdLogical(fround(x) == fround(y)), int32x4);
}
function testNotEqualFloat32x4(v, w) {
    testBinaryCompare(v, w, float32x4.notEqual, (x, y) => boolToSimdLogical(fround(x) != fround(y)), int32x4);
}
function testLessThanFloat32x4(v, w) {
    testBinaryCompare(v, w, float32x4.lessThan, (x, y) => boolToSimdLogical(fround(x) < fround(y)), int32x4);
}
function testLessThanOrEqualFloat32x4(v, w) {
    testBinaryCompare(v, w, float32x4.lessThanOrEqual, (x, y) => boolToSimdLogical(fround(x) <= fround(y)), int32x4);
}
function testGreaterThanFloat32x4(v, w) {
    testBinaryCompare(v, w, float32x4.greaterThan, (x, y) => boolToSimdLogical(fround(x) > fround(y)), int32x4);
}
function testGreaterThanOrEqualFloat32x4(v, w) {
    testBinaryCompare(v, w, float32x4.greaterThanOrEqual, (x, y) => boolToSimdLogical(fround(x) >= fround(y)), int32x4);
}

function testEqualFloat64x2(v, w) {
    testBinaryCompare(v, w, float64x2.equal, (x, y) => boolToSimdLogical(x == y), int32x4);
}
function testNotEqualFloat64x2(v, w) {
    testBinaryCompare(v, w, float64x2.notEqual, (x, y) => boolToSimdLogical(x != y), int32x4);
}
function testLessThanFloat64x2(v, w) {
    testBinaryCompare(v, w, float64x2.lessThan, (x, y) => boolToSimdLogical(x < y), int32x4);
}
function testLessThanOrEqualFloat64x2(v, w) {
    testBinaryCompare(v, w, float64x2.lessThanOrEqual, (x, y) => boolToSimdLogical(x <= y), int32x4);
}
function testGreaterThanFloat64x2(v, w) {
    testBinaryCompare(v, w, float64x2.greaterThan, (x, y) => boolToSimdLogical(x > y), int32x4);
}
function testGreaterThanOrEqualFloat64x2(v, w) {
    testBinaryCompare(v, w, float64x2.greaterThanOrEqual, (x, y) => boolToSimdLogical(x >= y), int32x4);
}

function testEqualInt8x16(v, w) {
    testBinaryCompare(v, w, int8x16.equal, (x, y) => boolToSimdLogical(x == y), int8x16);
}
function testNotEqualInt8x16(v, w) {
    testBinaryCompare(v, w, int8x16.notEqual, (x, y) => boolToSimdLogical(x != y), int8x16);
}
function testLessThanInt8x16(v, w) {
    testBinaryCompare(v, w, int8x16.lessThan, (x, y) => boolToSimdLogical(x < y), int8x16);
}
function testLessThanOrEqualInt8x16(v, w) {
    testBinaryCompare(v, w, int8x16.lessThanOrEqual, (x, y) => boolToSimdLogical(x <= y), int8x16);
}
function testGreaterThanInt8x16(v, w) {
    testBinaryCompare(v, w, int8x16.greaterThan, (x, y) => boolToSimdLogical(x > y), int8x16);
}
function testGreaterThanOrEqualInt8x16(v, w) {
    testBinaryCompare(v, w, int8x16.greaterThanOrEqual, (x, y) => boolToSimdLogical(x >= y), int8x16);
}

function testEqualInt16x8(v, w) {
    testBinaryCompare(v, w, int16x8.equal, (x, y) => boolToSimdLogical(x == y), int16x8);
}
function testNotEqualInt16x8(v, w) {
    testBinaryCompare(v, w, int16x8.notEqual, (x, y) => boolToSimdLogical(x != y), int16x8);
}
function testLessThanInt16x8(v, w) {
    testBinaryCompare(v, w, int16x8.lessThan, (x, y) => boolToSimdLogical(x < y), int16x8);
}
function testLessThanOrEqualInt16x8(v, w) {
    testBinaryCompare(v, w, int16x8.lessThanOrEqual, (x, y) => boolToSimdLogical(x <= y), int16x8);
}
function testGreaterThanInt16x8(v, w) {
    testBinaryCompare(v, w, int16x8.greaterThan, (x, y) => boolToSimdLogical(x > y), int16x8);
}
function testGreaterThanOrEqualInt16x8(v, w) {
    testBinaryCompare(v, w, int16x8.greaterThanOrEqual, (x, y) => boolToSimdLogical(x >= y), int16x8);
}

function testEqualInt32x4(v, w) {
    testBinaryCompare(v, w, int32x4.equal, (x, y) => boolToSimdLogical(x == y), int32x4);
}
function testNotEqualInt32x4(v, w) {
    testBinaryCompare(v, w, int32x4.notEqual, (x, y) => boolToSimdLogical(x != y), int32x4);
}
function testLessThanInt32x4(v, w) {
    testBinaryCompare(v, w, int32x4.lessThan, (x, y) => boolToSimdLogical(x < y), int32x4);
}
function testLessThanOrEqualInt32x4(v, w) {
    testBinaryCompare(v, w, int32x4.lessThanOrEqual, (x, y) => boolToSimdLogical(x <= y), int32x4);
}
function testGreaterThanInt32x4(v, w) {
    testBinaryCompare(v, w, int32x4.greaterThan, (x, y) => boolToSimdLogical(x > y), int32x4);
}
function testGreaterThanOrEqualInt32x4(v, w) {
    testBinaryCompare(v, w, int32x4.greaterThanOrEqual, (x, y) => boolToSimdLogical(x >= y), int32x4);
}

function test() {
  var float32x4val = [
      float32x4(1, 20, 30, 4),
      float32x4(10, 2, 3, 40),
      float32x4(9.999, 2.1234, 30.4443, 4),
      float32x4(10, 2.1233, 30.4444, 4.0001),
      float32x4(NaN, -Infinity, +Infinity, -0),
      float32x4(+Infinity, NaN, -0, -Infinity),
      float32x4(13.37, 42.42, NaN, 0)
  ];

  var v, w;
  for (v of float32x4val) {
      for (w of float32x4val) {
          testEqualFloat32x4(v, w);
          testNotEqualFloat32x4(v, w);
          testLessThanFloat32x4(v, w);
          testLessThanOrEqualFloat32x4(v, w);
          testGreaterThanFloat32x4(v, w);
          testGreaterThanOrEqualFloat32x4(v, w);
      }
  }

  var float64x2val = [
      float64x2(1, 20),
      float64x2(10, 2),
      float64x2(9.999, 2.1234),
      float64x2(10, 2.1233),
      float64x2(30.4443, 4),
      float64x2(30.4444, 4.0001),
      float64x2(NaN, -Infinity),
      float64x2(+Infinity, NaN),
      float64x2(+Infinity, -0),
      float64x2(-0, -Infinity),
      float64x2(13.37, 42.42),
      float64x2(NaN, 0)
  ];

  for (v of float64x2val) {
      for (w of float64x2val) {
          testEqualFloat64x2(v, w);
          testNotEqualFloat64x2(v, w);
          testLessThanFloat64x2(v, w);
          testLessThanOrEqualFloat64x2(v, w);
          testGreaterThanFloat64x2(v, w);
          testGreaterThanOrEqualFloat64x2(v, w);
      }
  }

  var int8x16val = [
      int8x16(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16),
      int8x16(-1, -2, -3, -4, -5, -6, -7, -8, -9, -10, -11, -12, -13, -14, -15, -16),
      int8x16(-1, 2, -3, 4, -5, 6, -7, 8, -9, 10, -11, 12, -13, 14, -15, 16),
      int8x16(1, -2, 3, -4, 5, -6, 7, -8, 9, -10, 11, -12, 13, -14, 15, -16),
      int8x16(INT8_MAX, INT8_MAX, INT8_MIN, INT8_MIN, INT8_MIN + 1, INT8_MAX - 1, -7, -8, -9, -10, -11, -12, -13, -14, -15, -16),
      int8x16(INT8_MAX, INT8_MIN, INT8_MAX, INT8_MIN, INT8_MAX - 1, INT8_MIN + 1, 7, 8, 9, 10, 11, 12, 13, 14, 15, -16)
  ];

  for (v of int8x16val) {
      for (w of int8x16val) {
          testEqualInt8x16(v, w);
          testNotEqualInt8x16(v, w);
          testLessThanInt8x16(v, w);
          testLessThanOrEqualInt8x16(v, w);
          testGreaterThanInt8x16(v, w);
          testGreaterThanOrEqualInt8x16(v, w);
      }
  }

  var int16x8val = [
      int16x8(1, 2, 3, 4, 5, 6, 7, 8),
      int16x8(-1, -2, -3, -4, -5, -6, -7, -8),
      int16x8(-1, 2, -3, 4, -5, 6, -7, 8),
      int16x8(1, -2, 3, -4, 5, -6, 7, -8),
      int16x8(INT16_MAX, INT16_MAX, INT16_MIN, INT16_MIN, INT16_MIN + 1, INT16_MAX - 1, -7, -8),
      int16x8(INT16_MAX, INT16_MIN, INT16_MAX, INT16_MIN, INT16_MAX - 1, INT16_MIN + 1, 7, -8)
  ];

  for (v of int16x8val) {
      for (w of int16x8val) {
          testEqualInt16x8(v, w);
          testNotEqualInt16x8(v, w);
          testLessThanInt16x8(v, w);
          testLessThanOrEqualInt16x8(v, w);
          testGreaterThanInt16x8(v, w);
          testGreaterThanOrEqualInt16x8(v, w);
      }
  }

  var int32x4val = [
      int32x4(1, 2, 3, 4),
      int32x4(-1, -2, -3, -4),
      int32x4(-1, 2, -3, 4),
      int32x4(1, -2, 3, -4),
      int32x4(INT32_MAX, INT32_MAX, INT32_MIN, INT32_MIN),
      int32x4(INT32_MAX, INT32_MIN, INT32_MAX, INT32_MIN)
  ];

  for (v of int32x4val) {
      for (w of int32x4val) {
          testEqualInt32x4(v, w);
          testNotEqualInt32x4(v, w);
          testLessThanInt32x4(v, w);
          testLessThanOrEqualInt32x4(v, w);
          testGreaterThanInt32x4(v, w);
          testGreaterThanOrEqualInt32x4(v, w);
      }
  }

  if (typeof reportCompare === "function")
    reportCompare(true, true);
}

test();
