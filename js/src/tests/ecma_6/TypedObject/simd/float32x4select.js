// |reftest| skip-if(!this.hasOwnProperty("SIMD"))
var BUGNUMBER = 1060437;
var float32x4 = SIMD.float32x4;
var int32x4 = SIMD.int32x4;

var summary = 'float32x4 select';

function test() {
  print(BUGNUMBER + ": " + summary);

  var a = float32x4(0.125,4.25,9.75,16.125);
  var b = float32x4(1.5,2.75,3.25,4.5);
  var sel_ttff = int32x4.bool(true, true, false, false);
  var c = SIMD.float32x4.select(sel_ttff,a,b);
  assertEq(c.x, 0.125);
  assertEq(c.y, 4.25);
  assertEq(c.z, 3.25);
  assertEq(c.w, 4.5);

  var b2 = float32x4(1.5,2.75,NaN,Infinity);
  var c = SIMD.float32x4.select(sel_ttff,a,b2);
  assertEq(c.x, 0.125);
  assertEq(c.y, 4.25);
  assertEq(c.z, NaN);
  assertEq(c.w, Infinity);

  var a2 = float32x4(-NaN,-Infinity,9.75,16.125);
  var c = SIMD.float32x4.select(sel_ttff,a2,b2);
  assertEq(c.x, -NaN);
  assertEq(c.y, -Infinity);
  assertEq(c.z, NaN);
  assertEq(c.w, Infinity);

  if (typeof reportCompare === "function")
    reportCompare(true, true);
}

test();
