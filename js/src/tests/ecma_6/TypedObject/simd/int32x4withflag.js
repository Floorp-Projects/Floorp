// |reftest| skip-if(!this.hasOwnProperty("SIMD"))
var BUGNUMBER = 946042;
var float32x4 = SIMD.float32x4;
var int32x4 = SIMD.int32x4;

var summary = 'int32x4 with';

function test() {
  print(BUGNUMBER + ": " + summary);

  // FIXME -- Bug 948379: Amend to check for correctness of border cases.

  var a = int32x4(1, 2, 3, 4);
  var x = SIMD.int32x4.withFlagX(a, true);
  var y = SIMD.int32x4.withFlagY(a, false);
  var z = SIMD.int32x4.withFlagZ(a, false);
  var w = SIMD.int32x4.withFlagW(a, true);
  assertEq(x.x, -1);
  assertEq(y.y, 0);
  assertEq(z.z, 0);
  assertEq(w.w, -1);

  if (typeof reportCompare === "function")
    reportCompare(true, true);
}

test();

