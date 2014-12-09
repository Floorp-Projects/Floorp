// |reftest| skip-if(!this.hasOwnProperty("SIMD"))
var BUGNUMBER = 946042;
var float32x4 = SIMD.float32x4;
var int32x4 = SIMD.int32x4;

var summary = 'int32x4 neg';

function test() {
  print(BUGNUMBER + ": " + summary);

  var a = int32x4(1, 2, 3, 4);
  var c = SIMD.int32x4.neg(a);
  assertEq(c.x, -1);
  assertEq(c.y, -2);
  assertEq(c.z, -3);
  assertEq(c.w, -4);

  var INT32_MAX = Math.pow(2, 31) - 1;
  var INT32_MIN = -Math.pow(2, 31);

  var d = int32x4(INT32_MAX, INT32_MIN, -0, 0);
  var f = SIMD.int32x4.neg(d);
  assertEq(f.x, -INT32_MAX | 0);
  assertEq(f.y, -INT32_MIN | 0);
  assertEq(f.z, 0);
  assertEq(f.w, 0);

  if (typeof reportCompare === "function")
    reportCompare(true, true);
}

test();

