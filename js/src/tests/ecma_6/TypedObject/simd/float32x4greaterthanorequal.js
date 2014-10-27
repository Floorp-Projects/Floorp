// |reftest| skip-if(!this.hasOwnProperty("SIMD"))
var BUGNUMBER = 946042;
var float32x4 = SIMD.float32x4;
var int32x4 = SIMD.int32x4;

var summary = 'float32x4 greaterThanOrEqual';

function test() {
  print(BUGNUMBER + ": " + summary);

  // FIXME -- Bug 1081697: Amend to check for correctness of -0/Infinity/-Infinity border cases.
  // FIXME -- Bug 1068028: Amend to check for correctness of NaN border cases once the semantics are defined.

  var a = float32x4(1, 20, 30, 40);
  var b = float32x4(10, 20, 30, 4);
  var c = SIMD.float32x4.greaterThanOrEqual(b, a);
  assertEq(c.x, -1);
  assertEq(c.y, -1);
  assertEq(c.z, -1);
  assertEq(c.w, 0);

  var d = float32x4(10.029, 20.87, 30.56, 4.7);
  var e = float32x4(10.03, 20.87, 30.56, 4.698);
  var f = float32x4.greaterThanOrEqual(e, d);
  assertEq(f.x, -1);
  assertEq(f.y, -1);
  assertEq(f.z, -1);
  assertEq(f.w, 0);

  if (typeof reportCompare === "function")
    reportCompare(true, true);
}

test();

