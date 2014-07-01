// |reftest| skip-if(!this.hasOwnProperty("SIMD"))
var BUGNUMBER = 946042;
var float32x4 = SIMD.float32x4;
var int32x4 = SIMD.int32x4;

var summary = 'int32x4 fromFloat32x4Bits';

function test() {
  print(BUGNUMBER + ": " + summary);

  // FIXME -- Bug 948379: Amend to check for correctness of border cases.

  var a = float32x4(1, 2, 3, 4);
  var c = SIMD.int32x4.fromFloat32x4Bits(a);
  assertEq(c.x, 1065353216);
  assertEq(c.y, 1073741824);
  assertEq(c.z, 1077936128);
  assertEq(c.w, 1082130432);

  if (typeof reportCompare === "function")
    reportCompare(true, true);
}

test();

