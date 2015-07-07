// |reftest| skip-if(!this.hasOwnProperty("SIMD"))

var float32x4 = SIMD.float32x4;
var int8x16 = SIMD.int8x16;
var int16x8 = SIMD.int16x8;
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

function test_int8x16() {
  var v, w;
  for ([v, w] of [[int8x16(-1, 20, 30, 4, -5, 6, 70, -80, 9, 100, -11, 12, 13, -14, 15, -16), 0b1010010010010001],
                  [int8x16(10, 2, 30.2, -4, -5.2, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16), 0b11000],
                  [int8x16(0, INT8_MIN, INT8_MAX, -0, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16), 0b10]])
  {
      assertEq(v.signMask, w);
  }

  if (typeof reportCompare === "function")
    reportCompare(true, true);
}

function test_int16x8() {
  var v, w;
  for ([v, w] of [[int16x8(-1, 20, 30, 4, -5, 6, 70, -80), 0b10010001],
                  [int16x8(10, 2, 30.2, -4, -5.2, 6, 7, 8), 0b11000],
                  [int16x8(0, INT16_MIN, INT16_MAX, -0, 5, 6, 7, 8), 0b10]])
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
test_int8x16();
test_int16x8();
test_int32x4();
