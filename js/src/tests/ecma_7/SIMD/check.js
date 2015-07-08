// |reftest| skip-if(!this.hasOwnProperty("SIMD"))

function test() {

  var i4 = SIMD.int32x4(1,2,3,4);
  var i8 = SIMD.int16x8(1,2,3,4,5,6,7,8);
  var i16 = SIMD.int8x16(1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16);
  var f4 = SIMD.float32x4(NaN, -0, Infinity, 13.37);
  var f2 = SIMD.float64x2(-0, 13.37);

  var ci4 = SIMD.int32x4.check(i4);
  assertEqX4(ci4, simdToArray(i4));
  assertThrowsInstanceOf(() => SIMD.int32x4.check(f4), TypeError);
  assertThrowsInstanceOf(() => SIMD.int32x4.check(f2), TypeError);
  assertThrowsInstanceOf(() => SIMD.int32x4.check(i8), TypeError);
  assertThrowsInstanceOf(() => SIMD.int32x4.check(i16), TypeError);
  assertThrowsInstanceOf(() => SIMD.int32x4.check("i swear i'm a vector"), TypeError);
  assertThrowsInstanceOf(() => SIMD.int32x4.check({}), TypeError);

  var ci8 = SIMD.int16x8.check(i8);
  assertEqX8(ci8, simdToArray(i8));
  assertThrowsInstanceOf(() => SIMD.int16x8.check(i4), TypeError);
  assertThrowsInstanceOf(() => SIMD.int16x8.check(i16), TypeError);
  assertThrowsInstanceOf(() => SIMD.int16x8.check(f4), TypeError);
  assertThrowsInstanceOf(() => SIMD.int16x8.check(f2), TypeError);
  assertThrowsInstanceOf(() => SIMD.int16x8.check("i swear i'm a vector"), TypeError);
  assertThrowsInstanceOf(() => SIMD.int16x8.check({}), TypeError);

  var ci16 = SIMD.int8x16.check(i16);
  assertEqX16(ci16, simdToArray(i16));
  assertThrowsInstanceOf(() => SIMD.int8x16.check(i4), TypeError);
  assertThrowsInstanceOf(() => SIMD.int8x16.check(i8), TypeError);
  assertThrowsInstanceOf(() => SIMD.int8x16.check(f4), TypeError);
  assertThrowsInstanceOf(() => SIMD.int8x16.check(f2), TypeError);
  assertThrowsInstanceOf(() => SIMD.int8x16.check("i swear i'm a vector"), TypeError);
  assertThrowsInstanceOf(() => SIMD.int8x16.check({}), TypeError);

  var cf4 = SIMD.float32x4.check(f4);
  assertEqX4(cf4, simdToArray(f4));
  assertThrowsInstanceOf(() => SIMD.float32x4.check(i4), TypeError);
  assertThrowsInstanceOf(() => SIMD.float32x4.check(i8), TypeError);
  assertThrowsInstanceOf(() => SIMD.float32x4.check(i16), TypeError);
  assertThrowsInstanceOf(() => SIMD.float32x4.check(f2), TypeError);
  assertThrowsInstanceOf(() => SIMD.float32x4.check("i swear i'm a vector"), TypeError);
  assertThrowsInstanceOf(() => SIMD.float32x4.check({}), TypeError);

  var cf2 = SIMD.float64x2.check(f2);
  assertEqX2(cf2, simdToArray(f2));
  assertThrowsInstanceOf(() => SIMD.float64x2.check(f4), TypeError);
  assertThrowsInstanceOf(() => SIMD.float64x2.check(i4), TypeError);
  assertThrowsInstanceOf(() => SIMD.float64x2.check(i8), TypeError);
  assertThrowsInstanceOf(() => SIMD.float64x2.check(i16), TypeError);
  assertThrowsInstanceOf(() => SIMD.float64x2.check("i swear i'm a vector"), TypeError);
  assertThrowsInstanceOf(() => SIMD.float64x2.check({}), TypeError);

  if (typeof reportCompare === "function")
    reportCompare(true, true);
}

test();

