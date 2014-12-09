// |reftest| skip-if(!this.hasOwnProperty("SIMD"))
var BUGNUMBER = 946042;
var float32x4 = SIMD.float32x4;
var int32x4 = SIMD.int32x4;

var summary = 'float32x4 mul';

function mulf(a, b) {
    return Math.fround(Math.fround(a) * Math.fround(b));
}

function test() {
  print(BUGNUMBER + ": " + summary);

  var a = float32x4(1, 2, 3, 4);
  var b = float32x4(10, 20, 30, 40);
  var c = SIMD.float32x4.mul(a, b);
  assertEq(c.x, 10);
  assertEq(c.y, 40);
  assertEq(c.z, 90);
  assertEq(c.w, 160);

  var d = float32x4(1.66, 2.57, 3.73, 4.12);
  var e = float32x4(10.67, 20.68, 30.02, 40.58);
  var f = SIMD.float32x4.mul(d, e);
  assertEq(f.x, mulf(1.66, 10.67));
  assertEq(f.y, mulf(2.57, 20.68));
  assertEq(f.z, mulf(3.73, 30.02));
  assertEq(f.w, mulf(4.12, 40.58));

  var g = float32x4(NaN, -0, Infinity, -Infinity);
  var h = float32x4(NaN, -0, -Infinity, 0);
  var i = SIMD.float32x4.mul(g, h);
  assertEq(i.x, NaN);
  assertEq(i.y, 0);
  assertEq(i.z, -Infinity);
  assertEq(i.w, NaN);

  if (typeof reportCompare === "function")
    reportCompare(true, true);
}

test();
