// |reftest| skip-if(!this.hasOwnProperty("SIMD"))
var BUGNUMBER = 1031203;
var float64x2 = SIMD.float64x2;

var summary = 'float64x2 with';

/*
 * Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/
 */

function test() {
  print(BUGNUMBER + ": " + summary);

  var a = float64x2(1, 2);
  var x = float64x2.withX(a, 5);
  var y = float64x2.withY(a, 5);
  assertEq(x.x, 5);
  assertEq(x.y, 2);
  assertEq(y.x, 1);
  assertEq(y.y, 5);

  var b = float64x2(NaN, -0);
  var x1 = float64x2.withX(b, Infinity);
  var y1 = float64x2.withY(b, -Infinity);
  assertEq(x1.x, Infinity);
  assertEq(x1.y, -0);
  assertEq(y1.x, NaN);
  assertEq(y1.y, -Infinity);

  if (typeof reportCompare === "function")
    reportCompare(true, true);
}

test();

