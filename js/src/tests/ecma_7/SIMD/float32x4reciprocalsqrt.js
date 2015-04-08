// |reftest| skip-if(!this.hasOwnProperty("SIMD"))
var BUGNUMBER = 946042;
var float32x4 = SIMD.float32x4;
var int32x4 = SIMD.int32x4;

var summary = 'float32x4 reciprocalSqrt';

function reciprocalsqrtf(a) {
    return Math.fround(1 / Math.sqrt(a));
}

function test() {
  print(BUGNUMBER + ": " + summary);

  var a = float32x4(1, 1, 0.25, 0.25);
  var c = SIMD.float32x4.reciprocalSqrtApproximation(a);
  assertEq(c.x, 1);
  assertEq(c.y, 1);
  assertEq(c.z, 2);
  assertEq(c.w, 2);

  var d = float32x4(25, 16, 6.25, 1.5625);
  var f = SIMD.float32x4.reciprocalSqrtApproximation(d);
  assertEq(f.x, reciprocalsqrtf(25));
  assertEq(f.y, reciprocalsqrtf(16));
  assertEq(f.z, reciprocalsqrtf(6.25));
  assertEq(f.w, reciprocalsqrtf(1.5625));

  var g = float32x4(NaN, -0, Infinity, -Infinity);
  var i = SIMD.float32x4.reciprocalSqrtApproximation(g);
  assertEq(i.x, reciprocalsqrtf(NaN));
  assertEq(i.y, reciprocalsqrtf(-0));
  assertEq(i.z, reciprocalsqrtf(Infinity));
  assertEq(i.w, reciprocalsqrtf(-Infinity));

  if (typeof reportCompare === "function")
    reportCompare(true, true);
}

test();

