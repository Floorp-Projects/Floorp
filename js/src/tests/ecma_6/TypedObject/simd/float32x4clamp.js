// |reftest| skip-if(!this.hasOwnProperty("SIMD"))
var BUGNUMBER = 946042;
var float32x4 = SIMD.float32x4;
var int32x4 = SIMD.int32x4;

var summary = 'float32x4 clamp';

function test() {
  print(BUGNUMBER + ": " + summary);

  var a = float32x4(-13.37, 10.46, 31.79, 0.54);
  var lower = float32x4(2.1, 1.1, 50.13, 0.0);
  var upper = float32x4(2.56, 5.55, 55.93, 1.1);
  var c = SIMD.float32x4.clamp(a, lower, upper);
  assertEq(c.x, Math.fround(2.1));
  assertEq(c.y, Math.fround(5.55));
  assertEq(c.z, Math.fround(50.13));
  assertEq(c.w, Math.fround(0.54));

  if (typeof reportCompare === "function")
    reportCompare(true, true);
}

test();

