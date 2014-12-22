// |reftest| skip-if(!this.hasOwnProperty("SIMD"))

/*
 * Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/
 */

var float32x4 = SIMD.float32x4;
var float64x2 = SIMD.float64x2;

function testMaxFloat32(v, w) {
    return testBinaryFunc(v, w, float32x4.max, (x, y) => Math.fround(Math.max(x, y)), 4);
}
function testMinFloat32(v, w) {
    return testBinaryFunc(v, w, float32x4.min, (x, y) => Math.fround(Math.min(x, y)), 4);
}

function testMaxFloat64(v, w) {
    return testBinaryFunc(v, w, float64x2.max, (x, y) => Math.max(x, y), 2);
}
function testMinFloat64(v, w) {
    return testBinaryFunc(v, w, float64x2.min, (x, y) => Math.min(x, y), 2);
}

function maxNum(x, y) {
    if (x != x)
        return y;
    if (y != y)
        return x;
    return Math.max(x, y);
}

function minNum(x, y) {
    if (x != x)
        return y;
    if (y != y)
        return x;
    return Math.min(x, y);
}

function testMaxNumFloat32(v, w) {
    return testBinaryFunc(v, w, float32x4.maxNum, maxNum, 4);
}
function testMinNumFloat32(v, w) {
    return testBinaryFunc(v, w, float32x4.minNum, minNum, 4);
}

function testMaxNumFloat64(v, w) {
    return testBinaryFunc(v, w, float64x2.maxNum, maxNum, 2);
}
function testMinNumFloat64(v, w) {
    return testBinaryFunc(v, w, float64x2.minNum, minNum, 2);
}

function test() {
  var v, w;
  for ([v, w] of [[float32x4(1, 20, 30, 4), float32x4(10, 2, 3, 40)],
                  [float32x4(9.999, 2.1234, 30.4443, 4), float32x4(10, 2.1233, 30.4444, 4.0001)],
                  [float32x4(NaN, -Infinity, +Infinity, -0), float32x4(13.37, 42.42, NaN, 0)]])
  {
      testMinFloat32(v, w);
      testMaxFloat32(v, w);
      testMinNumFloat32(v, w);
      testMaxNumFloat32(v, w);
  }

  for ([v, w] of [[float64x2(1, 20), float64x2(10, 2)],
                  [float64x2(30, 4), float64x2(3, 40)],
                  [float64x2(9.999, 2.1234), float64x2(10, 2.1233)],
                  [float64x2(30.4443, 4), float64x2(30.4444, 4.0001)],
                  [float64x2(NaN, -Infinity), float64x2(13.37, 42.42)],
                  [float64x2(+Infinity, -0), float64x2(NaN, 0)]])
  {
      testMinFloat64(v, w);
      testMaxFloat64(v, w);
      testMinNumFloat64(v, w);
      testMaxNumFloat64(v, w);
  }

  if (typeof reportCompare === "function")
    reportCompare(true, true);
}

test();

