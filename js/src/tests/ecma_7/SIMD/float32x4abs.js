// |reftest| skip-if(!this.hasOwnProperty("SIMD"))
var BUGNUMBER = 946042;
var float32x4 = SIMD.float32x4;
var int32x4 = SIMD.int32x4;

var summary = 'float32x4 abs';

function test() {
  print(BUGNUMBER + ": " + summary);

  var a = float32x4(-1, 2, -3, 4);
  var c = SIMD.float32x4.abs(a);
  assertEq(c.x, 1);
  assertEq(c.y, 2);
  assertEq(c.z, 3);
  assertEq(c.w, 4);

  var d = float32x4(-1.63, 2.46, -3.17, 4.94);
  var f = SIMD.float32x4.abs(d);
  assertEq(f.x, Math.fround(1.63));
  assertEq(f.y, Math.fround(2.46));
  assertEq(f.z, Math.fround(3.17));
  assertEq(f.w, Math.fround(4.94));

  var g = float32x4(NaN, -0, Infinity, -Infinity);
  var i = SIMD.float32x4.abs(g);
  assertEq(i.x, NaN);
  assertEq(i.y, 0);
  assertEq(i.z, Infinity);
  assertEq(i.w, Infinity);

  if (typeof reportCompare === "function")
    reportCompare(true, true);
}

test();
