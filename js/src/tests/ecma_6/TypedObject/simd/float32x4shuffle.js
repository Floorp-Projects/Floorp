// |reftest| skip-if(!this.hasOwnProperty("SIMD"))
var BUGNUMBER = 946042;
var float32x4 = SIMD.float32x4;
var int32x4 = SIMD.int32x4;

var summary = 'float32x4 shuffle';

function test() {
  print(BUGNUMBER + ": " + summary);

  // FIXME -- Bug 948379: Amend to check for correctness of border cases.

  var a = float32x4(1, 2, 3, 4);
  var c = SIMD.float32x4.shuffle(a, 0x1B);
  assertEq(c.x, 4);
  assertEq(c.y, 3);
  assertEq(c.z, 2);
  assertEq(c.w, 1);

  var d = SIMD.float32x4.shuffle(a, 0xFF);
  assertEq(d.x, 4);
  assertEq(d.y, 4);
  assertEq(d.z, 4);
  assertEq(d.w, 4);

  var d = SIMD.float32x4.shuffle(a, 0x0);
  assertEq(d.x, 1);
  assertEq(d.y, 1);
  assertEq(d.z, 1);
  assertEq(d.w, 1);

  var caught = false;
  try {
      var _ = SIMD.float32x4.shuffle(a, 0xFF + 1);
  } catch (e) {
      caught = true;
  }
  assertEq(caught, true, "Mask can't be more than 0xFF");

  var caught = false;
  try {
      var _ = SIMD.float32x4.shuffle(a, -1);
  } catch (e) {
      caught = true;
  }
  assertEq(caught, true, "Mask can't be less than 0");

  if (typeof reportCompare === "function")
    reportCompare(true, true);
}

test();

