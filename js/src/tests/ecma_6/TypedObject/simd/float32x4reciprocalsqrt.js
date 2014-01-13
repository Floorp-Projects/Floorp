// |reftest| skip-if(!this.hasOwnProperty("SIMD"))
var BUGNUMBER = 946042;
var float32x4 = SIMD.float32x4;
var int32x4 = SIMD.int32x4;

var summary = 'float32x4 reciprocalSqrt';

function test() {
  print(BUGNUMBER + ": " + summary);

  // FIXME -- Bug 948379: Amend to check for correctness of border cases.

  var a = float32x4(1, 1, 0.25, 0.25);
  var c = SIMD.float32x4.reciprocalSqrt(a);
  assertEq(c.x, 1);
  assertEq(c.y, 1);
  assertEq(c.z, 2);
  assertEq(c.w, 2);

  if (typeof reportCompare === "function")
    reportCompare(true, true);
}

test();

