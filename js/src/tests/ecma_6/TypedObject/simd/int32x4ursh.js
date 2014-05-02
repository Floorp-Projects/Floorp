// |reftest| skip-if(!this.hasOwnProperty("SIMD"))
var BUGNUMBER = 996076;
var float32x4 = SIMD.float32x4;
var int32x4 = SIMD.int32x4;

var summary = 'int32x4 ursh';

function test() {
  print(BUGNUMBER + ": " + summary);

  // FIXME -- Bug 948379: Amend to check for correctness of border cases.

  for (var bits = 0; bits < 32; bits++) {
      var a = int32x4(-1, 2, -3, 4);
      var c = SIMD.int32x4.shiftRightLogical(a, bits);
      assertEq(c.x >>> 0, -1 >>> bits);
      assertEq(c.y >>> 0, 2 >>> bits);
      assertEq(c.z >>> 0, -3 >>> bits);
      assertEq(c.w >>> 0, 4 >>> bits);
  }

  if (typeof reportCompare === "function")
    reportCompare(true, true);
}

test();

