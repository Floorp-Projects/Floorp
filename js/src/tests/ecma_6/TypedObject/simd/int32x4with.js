// |reftest| skip-if(!this.hasOwnProperty("SIMD"))
var BUGNUMBER = 946042;
var float32x4 = SIMD.float32x4;
var int32x4 = SIMD.int32x4;

var summary = 'int32x4 with';

function test() {
  print(BUGNUMBER + ": " + summary);

  // FIXME -- Bug 948379: Amend to check for correctness of border cases.

  var a = int32x4(1, 2, 3, 4);
  var x = SIMD.int32x4.withX(a, 5);
  var y = SIMD.int32x4.withY(a, 5);
  var z = SIMD.int32x4.withZ(a, 5);
  var w = SIMD.int32x4.withW(a, 5);
  assertEq(x.x, 5);
  assertEq(y.y, 5);
  assertEq(z.z, 5);
  assertEq(w.w, 5);

  if (typeof reportCompare === "function")
    reportCompare(true, true);
}

test();

