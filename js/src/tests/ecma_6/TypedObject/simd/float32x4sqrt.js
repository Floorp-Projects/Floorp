// |reftest| skip-if(!this.hasOwnProperty("SIMD"))
var BUGNUMBER = 946042;
var float32x4 = SIMD.float32x4;
var int32x4 = SIMD.int32x4;

var summary = 'float32x4 sqrt';

function sqrtf(a) {
  return Math.fround(Math.sqrt(Math.fround(a)));
}

function test() {
  print(BUGNUMBER + ": " + summary);

  var a = float32x4(1, 4, 9, 16);
  var c = SIMD.float32x4.sqrt(a);
  assertEq(c.x, 1);
  assertEq(c.y, 2);
  assertEq(c.z, 3);
  assertEq(c.w, 4);

  var d = float32x4(2.7225, 7.3441, 9.4249, -1);
  var f = SIMD.float32x4.sqrt(d);
  assertEq(f.x, sqrtf(2.7225));
  assertEq(f.y, sqrtf(7.3441));
  assertEq(f.z, sqrtf(9.4249));
  assertEq(f.w, NaN);

  var g = float32x4(NaN, -0, Infinity, -Infinity);
  var i = SIMD.float32x4.sqrt(g);
  assertEq(i.x, NaN);
  assertEq(i.y, -0);
  assertEq(i.z, Infinity);
  assertEq(i.w, NaN);

  if (typeof reportCompare === "function")
    reportCompare(true, true);
}

test();

