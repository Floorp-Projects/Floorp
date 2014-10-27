// |reftest| skip-if(!this.hasOwnProperty("SIMD"))
var BUGNUMBER = 946042;
var float32x4 = SIMD.float32x4;
var int32x4 = SIMD.int32x4;

var summary = 'float32x4 sub';

function subf(a, b) {
    return Math.fround(Math.fround(a) - Math.fround(b));
}

function test() {
  print(BUGNUMBER + ": " + summary);

  var a = float32x4(1, 2, 3, 4);
  var b = float32x4(10, 20, 30, 40);
  var c = SIMD.float32x4.sub(b, a);
  assertEq(c.x, 9);
  assertEq(c.y, 18);
  assertEq(c.z, 27);
  assertEq(c.w, 36);

  var d = float32x4(1.34, 2.95, 3.17, 4.29);
  var e = float32x4(10.18, 20.43, 30.63, 40.38);
  var f = SIMD.float32x4.sub(e, d);
  assertEq(f.x, subf(10.18, 1.34));
  assertEq(f.y, subf(20.43, 2.95));
  assertEq(f.z, subf(30.63, 3.17));
  assertEq(f.w, subf(40.38, 4.29));

  var g = float32x4(NaN, -0, -Infinity, -Infinity);
  var h = float32x4(NaN, -0, Infinity, -Infinity);
  var i = SIMD.float32x4.sub(h, g);
  assertEq(i.x, NaN);
  assertEq(i.y, 0);
  assertEq(i.z, Infinity);
  assertEq(i.w, NaN);

  if (typeof reportCompare === "function")
    reportCompare(true, true);
}

test();

