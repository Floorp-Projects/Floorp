// |reftest| skip-if(!this.hasOwnProperty("SIMD"))
var BUGNUMBER = 1031203;
var float64x2 = SIMD.float64x2;
var int32x4 = SIMD.int32x4;

var summary = 'float64x2 select';

/*
 * Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/
 */

function test() {
  print(BUGNUMBER + ": " + summary);

  var a = int32x4.bool(true, true, false, false);
  var b = SIMD.float64x2(1, 2);
  var c = SIMD.float64x2(3, 4);
  var d = SIMD.float64x2.select(a, b, c);
  assertEq(d.x, 1);
  assertEq(d.y, 4);

  var e = SIMD.float64x2(NaN, -0);
  var f = SIMD.float64x2(+Infinity, -Infinity);
  var g = SIMD.float64x2.select(a, e, f);
  assertEq(g.x, NaN);
  assertEq(g.y, -Infinity);

  if (typeof reportCompare === "function")
    reportCompare(true, true);
}

test();

