// |reftest| skip-if(!this.hasOwnProperty("SIMD"))

function test() {

  var i4 = SIMD.Int32x4(1,2,3,4);
  var i8 = SIMD.Int16x8(1,2,3,4,5,6,7,8);
  var i16 = SIMD.Int8x16(1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16);
  var u4 = SIMD.Uint32x4(1,2,3,4);
  var u8 = SIMD.Uint16x8(1,2,3,4,5,6,7,8);
  var u16 = SIMD.Uint8x16(1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16);
  var f4 = SIMD.Float32x4(NaN, -0, Infinity, 13.37);
  var f2 = SIMD.Float64x2(-0, 13.37);
  var b2 = SIMD.Bool64x2(true, false);
  var b4 = SIMD.Bool32x4(true, true, false, false);
  var b8 = SIMD.Bool16x8(true, true, false, false, true, true, false, false);
  var b16 = SIMD.Bool8x16(true, true, false, false, true, true, false, false, true, true, false, false, true, true, false, false);

  var ci4 = SIMD.Int32x4.check(i4);
  assertEqX4(ci4, simdToArray(i4));
  assertThrowsInstanceOf(() => SIMD.Int32x4.check(f4), TypeError);
  assertThrowsInstanceOf(() => SIMD.Int32x4.check(f2), TypeError);
  assertThrowsInstanceOf(() => SIMD.Int32x4.check(i8), TypeError);
  assertThrowsInstanceOf(() => SIMD.Int32x4.check(i16), TypeError);
  assertThrowsInstanceOf(() => SIMD.Int32x4.check(u4), TypeError);
  assertThrowsInstanceOf(() => SIMD.Int32x4.check(u8), TypeError);
  assertThrowsInstanceOf(() => SIMD.Int32x4.check(u16), TypeError);
  assertThrowsInstanceOf(() => SIMD.Int32x4.check(b2), TypeError);
  assertThrowsInstanceOf(() => SIMD.Int32x4.check(b4), TypeError);
  assertThrowsInstanceOf(() => SIMD.Int32x4.check(b8), TypeError);
  assertThrowsInstanceOf(() => SIMD.Int32x4.check(b16), TypeError);
  assertThrowsInstanceOf(() => SIMD.Int32x4.check("i swear i'm a vector"), TypeError);
  assertThrowsInstanceOf(() => SIMD.Int32x4.check({}), TypeError);

  var ci8 = SIMD.Int16x8.check(i8);
  assertEqX8(ci8, simdToArray(i8));
  assertThrowsInstanceOf(() => SIMD.Int16x8.check(i4), TypeError);
  assertThrowsInstanceOf(() => SIMD.Int16x8.check(i16), TypeError);
  assertThrowsInstanceOf(() => SIMD.Int16x8.check(u4), TypeError);
  assertThrowsInstanceOf(() => SIMD.Int16x8.check(u8), TypeError);
  assertThrowsInstanceOf(() => SIMD.Int16x8.check(u16), TypeError);
  assertThrowsInstanceOf(() => SIMD.Int16x8.check(f4), TypeError);
  assertThrowsInstanceOf(() => SIMD.Int16x8.check(f2), TypeError);
  assertThrowsInstanceOf(() => SIMD.Int16x8.check(b2), TypeError);
  assertThrowsInstanceOf(() => SIMD.Int16x8.check(b4), TypeError);
  assertThrowsInstanceOf(() => SIMD.Int16x8.check(b8), TypeError);
  assertThrowsInstanceOf(() => SIMD.Int16x8.check(b16), TypeError);
  assertThrowsInstanceOf(() => SIMD.Int16x8.check("i swear i'm a vector"), TypeError);
  assertThrowsInstanceOf(() => SIMD.Int16x8.check({}), TypeError);

  var ci16 = SIMD.Int8x16.check(i16);
  assertEqX16(ci16, simdToArray(i16));
  assertThrowsInstanceOf(() => SIMD.Int8x16.check(i4), TypeError);
  assertThrowsInstanceOf(() => SIMD.Int8x16.check(i8), TypeError);
  assertThrowsInstanceOf(() => SIMD.Int8x16.check(u4), TypeError);
  assertThrowsInstanceOf(() => SIMD.Int8x16.check(u8), TypeError);
  assertThrowsInstanceOf(() => SIMD.Int8x16.check(u16), TypeError);
  assertThrowsInstanceOf(() => SIMD.Int8x16.check(f4), TypeError);
  assertThrowsInstanceOf(() => SIMD.Int8x16.check(f2), TypeError);
  assertThrowsInstanceOf(() => SIMD.Int8x16.check(b2), TypeError);
  assertThrowsInstanceOf(() => SIMD.Int8x16.check(b4), TypeError);
  assertThrowsInstanceOf(() => SIMD.Int8x16.check(b8), TypeError);
  assertThrowsInstanceOf(() => SIMD.Int8x16.check(b16), TypeError);
  assertThrowsInstanceOf(() => SIMD.Int8x16.check("i swear i'm a vector"), TypeError);
  assertThrowsInstanceOf(() => SIMD.Int8x16.check({}), TypeError);

  var cu4 = SIMD.Uint32x4.check(u4);
  assertEqX4(cu4, simdToArray(u4));
  assertThrowsInstanceOf(() => SIMD.Uint32x4.check(f4), TypeError);
  assertThrowsInstanceOf(() => SIMD.Uint32x4.check(f2), TypeError);
  assertThrowsInstanceOf(() => SIMD.Uint32x4.check(i4), TypeError);
  assertThrowsInstanceOf(() => SIMD.Uint32x4.check(i8), TypeError);
  assertThrowsInstanceOf(() => SIMD.Uint32x4.check(i16), TypeError);
  assertThrowsInstanceOf(() => SIMD.Uint32x4.check(u8), TypeError);
  assertThrowsInstanceOf(() => SIMD.Uint32x4.check(u16), TypeError);
  assertThrowsInstanceOf(() => SIMD.Uint32x4.check(b2), TypeError);
  assertThrowsInstanceOf(() => SIMD.Uint32x4.check(b4), TypeError);
  assertThrowsInstanceOf(() => SIMD.Uint32x4.check(b8), TypeError);
  assertThrowsInstanceOf(() => SIMD.Uint32x4.check(b16), TypeError);
  assertThrowsInstanceOf(() => SIMD.Uint32x4.check("i swear i'm a vector"), TypeError);
  assertThrowsInstanceOf(() => SIMD.Uint32x4.check({}), TypeError);

  var cu8 = SIMD.Uint16x8.check(u8);
  assertEqX8(cu8, simdToArray(u8));
  assertThrowsInstanceOf(() => SIMD.Uint16x8.check(i4), TypeError);
  assertThrowsInstanceOf(() => SIMD.Uint16x8.check(i8), TypeError);
  assertThrowsInstanceOf(() => SIMD.Uint16x8.check(i16), TypeError);
  assertThrowsInstanceOf(() => SIMD.Uint16x8.check(u4), TypeError);
  assertThrowsInstanceOf(() => SIMD.Uint16x8.check(u16), TypeError);
  assertThrowsInstanceOf(() => SIMD.Uint16x8.check(f4), TypeError);
  assertThrowsInstanceOf(() => SIMD.Uint16x8.check(f2), TypeError);
  assertThrowsInstanceOf(() => SIMD.Uint16x8.check(b2), TypeError);
  assertThrowsInstanceOf(() => SIMD.Uint16x8.check(b4), TypeError);
  assertThrowsInstanceOf(() => SIMD.Uint16x8.check(b8), TypeError);
  assertThrowsInstanceOf(() => SIMD.Uint16x8.check(b16), TypeError);
  assertThrowsInstanceOf(() => SIMD.Uint16x8.check("i swear i'm a vector"), TypeError);
  assertThrowsInstanceOf(() => SIMD.Uint16x8.check({}), TypeError);

  var cu16 = SIMD.Uint8x16.check(u16);
  assertEqX16(cu16, simdToArray(u16));
  assertThrowsInstanceOf(() => SIMD.Uint8x16.check(i4), TypeError);
  assertThrowsInstanceOf(() => SIMD.Uint8x16.check(i8), TypeError);
  assertThrowsInstanceOf(() => SIMD.Uint8x16.check(i16), TypeError);
  assertThrowsInstanceOf(() => SIMD.Uint8x16.check(u4), TypeError);
  assertThrowsInstanceOf(() => SIMD.Uint8x16.check(u8), TypeError);
  assertThrowsInstanceOf(() => SIMD.Uint8x16.check(f4), TypeError);
  assertThrowsInstanceOf(() => SIMD.Uint8x16.check(f2), TypeError);
  assertThrowsInstanceOf(() => SIMD.Uint8x16.check(b2), TypeError);
  assertThrowsInstanceOf(() => SIMD.Uint8x16.check(b4), TypeError);
  assertThrowsInstanceOf(() => SIMD.Uint8x16.check(b8), TypeError);
  assertThrowsInstanceOf(() => SIMD.Uint8x16.check(b16), TypeError);
  assertThrowsInstanceOf(() => SIMD.Uint8x16.check("i swear i'm a vector"), TypeError);
  assertThrowsInstanceOf(() => SIMD.Uint8x16.check({}), TypeError);

  var cf4 = SIMD.Float32x4.check(f4);
  assertEqX4(cf4, simdToArray(f4));
  assertThrowsInstanceOf(() => SIMD.Float32x4.check(i4), TypeError);
  assertThrowsInstanceOf(() => SIMD.Float32x4.check(i8), TypeError);
  assertThrowsInstanceOf(() => SIMD.Float32x4.check(i16), TypeError);
  assertThrowsInstanceOf(() => SIMD.Float32x4.check(u4), TypeError);
  assertThrowsInstanceOf(() => SIMD.Float32x4.check(u8), TypeError);
  assertThrowsInstanceOf(() => SIMD.Float32x4.check(u16), TypeError);
  assertThrowsInstanceOf(() => SIMD.Float32x4.check(f2), TypeError);
  assertThrowsInstanceOf(() => SIMD.Float32x4.check(b2), TypeError);
  assertThrowsInstanceOf(() => SIMD.Float32x4.check(b4), TypeError);
  assertThrowsInstanceOf(() => SIMD.Float32x4.check(b8), TypeError);
  assertThrowsInstanceOf(() => SIMD.Float32x4.check(b16), TypeError);
  assertThrowsInstanceOf(() => SIMD.Float32x4.check("i swear i'm a vector"), TypeError);
  assertThrowsInstanceOf(() => SIMD.Float32x4.check({}), TypeError);

  var cf2 = SIMD.Float64x2.check(f2);
  assertEqX2(cf2, simdToArray(f2));
  assertThrowsInstanceOf(() => SIMD.Float64x2.check(f4), TypeError);
  assertThrowsInstanceOf(() => SIMD.Float64x2.check(i4), TypeError);
  assertThrowsInstanceOf(() => SIMD.Float64x2.check(u4), TypeError);
  assertThrowsInstanceOf(() => SIMD.Float64x2.check(u8), TypeError);
  assertThrowsInstanceOf(() => SIMD.Float64x2.check(u16), TypeError);
  assertThrowsInstanceOf(() => SIMD.Float64x2.check(i8), TypeError);
  assertThrowsInstanceOf(() => SIMD.Float64x2.check(i16), TypeError);
  assertThrowsInstanceOf(() => SIMD.Float64x2.check(b2), TypeError);
  assertThrowsInstanceOf(() => SIMD.Float64x2.check(b4), TypeError);
  assertThrowsInstanceOf(() => SIMD.Float64x2.check(b8), TypeError);
  assertThrowsInstanceOf(() => SIMD.Float64x2.check(b16), TypeError);
  assertThrowsInstanceOf(() => SIMD.Float64x2.check("i swear i'm a vector"), TypeError);
  assertThrowsInstanceOf(() => SIMD.Float64x2.check({}), TypeError);

  var cb2 = SIMD.Bool64x2.check(b2);
  assertEqX2(cb2, simdToArray(b2));
  assertThrowsInstanceOf(() => SIMD.Bool64x2.check(f4), TypeError);
  assertThrowsInstanceOf(() => SIMD.Bool64x2.check(f2), TypeError);
  assertThrowsInstanceOf(() => SIMD.Bool64x2.check(i8), TypeError);
  assertThrowsInstanceOf(() => SIMD.Bool64x2.check(i16), TypeError);
  assertThrowsInstanceOf(() => SIMD.Bool64x2.check(u4), TypeError);
  assertThrowsInstanceOf(() => SIMD.Bool64x2.check(u8), TypeError);
  assertThrowsInstanceOf(() => SIMD.Bool64x2.check(u16), TypeError);
  assertThrowsInstanceOf(() => SIMD.Bool64x2.check(b4), TypeError);
  assertThrowsInstanceOf(() => SIMD.Bool64x2.check(b8), TypeError);
  assertThrowsInstanceOf(() => SIMD.Bool64x2.check(b16), TypeError);
  assertThrowsInstanceOf(() => SIMD.Bool64x2.check("i swear i'm a vector"), TypeError);
  assertThrowsInstanceOf(() => SIMD.Bool64x2.check({}), TypeError);

  var cb4 = SIMD.Bool32x4.check(b4);
  assertEqX4(cb4, simdToArray(b4));
  assertThrowsInstanceOf(() => SIMD.Bool32x4.check(f4), TypeError);
  assertThrowsInstanceOf(() => SIMD.Bool32x4.check(f2), TypeError);
  assertThrowsInstanceOf(() => SIMD.Bool32x4.check(i4), TypeError);
  assertThrowsInstanceOf(() => SIMD.Bool32x4.check(i8), TypeError);
  assertThrowsInstanceOf(() => SIMD.Bool32x4.check(i16), TypeError);
  assertThrowsInstanceOf(() => SIMD.Bool32x4.check(u4), TypeError);
  assertThrowsInstanceOf(() => SIMD.Bool32x4.check(u8), TypeError);
  assertThrowsInstanceOf(() => SIMD.Bool32x4.check(u16), TypeError);
  assertThrowsInstanceOf(() => SIMD.Bool32x4.check(b2), TypeError);
  assertThrowsInstanceOf(() => SIMD.Bool32x4.check(b8), TypeError);
  assertThrowsInstanceOf(() => SIMD.Bool32x4.check(b16), TypeError);
  assertThrowsInstanceOf(() => SIMD.Bool32x4.check("i swear i'm a vector"), TypeError);
  assertThrowsInstanceOf(() => SIMD.Bool32x4.check({}), TypeError);

  var cb8 = SIMD.Bool16x8.check(b8);
  assertEqX8(cb8, simdToArray(b8));
  assertThrowsInstanceOf(() => SIMD.Bool16x8.check(f4), TypeError);
  assertThrowsInstanceOf(() => SIMD.Bool16x8.check(f2), TypeError);
  assertThrowsInstanceOf(() => SIMD.Bool16x8.check(i4), TypeError);
  assertThrowsInstanceOf(() => SIMD.Bool16x8.check(i8), TypeError);
  assertThrowsInstanceOf(() => SIMD.Bool16x8.check(i16), TypeError);
  assertThrowsInstanceOf(() => SIMD.Bool16x8.check(u4), TypeError);
  assertThrowsInstanceOf(() => SIMD.Bool16x8.check(u8), TypeError);
  assertThrowsInstanceOf(() => SIMD.Bool16x8.check(u16), TypeError);
  assertThrowsInstanceOf(() => SIMD.Bool16x8.check(b2), TypeError);
  assertThrowsInstanceOf(() => SIMD.Bool16x8.check(b4), TypeError);
  assertThrowsInstanceOf(() => SIMD.Bool16x8.check(b16), TypeError);
  assertThrowsInstanceOf(() => SIMD.Bool16x8.check("i swear i'm a vector"), TypeError);
  assertThrowsInstanceOf(() => SIMD.Bool16x8.check({}), TypeError);

  var cb16 = SIMD.Bool8x16.check(b16);
  assertEqX16(cb16, simdToArray(b16));
  assertThrowsInstanceOf(() => SIMD.Bool8x16.check(f4), TypeError);
  assertThrowsInstanceOf(() => SIMD.Bool8x16.check(f2), TypeError);
  assertThrowsInstanceOf(() => SIMD.Bool8x16.check(i4), TypeError);
  assertThrowsInstanceOf(() => SIMD.Bool8x16.check(i8), TypeError);
  assertThrowsInstanceOf(() => SIMD.Bool8x16.check(i16), TypeError);
  assertThrowsInstanceOf(() => SIMD.Bool8x16.check(u4), TypeError);
  assertThrowsInstanceOf(() => SIMD.Bool8x16.check(u8), TypeError);
  assertThrowsInstanceOf(() => SIMD.Bool8x16.check(u16), TypeError);
  assertThrowsInstanceOf(() => SIMD.Bool8x16.check(b2), TypeError);
  assertThrowsInstanceOf(() => SIMD.Bool8x16.check(b4), TypeError);
  assertThrowsInstanceOf(() => SIMD.Bool8x16.check(b8), TypeError);
  assertThrowsInstanceOf(() => SIMD.Bool8x16.check("i swear i'm a vector"), TypeError);
  assertThrowsInstanceOf(() => SIMD.Bool8x16.check({}), TypeError);

  if (typeof reportCompare === "function")
    reportCompare(true, true);
}

test();

