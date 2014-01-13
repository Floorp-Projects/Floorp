// |reftest| skip-if(!this.hasOwnProperty("SIMD"))
var BUGNUMBER = 946042;
var float32x4 = SIMD.float32x4;
var int32x4 = SIMD.int32x4;

var summary = 'float32x4 scale';

function test() {
  print(BUGNUMBER + ": " + summary);

  // FIXME -- Bug 948379: Amend to check for correctness of border cases.

  var a = float32x4(1, 2, 3, 4);
  var c = SIMD.float32x4.scale(a, 2);
  assertEq(c.x, 2);
  assertEq(c.y, 4);
  assertEq(c.z, 6);
  assertEq(c.w, 8);

  if (typeof reportCompare === "function")
    reportCompare(true, true);
}

test();

