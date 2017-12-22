// |reftest| skip-if(!this.hasOwnProperty("SIMD"))
var Float64x2 = SIMD.Float64x2;

/*
 * Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/
 */

function add(a, b) { return a + b; }
function sub(a, b) { return a - b; }
function mul(a, b) { return a * b; }
function div(a, b) { return a / b; }
function neg(a) { return -a; }
function reciprocalApproximation(a) { return 1 / a; }
function reciprocalSqrtApproximation(a) { return 1 / Math.sqrt(a); }

function testAdd(v, w) {
    return testBinaryFunc(v, w, Float64x2.add, add);
}
function testSub(v, w) {
    return testBinaryFunc(v, w, Float64x2.sub, sub);
}
function testMul(v, w) {
    return testBinaryFunc(v, w, Float64x2.mul, mul);
}
function testDiv(v, w) {
    return testBinaryFunc(v, w, Float64x2.div, div);
}
function testAbs(v) {
    return testUnaryFunc(v, Float64x2.abs, Math.abs);
}
function testNeg(v) {
    return testUnaryFunc(v, Float64x2.neg, neg);
}
function testReciprocalApproximation(v) {
    return testUnaryFunc(v, Float64x2.reciprocalApproximation, reciprocalApproximation);
}
function testReciprocalSqrtApproximation(v) {
    return testUnaryFunc(v, Float64x2.reciprocalSqrtApproximation, reciprocalSqrtApproximation);
}
function testSqrt(v) {
    return testUnaryFunc(v, Float64x2.sqrt, Math.sqrt);
}

function test() {
  var v, w;
  for ([v, w] of [[Float64x2(1, 2), Float64x2(3, 4)],
                  [Float64x2(1.894, 2.8909), Float64x2(100.764, 200.987)],
                  [Float64x2(-1, -2), Float64x2(-14.54, 57)],
                  [Float64x2(+Infinity, -Infinity), Float64x2(NaN, -0)],
                  [Float64x2(Math.pow(2, 31), Math.pow(2, -31)), Float64x2(Math.pow(2, -1047), Math.pow(2, -149))]])
  {
      testAdd(v, w);
      testSub(v, w);
      testMul(v, w);
      testDiv(v, w);
      testAbs(v);
      testNeg(v);
      testReciprocalApproximation(v);
      testSqrt(v);
      testReciprocalSqrtApproximation(v);
  }

  if (typeof reportCompare === "function")
    reportCompare(true, true);
}

test();

