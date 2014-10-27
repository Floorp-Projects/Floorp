// |reftest| skip-if(!this.hasOwnProperty("SIMD"))
var BUGNUMBER = 946042;
var float32x4 = SIMD.float32x4;
var int32x4 = SIMD.int32x4;

var summary = 'float32x4 fromInt32x4';

function test() {
  print(BUGNUMBER + ": " + summary);

  var INT32_MAX = Math.pow(2, 31) - 1;
  var INT32_MIN = -Math.pow(2, 31);

  var a = int32x4(1, 2, 3, 4);
  var c = SIMD.float32x4.fromInt32x4(a);
  assertEq(c.x, 1);
  assertEq(c.y, 2);
  assertEq(c.z, 3);
  assertEq(c.w, 4);

  var value1 = Math.pow(2, 30) - 1;
  var value2 = -Math.pow(2, 30);
  var d = int32x4(INT32_MIN, INT32_MAX, value1, value2);
  var f = float32x4.fromInt32x4(d);
  assertEq(f.x, Math.fround(INT32_MIN));
  assertEq(f.y, Math.fround(INT32_MAX));
  assertEq(f.z, Math.fround(value1));
  assertEq(f.w, Math.fround(value2));

  if (typeof reportCompare === "function")
    reportCompare(true, true);
}

test();

