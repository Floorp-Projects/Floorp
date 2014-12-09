// |reftest| skip-if(!this.hasOwnProperty("SIMD"))
var BUGNUMBER = 946042;
var float32x4 = SIMD.float32x4;
var int32x4 = SIMD.int32x4;

var summary = 'float32x4 with';

function test() {
  print(BUGNUMBER + ": " + summary);

  var a = float32x4(1, 2, 3, 4);
  var x = SIMD.float32x4.withX(a, 5);
  var y = SIMD.float32x4.withY(a, 5);
  var z = SIMD.float32x4.withZ(a, 5);
  var w = SIMD.float32x4.withW(a, 5);
  assertEq(x.x, 5);
  assertEq(x.y, 2);
  assertEq(x.z, 3);
  assertEq(x.w, 4);

  assertEq(y.x, 1);
  assertEq(y.y, 5);
  assertEq(y.z, 3);
  assertEq(y.w, 4);

  assertEq(z.x, 1);
  assertEq(z.y, 2);
  assertEq(z.z, 5);
  assertEq(z.w, 4);

  assertEq(w.x, 1);
  assertEq(w.y, 2);
  assertEq(w.z, 3);
  assertEq(w.w, 5);

  var b = float32x4(1.87, 2.08, 3.84, 4.17);
  var x1 = SIMD.float32x4.withX(b, 5.38);
  var y1 = SIMD.float32x4.withY(b, 5.19);
  var z1 = SIMD.float32x4.withZ(b, 5.11);
  var w1 = SIMD.float32x4.withW(b, 5.07);
  assertEq(x1.x, Math.fround(5.38));
  assertEq(x1.y, Math.fround(2.08));
  assertEq(x1.z, Math.fround(3.84));
  assertEq(x1.w, Math.fround(4.17));

  assertEq(y1.x, Math.fround(1.87));
  assertEq(y1.y, Math.fround(5.19));
  assertEq(y1.z, Math.fround(3.84));
  assertEq(y1.w, Math.fround(4.17));

  assertEq(z1.x, Math.fround(1.87));
  assertEq(z1.y, Math.fround(2.08));
  assertEq(z1.z, Math.fround(5.11));
  assertEq(z1.w, Math.fround(4.17));

  assertEq(w1.x, Math.fround(1.87));
  assertEq(w1.y, Math.fround(2.08));
  assertEq(w1.z, Math.fround(3.84));
  assertEq(w1.w, Math.fround(5.07));

  var c = float32x4(NaN, -0, Infinity, -Infinity);
  var x2 = SIMD.float32x4.withX(c, 0);
  var y2 = SIMD.float32x4.withY(c, 0);
  var z2 = SIMD.float32x4.withZ(c, 0);
  var w2 = SIMD.float32x4.withW(c, 0);
  assertEq(x2.x, 0);
  assertEq(x2.y, -0);
  assertEq(x2.z, Infinity);
  assertEq(x2.w, -Infinity);

  assertEq(y2.x, NaN);
  assertEq(y2.y, 0);
  assertEq(y2.z, Infinity);
  assertEq(y2.w, -Infinity);

  assertEq(z2.x, NaN);
  assertEq(z2.y, -0);
  assertEq(z2.z, 0);
  assertEq(z2.w, -Infinity);

  assertEq(w2.x, NaN);
  assertEq(w2.y, -0);
  assertEq(w2.z, Infinity);
  assertEq(w2.w, 0);

  if (typeof reportCompare === "function")
    reportCompare(true, true);
}

test();

