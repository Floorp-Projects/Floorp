// |reftest| skip-if(!this.hasOwnProperty("SIMD"))
var BUGNUMBER = 996076;
var float32x4 = SIMD.float32x4;
var int32x4 = SIMD.int32x4;

var summary = 'int32x4 ursh';

function test() {
  print(BUGNUMBER + ": " + summary);

  for (var bits = 0; bits < 32; bits++) {
      var a = int32x4(-1, 2, -3, 4);
      var c = SIMD.int32x4.shiftRightLogical(a, bits);
      assertEq(c.x >>> 0, -1 >>> bits);
      assertEq(c.y >>> 0, 2 >>> bits);
      assertEq(c.z >>> 0, -3 >>> bits);
      assertEq(c.w >>> 0, 4 >>> bits);
  }

  var INT32_MAX = Math.pow(2, 31) - 1;
  var INT32_MIN = -Math.pow(2, 31);

  var d = int32x4(INT32_MAX, INT32_MIN, INT32_MAX, INT32_MIN);
  var f = SIMD.int32x4.shiftRightLogical(d, 0);
  assertEq(f.x, (INT32_MAX >>> 0) | 0);
  assertEq(f.y, (INT32_MIN >>> 0) | 0);
  assertEq(f.z, (INT32_MAX >>> 0) | 0);
  assertEq(f.w, (INT32_MIN >>> 0) | 0);

  if (typeof reportCompare === "function")
    reportCompare(true, true);
}

test();

