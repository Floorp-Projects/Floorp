// |reftest| skip-if(!this.hasOwnProperty("SIMD"))
var BUGNUMBER = 1031203;
var float64x2 = SIMD.float64x2;
var int32x4 = SIMD.int32x4;

var summary = 'int32x4 fromFloat64x2Bits';

/*
 * Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/
 */

function test() {
  print(BUGNUMBER + ": " + summary);

  var a = float64x2(1.0, 2.0);
  var c = int32x4.fromFloat64x2Bits(a);
  assertEq(c.x, 0x00000000);
  assertEq(c.y, 0x3FF00000);
  assertEq(c.z, 0x00000000);
  assertEq(c.w, 0x40000000);

  var d = float64x2(+Infinity, -Infinity);
  var f = int32x4.fromFloat64x2Bits(d);
  assertEq(f.x, 0x00000000);
  assertEq(f.y, 0x7ff00000);
  assertEq(f.z, 0x00000000);
  assertEq(f.w, -0x100000);

  var g = float64x2(-0, NaN);
  var i = int32x4.fromFloat64x2Bits(g);
  assertEq(i.x, 0x00000000);
  assertEq(i.y, -0x80000000);
  assertEq(i.z, 0x00000000);
  assertEq(i.w, 0x7ff80000);

  var j = float64x2(1.0000006400213732, 2.0000002532866263);
  var l = int32x4.fromFloat64x2Bits(j);
  assertEq(l.x, -0x543210ee);
  assertEq(l.y, 0x3ff00000);
  assertEq(l.z, 0x21fedcba);
  assertEq(l.w, 0x40000000);

  if (typeof reportCompare === "function")
    reportCompare(true, true);
}

test();

