// |reftest| skip-if(!this.hasOwnProperty("SIMD"))
var BUGNUMBER = 946042;
var float32x4 = SIMD.float32x4;
var int32x4 = SIMD.int32x4;

var summary = 'float32x4 reciprocal';

function test() {
  print(BUGNUMBER + ": " + summary);

  // FIXME -- Bug 948379: Amend to check for correctness of border cases.

  var a = float32x4(1, 0.5, 0.25, 0.125);
  var c = SIMD.float32x4.reciprocal(a);
  assertEq(c.x, 1);
  assertEq(c.y, 2);
  assertEq(c.z, 4);
  assertEq(c.w, 8);

  if (typeof reportCompare === "function")
    reportCompare(true, true);
}

test();

