// |reftest| skip-if(!this.hasOwnProperty("SIMD"))
var BUGNUMBER = 946042;
var float32x4 = SIMD.float32x4;
var int32x4 = SIMD.int32x4;

var summary = 'float32x4 notEqual';

function test() {
  print(BUGNUMBER + ": " + summary);

  // FIXME -- Bug 1081697: Amend to check for correctness of NaN/-0/Infinity/-Infinity border cases.

  var a = float32x4(1, 20, 30, 40);
  var b = float32x4(10, 20, 30, 4);
  var c = SIMD.float32x4.notEqual(a, b);
  assertEq(c.x, -1);
  assertEq(c.y, 0);
  assertEq(c.z, 0);
  assertEq(c.w, -1);

  var d = float32x4(9.98, 20.65, 30.14, 4.235);
  var e = float32x4(9.99, 20.65, Math.fround(30.14), 4.23);
  var f = SIMD.float32x4.notEqual(d, e);
  assertEq(f.x, -1);
  assertEq(f.y, 0);
  assertEq(f.z, 0);
  assertEq(f.w, -1);

  if (typeof reportCompare === "function")
    reportCompare(true, true);
}

test();

