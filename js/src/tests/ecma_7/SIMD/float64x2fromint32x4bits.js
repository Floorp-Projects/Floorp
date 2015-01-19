// |reftest| skip-if(!this.hasOwnProperty("SIMD"))
var BUGNUMBER = 1031203;
var float64x2 = SIMD.float64x2;
var int32x4 = SIMD.int32x4;

var summary = 'float64x2 fromInt32x4Bits';

/*
 * Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/
 */

function test() {
  print(BUGNUMBER + ": " + summary);

  var a = int32x4(0x00000000, 0x3ff00000, 0x0000000, 0x40000000);
  var c = float64x2.fromInt32x4Bits(a);
  assertEq(c.x, 1.0);
  assertEq(c.y, 2.0);

  var d = int32x4(0xabcdef12, 0x3ff00000, 0x21fedcba, 0x40000000);
  var f = float64x2.fromInt32x4Bits(d);
  assertEq(f.x, 1.0000006400213732);
  assertEq(f.y, 2.0000002532866263);

  if (typeof reportCompare === "function")
    reportCompare(true, true);
}

test();

