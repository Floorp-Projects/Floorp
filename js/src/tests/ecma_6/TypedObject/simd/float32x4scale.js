// |reftest| skip-if(!this.hasOwnProperty("SIMD"))
var BUGNUMBER = 946042;
var float32x4 = SIMD.float32x4;
var int32x4 = SIMD.int32x4;

var summary = 'float32x4 scale';

function mulf(a, b) {
  return Math.fround(Math.fround(a) * Math.fround(b));
}

function test() {
  print(BUGNUMBER + ": " + summary);

  var a = float32x4(1, 2, 3, 4);
  var c = SIMD.float32x4.scale(a, 2);
  assertEq(c.x, 2);
  assertEq(c.y, 4);
  assertEq(c.z, 6);
  assertEq(c.w, 8);

  var d = float32x4(1.34, 2.76, 3.21, 4.09);
  var f = float32x4.scale(d, 2.54);
  assertEq(f.x, mulf(1.34, 2.54));
  assertEq(f.y, mulf(2.76, 2.54));
  assertEq(f.z, mulf(3.21, 2.54));
  assertEq(f.w, mulf(4.09, 2.54));

  var g = float32x4(NaN, -0, Infinity, -Infinity);
  var i = float32x4.scale(g, 2.54);
  assertEq(i.x, NaN);
  assertEq(i.y, -0);
  assertEq(i.z, Infinity);
  assertEq(i.w, -Infinity);

  if (typeof reportCompare === "function")
    reportCompare(true, true);
}

test();

