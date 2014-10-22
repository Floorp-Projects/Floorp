// |reftest| skip-if(!this.hasOwnProperty("SIMD"))
var BUGNUMBER = 946042;
var float32x4 = SIMD.float32x4;
var int32x4 = SIMD.int32x4;

var summary = 'float32x4 clamp';

function test() {
  print(BUGNUMBER + ": " + summary);

  // FIXME -- Bug 1068028: Amend to check for correctness of NaN border cases once the semantics are defined.

  var a = float32x4(-20, 10, 30, 0.5);
  var lower = float32x4(2, 1, 50, 0);
  var upper = float32x4(2.5, 5, 55, 1);
  var c = SIMD.float32x4.clamp(a, lower, upper);
  assertEq(c.x, 2);
  assertEq(c.y, 5);
  assertEq(c.z, 50);
  assertEq(c.w, 0.5);

  var d = float32x4(-13.37, 10.46, 31.79, 0.54);
  var lower1 = float32x4(2.1, 1.1, 50.13, 0.0);
  var upper1 = float32x4(2.56, 5.55, 55.93, 1.1);
  var f = SIMD.float32x4.clamp(d, lower1, upper1);
  assertEq(f.x, Math.fround(2.1));
  assertEq(f.y, Math.fround(5.55));
  assertEq(f.z, Math.fround(50.13));
  assertEq(f.w, Math.fround(0.54));

  var g = float32x4(4, -Infinity, 10, -10);
  var lower2 = float32x4(5, -Infinity, -Infinity, -Infinity);
  var upper2 = float32x4(Infinity, 5, Infinity, Infinity);
  var i = SIMD.float32x4.clamp(g, lower2, upper2);
  assertEq(i.x, 5);
  assertEq(i.y, -Infinity);
  assertEq(i.z, 10);
  assertEq(i.w, -10);

  if (typeof reportCompare === "function")
    reportCompare(true, true);
}

test();

