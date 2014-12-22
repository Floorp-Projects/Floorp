// |reftest| skip-if(!this.hasOwnProperty("SIMD"))
var BUGNUMBER = 1031203;
var float32x4 = SIMD.float32x4;
var float64x2 = SIMD.float64x2;
var int32x4 = SIMD.int32x4;

var summary = 'float64x2 fromFloat32x4Bits';

/*
 * Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/
 */

function test() {
  print(BUGNUMBER + ": " + summary);

  var a = float32x4(0, 1.875, 0, 2);
  var c = float64x2.fromFloat32x4Bits(a);
  assertEq(c.x, 1.0);
  assertEq(c.y, 2.0);

  var d = float32x4(NaN, -0, Infinity, -Infinity);
  var f = float64x2.fromFloat32x4Bits(d);
  assertEq(f.x, -1.058925634e-314);
  assertEq(f.y, -1.404448428688076e+306);

  if (typeof reportCompare === "function")
    reportCompare(true, true);
}

test();

