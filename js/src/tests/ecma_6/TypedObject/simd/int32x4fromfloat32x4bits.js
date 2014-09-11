// |reftest| skip-if(!this.hasOwnProperty("SIMD"))
var BUGNUMBER = 946042;
var float32x4 = SIMD.float32x4;
var int32x4 = SIMD.int32x4;

var summary = 'int32x4 fromFloat32x4Bits';

function test() {
  print(BUGNUMBER + ": " + summary);

  var a = float32x4(1, 2, 3, 4);
  var c = SIMD.int32x4.fromFloat32x4Bits(a);
  assertEq(c.x, 0x3f800000 | 0);
  assertEq(c.y, 0x40000000 | 0);
  assertEq(c.z, 0x40400000 | 0);
  assertEq(c.w, 0x40800000 | 0);

  var d = float32x4(NaN, -0, Infinity, -Infinity);
  var f = SIMD.int32x4.fromFloat32x4Bits(d);
  assertEq(f.x, 0x7fc00000 | 0);
  assertEq(f.y, 0x80000000 | 0);
  assertEq(f.z, 0x7f800000 | 0);
  assertEq(f.w, 0xff800000 | 0);

  if (typeof reportCompare === "function")
    reportCompare(true, true);
}

test();

