// |reftest| skip-if(!this.hasOwnProperty("SIMD"))
var BUGNUMBER = 946042;
var float32x4 = SIMD.float32x4;
var int32x4 = SIMD.int32x4;

var summary = 'int32x4 select';

function test() {
  print(BUGNUMBER + ": " + summary);

  // FIXME -- Bug 948379: Amend to check for correctness of border cases.

  var a = float32x4(0.0,4.0,9.0,16.0)
  var b = float32x4(1.0,2.0,3.0,4.0)
  var sel_ttff = int32x4.bool(true, true, false, false);
  var c = SIMD.int32x4.select(sel_ttff,a,b);
  assertEq(c.x, 0);
  assertEq(c.y, 4);
  assertEq(c.z, 3);
  assertEq(c.w, 4);

  if (typeof reportCompare === "function")
    reportCompare(true, true);
}

test();

