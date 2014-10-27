// |reftest| skip-if(!this.hasOwnProperty("SIMD"))
var BUGNUMBER = 946042;
var float32x4 = SIMD.float32x4;
var int32x4 = SIMD.int32x4;

var summary = 'float32x4 lessThan';

function test() {
  print(BUGNUMBER + ": " + summary);

  // FIXME -- Bug 1081697: Amend to check for correctness of -0/Infinity/-Infinity border cases.
  // FIXME -- Bug 1068028: Amend to check for correctness of NaN border cases once the semantics are defined.

  var a = float32x4(1, 20, 3, 40);
  var b = float32x4(10, 2, 30, 4);
  var c = SIMD.float32x4.lessThan(a, b);
  assertEq(c.x, -1);
  assertEq(c.y, 0);
  assertEq(c.z, -1);
  assertEq(c.w, 0);

  var d = float32x4(1.5399, 20.001, 30.045, 4.74);
  var e = float32x4(1.54, 19.999, 30.05, 4.72);
  var f = float32x4.lessThan(a, b);
  assertEq(f.x, -1);
  assertEq(f.y, 0);
  assertEq(f.z, -1);
  assertEq(f.w, 0);

  if (typeof reportCompare === "function")
    reportCompare(true, true);
}

test();

