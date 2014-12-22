// |reftest| skip-if(!this.hasOwnProperty("SIMD"))
var BUGNUMBER = 1031203;
var float64x2 = SIMD.float64x2;
var int32x4 = SIMD.int32x4;

var summary = 'float64x2 fromInt32x4';

/*
 * Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/
 */

function test() {
  print(BUGNUMBER + ": " + summary);

  var a = int32x4(1, 2, 3, 4);
  var c = float64x2.fromInt32x4(a);
  assertEq(c.x, 1);
  assertEq(c.y, 2);

  var d = int32x4(INT32_MAX, INT32_MIN, 0, 0);
  var f = float64x2.fromInt32x4(d);
  assertEq(f.x, INT32_MAX);
  assertEq(f.y, INT32_MIN);

  if (typeof reportCompare === "function")
    reportCompare(true, true);
}

test();

