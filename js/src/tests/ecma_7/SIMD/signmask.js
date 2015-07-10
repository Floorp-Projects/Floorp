// |reftest| skip-if(!this.hasOwnProperty("SIMD"))

var Float32x4 = SIMD.Float32x4;
var Int8x16 = SIMD.Int8x16;
var Int16x8 = SIMD.Int16x8;
var Int32x4 = SIMD.Int32x4;

function test_Float32x4() {
  var v, w;
  for ([v, w] of [[Float32x4(-1, 20, 30, 4), 0b0001],
                  [Float32x4(9.999, 2.1234, 30.4443, -4), 0b1000],
                  [Float32x4(0, -Infinity, +Infinity, -0), 0b1010]])
  {
      assertEq(v.signMask, w);
  }

  if (typeof reportCompare === "function")
    reportCompare(true, true);
}

function test_Int8x16() {
  var v, w;
  for ([v, w] of [[Int8x16(-1, 20, 30, 4, -5, 6, 70, -80, 9, 100, -11, 12, 13, -14, 15, -16), 0b1010010010010001],
                  [Int8x16(10, 2, 30.2, -4, -5.2, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16), 0b11000],
                  [Int8x16(0, INT8_MIN, INT8_MAX, -0, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16), 0b10]])
  {
      assertEq(v.signMask, w);
  }

  if (typeof reportCompare === "function")
    reportCompare(true, true);
}

function test_Int16x8() {
  var v, w;
  for ([v, w] of [[Int16x8(-1, 20, 30, 4, -5, 6, 70, -80), 0b10010001],
                  [Int16x8(10, 2, 30.2, -4, -5.2, 6, 7, 8), 0b11000],
                  [Int16x8(0, INT16_MIN, INT16_MAX, -0, 5, 6, 7, 8), 0b10]])
  {
      assertEq(v.signMask, w);
  }

  if (typeof reportCompare === "function")
    reportCompare(true, true);
}

function test_Int32x4() {
  var v, w;
  for ([v, w] of [[Int32x4(-1, 20, 30, 4), 0b0001],
                  [Int32x4(10, 2, 30.2, -4), 0b1000],
                  [Int32x4(0, 0x80000000, 0x7fffffff, -0), 0b0010]])
  {
      assertEq(v.signMask, w);
  }

  if (typeof reportCompare === "function")
    reportCompare(true, true);
}

test_Float32x4();
test_Int8x16();
test_Int16x8();
test_Int32x4();
