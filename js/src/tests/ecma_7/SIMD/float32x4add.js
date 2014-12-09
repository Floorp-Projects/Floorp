// |reftest| skip-if(!this.hasOwnProperty("SIMD"))
var BUGNUMBER = 946042;
var float32x4 = SIMD.float32x4;
var int32x4 = SIMD.int32x4;

var summary = 'float32x4 add';

function addf(a, b) {
  return Math.fround(Math.fround(a) + Math.fround(b));
}

function test() {
  print(BUGNUMBER + ": " + summary);

  var a = float32x4(1, 2, 3, 4);
  var b = float32x4(10, 20, 30, 40);
  var c = SIMD.float32x4.add(a, b);
  assertEq(c.x, 11);
  assertEq(c.y, 22);
  assertEq(c.z, 33);
  assertEq(c.w, 44);

  var d = float32x4(1.57, 2.27, 3.57, 4.19);
  var e = float32x4(10.31, 20.49, 30.41, 40.72);
  var f = SIMD.float32x4.add(d, e);
  assertEq(f.x, addf(1.57, 10.31));
  assertEq(f.y, addf(2.27, 20.49));
  assertEq(f.z, addf(3.57, 30.41));
  assertEq(f.w, addf(4.19, 40.72));

  var g = float32x4(NaN, -0, Infinity, -Infinity);
  var h = float32x4(0, -0, -Infinity, -Infinity);
  var i = SIMD.float32x4.add(g, h);
  assertEq(i.x, NaN);
  assertEq(i.y, -0);
  assertEq(i.z, NaN);
  assertEq(i.w, -Infinity);

  if (typeof reportCompare === "function")
    reportCompare(true, true);
}

test();
