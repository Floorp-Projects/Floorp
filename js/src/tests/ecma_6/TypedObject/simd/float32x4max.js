// |reftest| skip-if(!this.hasOwnProperty("SIMD"))
var BUGNUMBER = 946042;
var float32x4 = SIMD.float32x4;
var int32x4 = SIMD.int32x4;

var summary = 'float32x4 max';

function test() {
  print(BUGNUMBER + ": " + summary);

  // FIXME -- Bug 1081697: Amend to check for correctness of -0/Infinity/-Infinity border cases.
  // FIXME -- Bug 1068028: Amend to check for correctness of NaN border cases once the semantics are defined.

  var a = float32x4(1, 20, 30, 4);
  var b = float32x4(10, 2, 3, 40);
  var c = SIMD.float32x4.max(a, b);
  assertEq(c.x, 10);
  assertEq(c.y, 20);
  assertEq(c.z, 30);
  assertEq(c.w, 40);

  var d = float32x4(9.999, 2.1234, 30.4443, 4);
  var e = float32x4(10, 2.1233, 30.4444, 4.0001);
  var f = float32x4.max(d, e);
  assertEq(f.x, 10);
  assertEq(f.y, Math.fround(2.1234));
  assertEq(f.z, Math.fround(30.4444));
  assertEq(f.w, Math.fround(4.0001));

  if (typeof reportCompare === "function")
    reportCompare(true, true);
}

test();

