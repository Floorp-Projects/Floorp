// |reftest| skip-if(!this.hasOwnProperty("SIMD"))
var Float32x4 = SIMD.Float32x4;
var Float64x2 = SIMD.Float64x2;

function test() {
  // FIXME -- Bug 1068028: Amend to check for correctness of NaN/-0 border
  // cases once the semantics are defined.

  var a = Float32x4(-20, 10, 30, 0.5);
  var lower = Float32x4(2, 1, 50, 0);
  var upper = Float32x4(2.5, 5, 55, 1);
  var c = SIMD.Float32x4.clamp(a, lower, upper);
  assertEqX4(c, [2, 5, 50, .5])

  var d = Float32x4(-13.37, 10.46, 31.79, 0.54);
  var lower1 = Float32x4(2.1, 1.1, 50.13, 0.0);
  var upper1 = Float32x4(2.56, 5.55, 55.93, 1.1);
  var f = SIMD.Float32x4.clamp(d, lower1, upper1);
  assertEqX4(f, [2.1, 5.55, 50.13, 0.54].map(Math.fround));

  var g = Float32x4(4, -Infinity, 10, -10);
  var lower2 = Float32x4(5, -Infinity, -Infinity, -Infinity);
  var upper2 = Float32x4(Infinity, 5, Infinity, Infinity);
  var i = SIMD.Float32x4.clamp(g, lower2, upper2);
  assertEqX4(i, [5, -Infinity, 10, -10].map(Math.fround));

  var j = Float64x2(-20, 10);
  var k = Float64x2(2.125, 3);
  var lower3 = Float64x2(2, 1);
  var upper3 = Float64x2(2.5, 5);
  var l = Float64x2.clamp(j, lower3, upper3);
  assertEqX2(l, [2, 5]);
  var m = Float64x2.clamp(k, lower3, upper3);
  assertEqX2(m, [2.125, 3]);

  var n = Float64x2(-5, 5);
  var lower4 = Float64x2(-Infinity, 0);
  var upper4 = Float64x2(+Infinity, +Infinity);
  var p = Float64x2.clamp(n, lower4, upper4);
  assertEqX2(p, [-5, 5]);

  if (typeof reportCompare === "function")
    reportCompare(true, true);
}

test();

