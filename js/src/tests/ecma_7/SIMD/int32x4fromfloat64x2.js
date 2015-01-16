// |reftest| skip-if(!this.hasOwnProperty("SIMD"))
var BUGNUMBER = 1031203;
var float64x2 = SIMD.float64x2;
var int32x4 = SIMD.int32x4;

var summary = 'int32x4 fromFloat64x2';

/*
 * Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/
 */

function test() {
  print(BUGNUMBER + ": " + summary);

  var a = float64x2(1, 2.2);
  var c = int32x4.fromFloat64x2(a);
  assertEq(c.x, 1);
  assertEq(c.y, 2);
  assertEq(c.z, 0);
  assertEq(c.w, 0);

  var d = float64x2(NaN, -0);
  var f = int32x4.fromFloat64x2(d);
  assertEq(f.x, 0);
  assertEq(f.y, 0);
  assertEq(f.z, 0);
  assertEq(f.w, 0);

  var g = float64x2(Infinity, -Infinity);
  var i = int32x4.fromFloat64x2(g);
  assertEq(i.x, 0);
  assertEq(i.y, 0);
  assertEq(i.z, 0);
  assertEq(i.w, 0);

  var j = Math.pow(2, 31);
  var k = -Math.pow(2, 31) - 1;
  var m = float64x2(j, k);
  var l = int32x4.fromFloat64x2(m);
  assertEq(l.x, INT32_MIN);
  assertEq(l.y, INT32_MAX);
  assertEq(l.z, 0);
  assertEq(l.w, 0);

  if (typeof reportCompare === "function")
    reportCompare(true, true);
}

test();

