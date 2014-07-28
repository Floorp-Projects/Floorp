// |reftest| skip-if(!this.hasOwnProperty("SIMD"))
var BUGNUMBER = 946042;
var float32x4 = SIMD.float32x4;
var int32x4 = SIMD.int32x4;

var summary = 'int32x4 shuffleMix';

function test() {
  print(BUGNUMBER + ": " + summary);

  // FIXME -- Bug 948379: Amend to check for correctness of border cases.

  var a = int32x4(1, 2, 3, 4);
  var b = int32x4(10, 20, 30, 40);
  var c = SIMD.int32x4.shuffleMix(a,b, 0x1B);
  assertEq(c.x, 4);
  assertEq(c.y, 3);
  assertEq(c.z, 20);
  assertEq(c.w, 10);

  var d = SIMD.int32x4.shuffleMix(a, b, 0xFF);
  assertEq(d.x, 4);
  assertEq(d.y, 4);
  assertEq(d.z, 40);
  assertEq(d.w, 40);

  var d = SIMD.int32x4.shuffleMix(a, b, 0x0);
  assertEq(d.x, 1);
  assertEq(d.y, 1);
  assertEq(d.z, 10);
  assertEq(d.w, 10);

  var caught = false;
  try {
      var _ = SIMD.int32x4.shuffleMix(a, b, 0xFF + 1);
  } catch (e) {
      caught = true;
  }
  assertEq(caught, true, "Mask can't be more than 0xFF");

  var caught = false;
  try {
      var _ = SIMD.int32x4.shuffleMix(a, b, -1);
  } catch (e) {
      caught = true;
  }
  assertEq(caught, true, "Mask can't be less than 0");


  if (typeof reportCompare === "function")
    reportCompare(true, true);
}

test();

