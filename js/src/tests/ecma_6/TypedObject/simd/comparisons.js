// |reftest| skip-if(!this.hasOwnProperty("SIMD"))

/*
 * Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/
 */

var float32x4 = SIMD.float32x4;
var int32x4 = SIMD.int32x4;

var fround = Math.fround;

function boolToSimdLogical(b) {
    return b ? 0xFFFFFFFF | 0 : 0x0;
}

function testEqualFloat32x4(v, w) {
    testBinaryFunc(v, w, float32x4.equal, (x, y) => boolToSimdLogical(fround(x) == fround(y)));
}
function testNotEqualFloat32x4(v, w) {
    testBinaryFunc(v, w, float32x4.notEqual, (x, y) => boolToSimdLogical(fround(x) != fround(y)));
}
function testLessThanFloat32x4(v, w) {
    testBinaryFunc(v, w, float32x4.lessThan, (x, y) => boolToSimdLogical(fround(x) < fround(y)));
}
function testLessThanOrEqualFloat32x4(v, w) {
    testBinaryFunc(v, w, float32x4.lessThanOrEqual, (x, y) => boolToSimdLogical(fround(x) <= fround(y)));
}
function testGreaterThanFloat32x4(v, w) {
    testBinaryFunc(v, w, float32x4.greaterThan, (x, y) => boolToSimdLogical(fround(x) > fround(y)));
}
function testGreaterThanOrEqualFloat32x4(v, w) {
    testBinaryFunc(v, w, float32x4.greaterThanOrEqual, (x, y) => boolToSimdLogical(fround(x) >= fround(y)));
}

function testEqualInt32x4(v, w) {
    testBinaryFunc(v, w, int32x4.equal, (x, y) => boolToSimdLogical(x == y));
}
function testNotEqualInt32x4(v, w) {
    testBinaryFunc(v, w, int32x4.notEqual, (x, y) => boolToSimdLogical(x != y));
}
function testLessThanInt32x4(v, w) {
    testBinaryFunc(v, w, int32x4.lessThan, (x, y) => boolToSimdLogical(x < y));
}
function testLessThanOrEqualInt32x4(v, w) {
    testBinaryFunc(v, w, int32x4.lessThanOrEqual, (x, y) => boolToSimdLogical(x <= y));
}
function testGreaterThanInt32x4(v, w) {
    testBinaryFunc(v, w, int32x4.greaterThan, (x, y) => boolToSimdLogical(x > y));
}
function testGreaterThanOrEqualInt32x4(v, w) {
    testBinaryFunc(v, w, int32x4.greaterThanOrEqual, (x, y) => boolToSimdLogical(x >= y));
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
