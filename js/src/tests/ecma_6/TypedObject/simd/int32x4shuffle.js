// |reftest| skip-if(!this.hasOwnProperty("SIMD"))
var BUGNUMBER = 946042;
var float32x4 = SIMD.float32x4;
var int32x4 = SIMD.int32x4;

var summary = 'int32x4 shuffle';

function test() {
  print(BUGNUMBER + ": " + summary);

  // FIXME -- Bug 948379: Amend to check for correctness of border cases.

  var a = int32x4(1, 2, 3, 4);
  var c = SIMD.int32x4.shuffle(a, 0x1B);
  assertEq(c.x, 4);
  assertEq(c.y, 3);
  assertEq(c.z, 2);
  assertEq(c.w, 1);

  if (typeof reportCompare === "function")
    reportCompare(true, true);
}

test();

