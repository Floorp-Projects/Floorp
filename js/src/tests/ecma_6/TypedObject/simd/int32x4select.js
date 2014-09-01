// |reftest| skip-if(!this.hasOwnProperty("SIMD"))
var BUGNUMBER = 1060437;
var int32x4 = SIMD.int32x4;

var summary = 'int32x4 select';

const INT32_MAX = Math.pow(2, 31) - 1;
const INT32_MIN = INT32_MAX + 1 | 0;

function test() {
  print(BUGNUMBER + ": " + summary);

  var a = int32x4(0,4,9,16)
  var b = int32x4(1,2,3,4)
  var sel_ttff = int32x4.bool(true, true, false, false);
  var c = SIMD.int32x4.select(sel_ttff,a,b);
  assertEq(c.x, 0);
  assertEq(c.y, 4);
  assertEq(c.z, 3);
  assertEq(c.w, 4);

  var a2 = int32x4(INT32_MAX,INT32_MIN,9,16)
  var c = SIMD.int32x4.select(sel_ttff,a2,b);
  assertEq(c.x, INT32_MAX);
  assertEq(c.y, INT32_MIN);
  assertEq(c.z, 3);
  assertEq(c.w, 4);

  var b2 = int32x4(1,2,INT32_MAX,INT32_MIN)
  var c = SIMD.int32x4.select(sel_ttff,a2,b2);
  assertEq(c.x, INT32_MAX);
  assertEq(c.y, INT32_MIN);
  assertEq(c.z, INT32_MAX);
  assertEq(c.w, INT32_MIN);

  if (typeof reportCompare === "function")
    reportCompare(true, true);
}

test();
