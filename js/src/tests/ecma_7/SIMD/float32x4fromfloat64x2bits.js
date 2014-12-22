// |reftest| skip-if(!this.hasOwnProperty("SIMD"))
var BUGNUMBER = 1031203;
var float32x4 = SIMD.float32x4;
var float64x2 = SIMD.float64x2;
var int32x4 = SIMD.int32x4;

var summary = 'float32x4 fromFloat64x2Bits';

/*
 * Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/
 */

function test() {
  print(BUGNUMBER + ": " + summary);

  var a = float64x2(2.000000473111868, 512.0001225471497);
  var b = float32x4.fromFloat64x2Bits(a);
  assertEq(b.x, 1.0);
  assertEq(b.y, 2.0);
  assertEq(b.z, 3.0);
  assertEq(b.w, 4.0);

  var c = float64x2(-0, NaN);
  var d = float32x4.fromFloat64x2Bits(c);
  assertEq(d.x, 0);
  assertEq(d.y, -0);
  assertEq(d.z, 0);
  assertEq(d.w, NaN);

  var e = float64x2(Infinity, -Infinity);
  var f = float32x4.fromFloat64x2Bits(e);
  assertEq(f.x, 0);
  assertEq(f.y, NaN);
  assertEq(f.z, 0);
  assertEq(f.w, NaN);

  if (typeof reportCompare === "function")
    reportCompare(true, true);
}

test();

