// |reftest| skip-if(!this.hasOwnProperty("SIMD"))
var BUGNUMBER = 946042;
var float32x4 = SIMD.float32x4;
var int32x4 = SIMD.int32x4;

var summary = 'int32x4 fromFloat32x4';

function test() {
  print(BUGNUMBER + ": " + summary);

  var a = float32x4(1.1, 2.2, 3.3, 4.6);
  var c = SIMD.int32x4.fromFloat32x4(a);
  assertEq(c.x, 1);
  assertEq(c.y, 2);
  assertEq(c.z, 3);
  assertEq(c.w, 4);

  var d = float32x4(NaN, -0, Infinity, -Infinity);
  var f = SIMD.int32x4.fromFloat32x4(d);
  assertEq(f.x, 0);
  assertEq(f.y, 0);
  assertEq(f.z, 0);
  assertEq(f.w, 0);

  if (typeof reportCompare === "function")
    reportCompare(true, true);
}

test();

