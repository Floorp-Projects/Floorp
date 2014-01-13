// |reftest| skip-if(!this.hasOwnProperty("SIMD"))
var BUGNUMBER = 946042;
var float32x4 = SIMD.float32x4;
var int32x4 = SIMD.int32x4;

var summary = 'int32x4 xor';

function test() {
  print(BUGNUMBER + ": " + summary);

  // FIXME -- Bug 948379: Amend to check for correctness of border cases.

  var a = int32x4(1, 2, 3, 4);
  var b = int32x4(10, 20, 30, 40);
  var c = SIMD.int32x4.xor(a, b);
  assertEq(c.x, 11);
  assertEq(c.y, 22);
  assertEq(c.z, 29);
  assertEq(c.w, 44);

  if (typeof reportCompare === "function")
    reportCompare(true, true);
}

test();

