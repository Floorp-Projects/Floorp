// |reftest| skip-if(!this.hasOwnProperty("SIMD"))
var BUGNUMBER = 946042;
var float32x4 = SIMD.float32x4;
var int32x4 = SIMD.int32x4;

var summary = 'float32x4 reciprocal';

function reciprocalf(a) {
  return Math.fround(1 / Math.fround(a));
}

function test() {
  print(BUGNUMBER + ": " + summary);

  var a = float32x4(1, 0.5, 0.25, 0.125);
  var c = SIMD.float32x4.reciprocal(a);
  assertEq(c.x, 1);
  assertEq(c.y, 2);
  assertEq(c.z, 4);
  assertEq(c.w, 8);

  var d = float32x4(1.6, 0.8, 0.4, 0.2);
  var f = SIMD.float32x4.reciprocal(d);
  assertEq(f.x, reciprocalf(1.6));
  assertEq(f.y, reciprocalf(0.8));
  assertEq(f.z, reciprocalf(0.4));
  assertEq(f.w, reciprocalf(0.2));

  var g = float32x4(NaN, -0, Infinity, -Infinity);
  var i = SIMD.float32x4.reciprocal(g);
  assertEq(i.x, NaN);
  assertEq(i.y, -Infinity);
  assertEq(i.z, 0);
  assertEq(i.w, -0);

  if (typeof reportCompare === "function")
    reportCompare(true, true);
}

test();

