// |reftest| skip-if(!this.hasOwnProperty("SIMD"))

var float32x4 = SIMD.float32x4;
var int32x4 = SIMD.int32x4;

function test_float32x4() {
  var v, w;
  for ([v, w] of [[float32x4(-1, 20, 30, 4), 0b0001],
                  [float32x4(9.999, 2.1234, 30.4443, -4), 0b1000],
                  [float32x4(0, -Infinity, +Infinity, -0), 0b1010]])
  {
      assertEq(v.signMask, w);
  }

  if (typeof reportCompare === "function")
    reportCompare(true, true);
}

function test_int32x4() {
  var v, w;
  for ([v, w] of [[int32x4(-1, 20, 30, 4), 0b0001],
                  [int32x4(10, 2, 30.2, -4), 0b1000],
                  [int32x4(0, 0x80000000, 0x7fffffff, -0), 0b0010]])
  {
      assertEq(v.signMask, w);
  }

  if (typeof reportCompare === "function")
    reportCompare(true, true);
}

test_float32x4();
test_int32x4();
