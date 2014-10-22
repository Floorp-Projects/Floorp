// |reftest| skip-if(!this.hasOwnProperty("SIMD"))
var BUGNUMBER = 946042;
var float32x4 = SIMD.float32x4;
var int32x4 = SIMD.int32x4;

var summary = 'float32x4 min';

function test() {
  print(BUGNUMBER + ": " + summary);

  // FIXME -- Bug 1081697: Amend to check for correctness of -0/Infinity/-Infinity border cases.
  // FIXME -- Bug 1068028: Amend to check for correctness of NaN border cases once the semantics are defined.

  var a = float32x4(1, 20, 3, 40);
  var b = float32x4(10, 2, 30, 4);
  var c = SIMD.float32x4.min(a, b);
  assertEq(c.x, 1);
  assertEq(c.y, 2);
  assertEq(c.z, 3);
  assertEq(c.w, 4);

  var d = float32x4(1.4321, 20.5567, 30.8999, 4.0002);
  var e = float32x4(1.432, 20.5568, 30.8998, 4.0001);
  var f = float32x4.min(d, e);
  assertEq(f.x, Math.fround(1.432));
  assertEq(f.y, Math.fround(20.5567));
  assertEq(f.z, Math.fround(30.8998));
  assertEq(f.w, Math.fround(4.0001));

  if (typeof reportCompare === "function")
    reportCompare(true, true);
}

test();

