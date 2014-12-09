// |reftest| skip-if(!this.hasOwnProperty("SIMD"))
var BUGNUMBER = 946042;
var float32x4 = SIMD.float32x4;
var int32x4 = SIMD.int32x4;

var summary = 'float32x4 div';

function divf(a, b) {
    return Math.fround(Math.fround(a) / Math.fround(b));
}

function test() {
  print(BUGNUMBER + ": " + summary);

  var a = float32x4(1, 2, 3, 4);
  var b = float32x4(10, 20, 30, 40);
  var c = SIMD.float32x4.div(b, a);
  assertEq(c.x, 10);
  assertEq(c.y, 10);
  assertEq(c.z, 10);
  assertEq(c.w, 10);

  var d = float32x4(1.26, 2.03, 3.17, 4.59);
  var e = float32x4(11.025, 17.3768, 29.1957, 46.4049);
  var f = SIMD.float32x4.div(e, d);
  assertEq(f.x, divf(11.025, 1.26));
  assertEq(f.y, divf(17.3768, 2.03));
  assertEq(f.z, divf(29.1957, 3.17));
  assertEq(f.w, divf(46.4049, 4.59));

  var g = float32x4(0, -0, Infinity, -Infinity);
  var h = float32x4(1, 1, -Infinity, Infinity);
  var i = SIMD.float32x4.div(h, g);
  assertEq(i.x, Infinity);
  assertEq(i.y, -Infinity);
  assertEq(i.z, NaN);
  assertEq(i.w, NaN);

  if (typeof reportCompare === "function")
    reportCompare(true, true);
}

test();
