// |reftest| skip-if(!this.hasOwnProperty("SIMD"))
var BUGNUMBER = 1031203;
var float32x4 = SIMD.float32x4;
var float64x2 = SIMD.float64x2;

var summary = 'float64x2 fromFloat32x4';

/*
 * Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/
 */

function test() {
  print(BUGNUMBER + ": " + summary);

  var a = float32x4(100, 200, 300, 400);
  var c = float64x2.fromFloat32x4(a);
  assertEq(c.x, 100);
  assertEq(c.y, 200);

  var d = float32x4(NaN, -0, NaN, -0);
  var f = float64x2.fromFloat32x4(d);
  assertEq(f.x, NaN);
  assertEq(f.y, -0);

  var g = float32x4(Infinity, -Infinity, Infinity, -Infinity);
  var i = float64x2.fromFloat32x4(g);
  assertEq(i.x, Infinity);
  assertEq(i.y, -Infinity);

  var j = float32x4(13.37, 12.853, 49.97, 53.124);
  var l = float64x2.fromFloat32x4(j);
  assertEq(l.x, Math.fround(13.37));
  assertEq(l.y, Math.fround(12.853));

  if (typeof reportCompare === "function")
    reportCompare(true, true);
}

test();

