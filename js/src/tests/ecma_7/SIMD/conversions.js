// |reftest| skip-if(!this.hasOwnProperty("SIMD"))
var Float32x4 = SIMD.Float32x4;
var Float64x2 = SIMD.Float64x2;
var Int8x16 = SIMD.Int8x16;
var Int16x8 = SIMD.Int16x8;
var Int32x4 = SIMD.Int32x4;
var Uint8x16 = SIMD.Uint8x16;
var Uint16x8 = SIMD.Uint16x8;
var Uint32x4 = SIMD.Uint32x4;

function testFloat32x4FromFloat64x2Bits() {
  var valsExp = [
    [[2.000000473111868, 512.0001225471497], [1.0, 2.0, 3.0, 4.0]],
    [[-0, NaN], [0, -0, 0, NaN]],
    [[Infinity, -Infinity], [0, NaN, 0, NaN]]
  ];

  for (var [v,w] of valsExp) {
    assertEqX4(Float32x4.fromFloat64x2Bits(Float64x2(...v)), w);
  }
}

function testFloat32x4FromInt8x16Bits() {
  function expected(v, Buffer) {
    var i8 = new Int8Array(new Buffer(16));
    var f32 = new Float32Array(i8.buffer);
    var asArr = simdToArray(v);
    for (var i = 0; i < 16; i++) i8[i] = asArr[i];
    return [f32[0], f32[1], f32[2], f32[3]];
  }

  var vals = [[1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16],
              [INT8_MIN, INT8_MAX, INT8_MIN, INT8_MAX, INT8_MIN, INT8_MAX, INT8_MIN, INT8_MAX,
               INT8_MAX, INT8_MIN, INT8_MAX, INT8_MIN, INT8_MAX, INT8_MIN, INT8_MAX, INT8_MIN]];
  for (var v of vals) {
    var i = Int8x16(...v);
    assertEqX4(Float32x4.fromInt8x16Bits(i), expected(i, ArrayBuffer));
    if (typeof SharedArrayBuffer != "undefined")
      assertEqX4(Float32x4.fromInt8x16Bits(i), expected(i, SharedArrayBuffer));
  }
}

function testFloat32x4FromUint8x16Bits() {
  function expected(v, Buffer) {
    var u8 = new Uint8Array(new Buffer(16));
    var f32 = new Float32Array(u8.buffer);
    var asArr = simdToArray(v);
    for (var i = 0; i < 16; i++) u8[i] = asArr[i];
    return [f32[0], f32[1], f32[2], f32[3]];
  }

  var vals = [[1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16],
              [0, UINT8_MAX, 0, UINT8_MAX, 0, UINT8_MAX, 0, UINT8_MAX,
               UINT8_MAX, 0, UINT8_MAX, 0, UINT8_MAX, 0, UINT8_MAX, 0]];
  for (var v of vals) {
    var i = Uint8x16(...v);
    assertEqX4(Float32x4.fromUint8x16Bits(i), expected(i, ArrayBuffer));
    if (typeof SharedArrayBuffer != "undefined")
      assertEqX4(Float32x4.fromUint8x16Bits(i), expected(i, SharedArrayBuffer));
  }
}

function testFloat32x4FromInt16x8Bits() {
  function expected(v, Buffer) {
    var i16 = new Int16Array(new Buffer(16));
    var f32 = new Float32Array(i16.buffer);
    var asArr = simdToArray(v);
    for (var i = 0; i < 8; i++) i16[i] = asArr[i];
    return [f32[0], f32[1], f32[2], f32[3]];
  }

  var vals = [[1, 2, 3, 4, 5, 6, 7, 8],
              [INT16_MIN, INT16_MAX, INT16_MIN, INT16_MAX, INT16_MIN, INT16_MAX, INT16_MIN, INT16_MAX]];
  for (var v of vals) {
    var i = Int16x8(...v);
    assertEqX4(Float32x4.fromInt16x8Bits(i), expected(i, ArrayBuffer));
    if (typeof SharedArrayBuffer != "undefined")
      assertEqX4(Float32x4.fromInt16x8Bits(i), expected(i, SharedArrayBuffer));
  }
}

function testFloat32x4FromUint16x8Bits() {
  function expected(v, Buffer) {
    var u16 = new Uint16Array(new Buffer(16));
    var f32 = new Float32Array(u16.buffer);
    var asArr = simdToArray(v);
    for (var i = 0; i < 8; i++) u16[i] = asArr[i];
    return [f32[0], f32[1], f32[2], f32[3]];
  }

  var vals = [[1, 2, 3, 4, 5, 6, 7, 8],
              [0, UINT16_MAX, 0, UINT16_MAX, 0, UINT16_MAX, 0, UINT16_MAX]];
  for (var v of vals) {
    var i = Uint16x8(...v);
    assertEqX4(Float32x4.fromUint16x8Bits(i), expected(i, ArrayBuffer));
    if (typeof SharedArrayBuffer != "undefined")
      assertEqX4(Float32x4.fromUint16x8Bits(i), expected(i, SharedArrayBuffer));
  }
}

function testFloat32x4FromInt32x4() {
  function expected(v) {
    return v.map(Math.fround);
  }
  var vals = [
    [1, 2, 3, 4],
    [INT32_MIN, INT32_MAX, Math.pow(2, 30) - 1, -Math.pow(2, 30)]
  ];

  for (var v of vals) {
    assertEqX4(Float32x4.fromInt32x4(Int32x4(...v)), expected(v));
  }

  // Check that rounding to nearest, even is applied.
  {
      var num = makeFloat(0, 150 + 2, 0);
      var next = makeFloat(0, 150 + 2, 1);
      assertEq(num + 4, next);

      v = Float32x4.fromInt32x4(Int32x4(num, num + 1, num + 2, num + 3));
      assertEqX4(v, [num, num, /* even */ num, next]);
  }

  {
      var num = makeFloat(0, 150 + 2, 1);
      var next = makeFloat(0, 150 + 2, 2);
      assertEq(num + 4, next);

      v = Float32x4.fromInt32x4(Int32x4(num, num + 1, num + 2, num + 3));
      assertEqX4(v, [num, num, /* even */ next, next]);
  }

  {
      var last = makeFloat(0, 157, 0x7fffff);

      assertEq(last, Math.fround(last), "float");
      assertEq(last < Math.pow(2, 31), true, "less than 2**31");
      assertEq(last | 0, last, "it should be an integer, as exponent >= 150");

      var diff = (Math.pow(2, 31) - 1) - last;
      v = Float32x4.fromInt32x4(Int32x4(Math.pow(2, 31) - 1,
                                        Math.pow(2, 30) + 1,
                                        last + (diff / 2) | 0,      // nearest is last
                                        last + (diff / 2) + 1 | 0   // nearest is Math.pow(2, 31)
                                ));
      assertEqX4(v, [Math.pow(2, 31),
                     Math.pow(2, 30),
                     last,
                     Math.pow(2, 31)
                    ]);
  }
}

function testFloat32x4FromUint32x4() {
  function expected(v) {
    return v.map(Math.fround);
  }
  var vals = [
    [1, 2, 3, 4],
    [0, UINT32_MAX, Math.pow(2, 30) - 1, Math.pow(2, 31)]
  ];

  for (var v of vals) {
    assertEqX4(Float32x4.fromUint32x4(Uint32x4(...v)), expected(v));
  }

  // Check that rounding to nearest, even is applied.
  {
      var num = makeFloat(0, 150 + 2, 0);
      var next = makeFloat(0, 150 + 2, 1);
      assertEq(num + 4, next);

      v = Float32x4.fromUint32x4(Uint32x4(num, num + 1, num + 2, num + 3));
      assertEqX4(v, [num, num, /* even */ num, next]);
  }

  {
      var num = makeFloat(0, 150 + 2, 1);
      var next = makeFloat(0, 150 + 2, 2);
      assertEq(num + 4, next);

      v = Float32x4.fromUint32x4(Uint32x4(num, num + 1, num + 2, num + 3));
      assertEqX4(v, [num, num, /* even */ next, next]);
  }

  {
      var last = makeFloat(0, 157, 0x7fffff);

      assertEq(last, Math.fround(last), "float");
      assertEq(last < Math.pow(2, 31), true, "less than 2**31");
      assertEq(last | 0, last, "it should be an integer, as exponent >= 150");

      var diff = (Math.pow(2, 31) - 1) - last;
      v = Float32x4.fromUint32x4(Uint32x4(Math.pow(2, 31) - 1,
                                        Math.pow(2, 30) + 1,
                                        last + (diff / 2) | 0,      // nearest is last
                                        last + (diff / 2) + 1 | 0   // nearest is Math.pow(2, 31)
                                ));
      assertEqX4(v, [Math.pow(2, 31),
                     Math.pow(2, 30),
                     last,
                     Math.pow(2, 31)
                    ]);
  }
}

function testFloat32x4FromInt32x4Bits() {
  var valsExp = [
    [[100, 200, 300, 400], [1.401298464324817e-43, 2.802596928649634e-43, 4.203895392974451e-43, 5.605193857299268e-43]],
    [[INT32_MIN, INT32_MAX, 0, 0], [-0, NaN, 0, 0]]
  ];

  for (var [v,w] of valsExp) {
    assertEqX4(Float32x4.fromInt32x4Bits(Int32x4(...v)), w);
  }
}

function testFloat32x4FromUint32x4Bits() {
  var valsExp = [
    [[100, 200, 300, 400], [1.401298464324817e-43, 2.802596928649634e-43, 4.203895392974451e-43, 5.605193857299268e-43]],
    [[INT32_MIN, INT32_MAX, 0, 0], [-0, NaN, 0, 0]]
  ];

  for (var [v,w] of valsExp) {
    assertEqX4(Float32x4.fromUint32x4Bits(Uint32x4(...v)), w);
  }
}

function testFloat64x2FromFloat32x4Bits() {
  var valsExp = [
    [[0, 1.875, 0, 2], [1.0, 2.0]],
    [[NaN, -0, Infinity, -Infinity], [-1.058925634e-314, -1.404448428688076e+306]]
  ];

  for (var [v,w] of valsExp) {
    assertEqX2(Float64x2.fromFloat32x4Bits(Float32x4(...v)), w);
  }
}

function testFloat64x2FromInt8x16Bits() {
  function expected(v, Buffer) {
    var i8 = new Int8Array(new Buffer(16));
    var f64 = new Float64Array(i8.buffer);
    var asArr = simdToArray(v);
    for (var i = 0; i < 16; i++) i8[i] = asArr[i];
    return [f64[0], f64[1]];
  }

  var vals = [[1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16],
              [INT8_MIN, INT8_MAX, INT8_MIN, INT8_MAX, INT8_MIN, INT8_MAX, INT8_MIN, INT8_MAX,
               INT8_MAX, INT8_MIN, INT8_MAX, INT8_MIN, INT8_MAX, INT8_MIN, INT8_MAX, INT8_MIN]];

  for (var v of vals) {
    var f = Int8x16(...v);
    assertEqX2(Float64x2.fromInt8x16Bits(f), expected(f, ArrayBuffer));
    if (typeof SharedArrayBuffer != "undefined")
      assertEqX2(Float64x2.fromInt8x16Bits(f), expected(f, SharedArrayBuffer));
  }
}

function testFloat64x2FromUint8x16Bits() {
  function expected(v, Buffer) {
    var u8 = new Uint8Array(new Buffer(16));
    var f64 = new Float64Array(u8.buffer);
    var asArr = simdToArray(v);
    for (var i = 0; i < 16; i++) u8[i] = asArr[i];
    return [f64[0], f64[1]];
  }

  var vals = [[1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16],
              [0, UINT8_MAX, 0, UINT8_MAX, 0, UINT8_MAX, 0, UINT8_MAX,
               UINT8_MAX, 0, UINT8_MAX, 0, UINT8_MAX, 0, UINT8_MAX, 0]];

  for (var v of vals) {
    var f = Uint8x16(...v);
    assertEqX2(Float64x2.fromUint8x16Bits(f), expected(f, ArrayBuffer));
    if (typeof SharedArrayBuffer != "undefined")
      assertEqX2(Float64x2.fromUint8x16Bits(f), expected(f, SharedArrayBuffer));
  }
}

function testFloat64x2FromInt16x8Bits() {
  function expected(v, Buffer) {
    var i16 = new Int16Array(new Buffer(16));
    var f64 = new Float64Array(i16.buffer);
    var asArr = simdToArray(v);
    for (var i = 0; i < 8; i++) i16[i] = asArr[i];
    return [f64[0], f64[1]];
  }

  var vals = [[1, 2, 3, 4, 5, 6, 7, 8],
              [INT16_MIN, INT16_MAX, INT16_MIN, INT16_MAX, INT16_MIN, INT16_MAX, INT16_MIN, INT16_MAX]];

  for (var v of vals) {
    var f = Int16x8(...v);
    assertEqX2(Float64x2.fromInt16x8Bits(f), expected(f, ArrayBuffer));
    if (typeof SharedArrayBuffer != "undefined")
      assertEqX2(Float64x2.fromInt16x8Bits(f), expected(f, SharedArrayBuffer));
  }
}

function testFloat64x2FromUint16x8Bits() {
  function expected(v, Buffer) {
    var u16 = new Uint16Array(new Buffer(16));
    var f64 = new Float64Array(u16.buffer);
    var asArr = simdToArray(v);
    for (var i = 0; i < 8; i++) u16[i] = asArr[i];
    return [f64[0], f64[1]];
  }

  var vals = [[1, 2, 3, 4, 5, 6, 7, 8],
              [0, UINT16_MAX, 0, UINT16_MAX, 0, UINT16_MAX, 0, UINT16_MAX]];

  for (var v of vals) {
    var f = Uint16x8(...v);
    assertEqX2(Float64x2.fromUint16x8Bits(f), expected(f, ArrayBuffer));
    if (typeof SharedArrayBuffer != "undefined")
      assertEqX2(Float64x2.fromUint16x8Bits(f), expected(f, SharedArrayBuffer));
  }
}

function testFloat64x2FromInt32x4Bits() {
  var valsExp = [
    [[0x00000000, 0x3ff00000, 0x0000000, 0x40000000], [1.0, 2.0]],
    [[0xabcdef12, 0x3ff00000, 0x21fedcba, 0x40000000], [1.0000006400213732, 2.0000002532866263]]
  ];

  for (var [v,w] of valsExp) {
    assertEqX2(Float64x2.fromInt32x4Bits(Int32x4(...v)), w);
  }
}

function testFloat64x2FromUint32x4Bits() {
  var valsExp = [
    [[0x00000000, 0x3ff00000, 0x0000000, 0x40000000], [1.0, 2.0]],
    [[0xabcdef12, 0x3ff00000, 0x21fedcba, 0x40000000], [1.0000006400213732, 2.0000002532866263]]
  ];

  for (var [v,w] of valsExp) {
    assertEqX2(Float64x2.fromUint32x4Bits(Uint32x4(...v)), w);
  }
}

function testInt32x4FromFloat32x4() {
  var d = Float32x4(1.1, 2.2, 3.3, 4.6);
  assertEqX4(Int32x4.fromFloat32x4(d), [1, 2, 3, 4]);

  var d = Float32x4(NaN, 0, 0, 0);
  assertThrowsInstanceOf(() => SIMD.Int32x4.fromFloat32x4(d), RangeError);

  var d = Float32x4(Infinity, 0, 0, 0);
  assertThrowsInstanceOf(() => SIMD.Int32x4.fromFloat32x4(d), RangeError);

  var d = Float32x4(-Infinity, 0, 0, 0);
  assertThrowsInstanceOf(() => SIMD.Int32x4.fromFloat32x4(d), RangeError);

  // Test high boundaries: float(0, 157, 0x7fffff) < INT32_MAX < float(0, 158, 0)
  var d = Float32x4(makeFloat(0, 127 + 31, 0), 0, 0, 0);
  assertThrowsInstanceOf(() => SIMD.Int32x4.fromFloat32x4(d), RangeError);

  var lastFloat = makeFloat(0, 127 + 30, 0x7FFFFF);
  var d = Float32x4(lastFloat, 0, 0, 0);
  var e = SIMD.Int32x4.fromFloat32x4(d);
  assertEqX4(e, [lastFloat, 0, 0, 0]);

  // Test low boundaries
  assertEq(makeFloat(1, 127 + 31, 0), INT32_MIN);
  var d = Float32x4(makeFloat(1, 127 + 31, 0), 0, 0, 0);
  var e = SIMD.Int32x4.fromFloat32x4(d);
  assertEqX4(e, [INT32_MIN, 0, 0, 0]);

  var d = Float32x4(makeFloat(1, 127 + 31, 1), 0, 0, 0);
  assertThrowsInstanceOf(() => SIMD.Int32x4.fromFloat32x4(d), RangeError);
}

function testUint32x4FromFloat32x4() {
  var d = Float32x4(1.1, 2.2, 3.3, 4.6);
  assertEqX4(Uint32x4.fromFloat32x4(d), [1, 2, 3, 4]);

  var d = Float32x4(NaN, 0, 0, 0);
  assertThrowsInstanceOf(() => SIMD.Uint32x4.fromFloat32x4(d), RangeError);

  var d = Float32x4(Infinity, 0, 0, 0);
  assertThrowsInstanceOf(() => SIMD.Uint32x4.fromFloat32x4(d), RangeError);

  var d = Float32x4(-Infinity, 0, 0, 0);
  assertThrowsInstanceOf(() => SIMD.Uint32x4.fromFloat32x4(d), RangeError);

  // Test high boundaries: float(0, 158, 0x7fffff) < UINT32_MAX < float(0, 159, 0)
  var d = Float32x4(makeFloat(0, 127 + 32, 0), 0, 0, 0);
  assertThrowsInstanceOf(() => SIMD.Uint32x4.fromFloat32x4(d), RangeError);

  var lastFloat = makeFloat(0, 127 + 31, 0x7FFFFF);
  var d = Float32x4(lastFloat, 0, 0, 0);
  var e = SIMD.Uint32x4.fromFloat32x4(d);
  assertEqX4(e, [lastFloat, 0, 0, 0]);
}

function testInt32x4FromFloat32x4Bits() {
  var valsExp = [
    [[1, 2, 3, 4], [0x3f800000 | 0, 0x40000000 | 0, 0x40400000 | 0, 0x40800000 | 0]],
    [[NaN, -0, Infinity, -Infinity], [0x7fc00000 | 0, 0x80000000 | 0, 0x7f800000 | 0, 0xff800000 | 0]]
  ];

  for (var [v,w] of valsExp) {
    assertEqX4(Int32x4.fromFloat32x4Bits(Float32x4(...v)), w);
  }
}

function testUint32x4FromFloat32x4Bits() {
  var valsExp = [
    [[1, 2, 3, 4], [0x3f800000, 0x40000000, 0x40400000, 0x40800000]],
    [[NaN, -0, Infinity, -Infinity], [0x7fc00000, 0x80000000, 0x7f800000, 0xff800000]]
  ];

  for (var [v,w] of valsExp) {
    assertEqX4(Uint32x4.fromFloat32x4Bits(Float32x4(...v)), w);
  }
}

function testInt32x4FromFloat64x2Bits() {
  var valsExp = [
    [[1.0, 2.0], [0x00000000, 0x3FF00000, 0x00000000, 0x40000000]],
    [[+Infinity, -Infinity], [0x00000000, 0x7ff00000, 0x00000000, -0x100000]],
    [[-0, NaN], [0x00000000, -0x80000000, 0x00000000, 0x7ff80000]],
    [[1.0000006400213732, 2.0000002532866263], [-0x543210ee, 0x3ff00000, 0x21fedcba, 0x40000000]]
  ];

  for (var [v,w] of valsExp) {
    assertEqX4(Int32x4.fromFloat64x2Bits(Float64x2(...v)), w);
  }
}

function testUint32x4FromFloat64x2Bits() {
  var valsExp = [
    [[1.0, 2.0], [0x00000000, 0x3FF00000, 0x00000000, 0x40000000]],
    [[+Infinity, -Infinity], [0x00000000, 0x7ff00000, 0x00000000, 0xfff00000]],
    [[-0, NaN], [0x00000000, 0x80000000, 0x00000000, 0x7ff80000]],
    [[1.0000006400213732, 2.0000002532866263], [0xabcdef12, 0x3ff00000, 0x21fedcba, 0x40000000]]
  ];

  for (var [v,w] of valsExp) {
    assertEqX4(Uint32x4.fromFloat64x2Bits(Float64x2(...v)), w);
  }
}

function testInt32x4FromInt8x16Bits() {
  function expected(v, Buffer) {
    var i8 = new Int8Array(new Buffer(16));
    var i32 = new Int32Array(i8.buffer);
    var asArr = simdToArray(v);
    for (var i = 0; i < 16; i++) i8[i] = asArr[i];
    return [i32[0], i32[1], i32[2], i32[3]];
  }

  var vals = [[1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16],
              [INT8_MIN, INT8_MAX, INT8_MIN, INT8_MAX, INT8_MIN, INT8_MAX, INT8_MIN, INT8_MAX,
               INT8_MAX, INT8_MIN, INT8_MAX, INT8_MIN, INT8_MAX, INT8_MIN, INT8_MAX, INT8_MIN]];

  for (var v of vals) {
    var i = Int8x16(...v);
    assertEqX4(Int32x4.fromInt8x16Bits(i), expected(i, ArrayBuffer));
    if (typeof SharedArrayBuffer != "undefined")
      assertEqX4(Int32x4.fromInt8x16Bits(i), expected(i, SharedArrayBuffer));
  }
}

function testInt32x4FromUint8x16Bits() {
  function expected(v, Buffer) {
    var u8 = new Uint8Array(new Buffer(16));
    var i32 = new Int32Array(u8.buffer);
    var asArr = simdToArray(v);
    for (var i = 0; i < 16; i++) u8[i] = asArr[i];
    return [i32[0], i32[1], i32[2], i32[3]];
  }

  var vals = [[1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16],
              [0, UINT8_MAX, 0, UINT8_MAX, 0, UINT8_MAX, 0, UINT8_MAX,
               UINT8_MAX, 0, UINT8_MAX, 0, UINT8_MAX, 0, UINT8_MAX, 0]];

  for (var v of vals) {
    var i = Uint8x16(...v);
    assertEqX4(Int32x4.fromUint8x16Bits(i), expected(i, ArrayBuffer));
    if (typeof SharedArrayBuffer != "undefined")
      assertEqX4(Int32x4.fromUint8x16Bits(i), expected(i, SharedArrayBuffer));
  }
}

function testInt32x4FromInt16x8Bits() {
  function expected(v, Buffer) {
    var i16 = new Int16Array(new Buffer(16));
    var i32 = new Int32Array(i16.buffer);
    var asArr = simdToArray(v);
    for (var i = 0; i < 8; i++) i16[i] = asArr[i];
    return [i32[0], i32[1], i32[2], i32[3]];
  }

  var vals = [[1, 2, 3, 4, 5, 6, 7, 8],
              [INT16_MIN, INT16_MAX, INT16_MIN, INT16_MAX, INT16_MIN, INT16_MAX, INT16_MIN, INT16_MAX]];

  for (var v of vals) {
    var i = Int16x8(...v);
    assertEqX4(Int32x4.fromInt16x8Bits(i), expected(i, ArrayBuffer));
    if (typeof SharedArrayBuffer != "undefined")
      assertEqX4(Int32x4.fromInt16x8Bits(i), expected(i, SharedArrayBuffer));
  }
}

function testInt32x4FromUint16x8Bits() {
  function expected(v, Buffer) {
    var u16 = new Uint16Array(new Buffer(16));
    var i32 = new Int32Array(u16.buffer);
    var asArr = simdToArray(v);
    for (var i = 0; i < 8; i++) u16[i] = asArr[i];
    return [i32[0], i32[1], i32[2], i32[3]];
  }

  var vals = [[1, 2, 3, 4, 5, 6, 7, 8],
              [0, UINT16_MAX, 0, UINT16_MAX, 0, UINT16_MAX, 0, UINT16_MAX]];

  for (var v of vals) {
    var i = Uint16x8(...v);
    assertEqX4(Int32x4.fromUint16x8Bits(i), expected(i, ArrayBuffer));
    if (typeof SharedArrayBuffer != "undefined")
      assertEqX4(Int32x4.fromUint16x8Bits(i), expected(i, SharedArrayBuffer));
  }
}

function testInt32x4FromUint32x4Bits() {
  function expected(v, Buffer) {
    var u32 = new Uint32Array(new Buffer(16));
    var i32 = new Int32Array(u32.buffer);
    var asArr = simdToArray(v);
    for (var i = 0; i < 8; i++) u32[i] = asArr[i];
    return [i32[0], i32[1], i32[2], i32[3]];
  }

  var vals = [[0, 1, -2, 3], [INT8_MIN, UINT32_MAX, INT32_MIN, INT32_MAX]];

  for (var v of vals) {
    var i = Uint32x4(...v);
    assertEqX4(Int32x4.fromUint32x4Bits(i), expected(i, ArrayBuffer));
    if (typeof SharedArrayBuffer != "undefined")
      assertEqX4(Int32x4.fromUint32x4Bits(i), expected(i, SharedArrayBuffer));
  }
}

function testInt8x16FromFloat32x4Bits() {
  function expected(v, Buffer) {
    var f32 = new Float32Array(new Buffer(16));
    var i8 = new Int8Array(f32.buffer);
    var asArr = simdToArray(v);
    for (var i = 0; i < 4; i++) f32[i] = asArr[i];
    return [i8[0], i8[1], i8[2], i8[3], i8[4], i8[5], i8[6], i8[7],
            i8[8], i8[9], i8[10], i8[11], i8[12], i8[13], i8[14], i8[15]];
  }

  var vals = [[1, -2, 3, -4], [Infinity, -Infinity, NaN, -0]];

  for (var v of vals) {
    var f = Float32x4(...v);
    assertEqX16(Int8x16.fromFloat32x4Bits(f), expected(f, ArrayBuffer));
    if (typeof SharedArrayBuffer != "undefined")
      assertEqX16(Int8x16.fromFloat32x4Bits(f), expected(f, SharedArrayBuffer));
  }
}

function testInt8x16FromFloat64x2Bits() {
  function expected(v, Buffer) {
    var f64 = new Float64Array(new Buffer(16));
    var i8 = new Int8Array(f64.buffer);
    f64[0] = Float64x2.extractLane(v, 0);
    f64[1] = Float64x2.extractLane(v, 1);
    return [i8[0], i8[1], i8[2], i8[3], i8[4], i8[5], i8[6], i8[7],
            i8[8], i8[9], i8[10], i8[11], i8[12], i8[13], i8[14], i8[15]];
  }
  var vals = [[1, -2], [-3, 4], [Infinity, -Infinity], [NaN, -0]];

  for (var v of vals) {
    var f = Float64x2(...v);
    assertEqX16(Int8x16.fromFloat64x2Bits(f), expected(f, ArrayBuffer));
    if (typeof SharedArrayBuffer != "undefined")
      assertEqX16(Int8x16.fromFloat64x2Bits(f), expected(f, SharedArrayBuffer));
  }
}

function testInt8x16FromUint8x16Bits() {
  function expected(v, Buffer) {
    var u8 = new Uint8Array(new Buffer(16));
    var i8 = new Int8Array(u8.buffer);
    var asArr = simdToArray(v);
    for (var i = 0; i < 16; i++) u8[i] = asArr[i];
    return [i8[0], i8[1], i8[2], i8[3], i8[4], i8[5], i8[6], i8[7],
            i8[8], i8[9], i8[10], i8[11], i8[12], i8[13], i8[14], i8[15]];
  }

  var vals = [[0, 1, -2, 3, -4, 5, INT8_MIN, UINT8_MAX, -6, 7, -8, 9, -10, 11, -12, 13]];

  for (var v of vals) {
    var i = Uint8x16(...v);
    assertEqX16(Int8x16.fromUint8x16Bits(i), expected(i, ArrayBuffer));
    if (typeof SharedArrayBuffer != "undefined")
      assertEqX16(Int8x16.fromUint8x16Bits(i), expected(i, SharedArrayBuffer));
  }
}

function testInt8x16FromInt16x8Bits() {
  function expected(v, Buffer) {
    var i16 = new Int16Array(new Buffer(16));
    var i8 = new Int8Array(i16.buffer);
    var asArr = simdToArray(v);
    for (var i = 0; i < 8; i++) i16[i] = asArr[i];
    return [i8[0], i8[1], i8[2], i8[3], i8[4], i8[5], i8[6], i8[7],
            i8[8], i8[9], i8[10], i8[11], i8[12], i8[13], i8[14], i8[15]];
  }

  var vals = [[0, 1, -2, 3, INT8_MIN, INT8_MAX, INT16_MIN, INT16_MAX]];
  for (var v of vals) {
    var i = Int16x8(...v);
    assertEqX16(Int8x16.fromInt16x8Bits(i), expected(i, ArrayBuffer));
    if (typeof SharedArrayBuffer != "undefined")
      assertEqX16(Int8x16.fromInt16x8Bits(i), expected(i, SharedArrayBuffer));
  }
}

function testInt8x16FromUint16x8Bits() {
  function expected(v, Buffer) {
    var u16 = new Uint16Array(new Buffer(16));
    var i8 = new Int8Array(u16.buffer);
    var asArr = simdToArray(v);
    for (var i = 0; i < 8; i++) u16[i] = asArr[i];
    return [i8[0], i8[1], i8[2], i8[3], i8[4], i8[5], i8[6], i8[7],
            i8[8], i8[9], i8[10], i8[11], i8[12], i8[13], i8[14], i8[15]];
  }

  var vals = [[0, 1, -2, UINT16_MAX, INT8_MIN, INT8_MAX, INT16_MIN, INT16_MAX]];
  for (var v of vals) {
    var i = Uint16x8(...v);
    assertEqX16(Int8x16.fromUint16x8Bits(i), expected(i, ArrayBuffer));
    if (typeof SharedArrayBuffer != "undefined")
      assertEqX16(Int8x16.fromUint16x8Bits(i), expected(i, SharedArrayBuffer));
  }
}

function testInt8x16FromInt32x4Bits() {
  function expected(v, Buffer) {
    var i32 = new Int32Array(new Buffer(16));
    var i8 = new Int8Array(i32.buffer);
    var asArr = simdToArray(v);
    for (var i = 0; i < 4; i++) i32[i] = asArr[i];
    return [i8[0], i8[1], i8[2], i8[3], i8[4], i8[5], i8[6], i8[7],
            i8[8], i8[9], i8[10], i8[11], i8[12], i8[13], i8[14], i8[15]];
  }

  var vals = [[0, 1, -2, 3], [INT8_MIN, INT8_MAX, INT32_MIN, INT32_MAX]];
  for (var v of vals) {
    var i = Int32x4(...v);
    assertEqX16(Int8x16.fromInt32x4Bits(i), expected(i, ArrayBuffer));
    if (typeof SharedArrayBuffer != "undefined")
      assertEqX16(Int8x16.fromInt32x4Bits(i), expected(i, SharedArrayBuffer));
  }
}

function testInt8x16FromUint32x4Bits() {
  function expected(v, Buffer) {
    var u32 = new Uint32Array(new Buffer(16));
    var i8 = new Int8Array(u32.buffer);
    var asArr = simdToArray(v);
    for (var i = 0; i < 4; i++) u32[i] = asArr[i];
    return [i8[0], i8[1], i8[2], i8[3], i8[4], i8[5], i8[6], i8[7],
            i8[8], i8[9], i8[10], i8[11], i8[12], i8[13], i8[14], i8[15]];
  }

  var vals = [[0, 1, -2, 3], [INT8_MIN, INT8_MAX, INT32_MIN, INT32_MAX]];
  for (var v of vals) {
    var i = Uint32x4(...v);
    assertEqX16(Int8x16.fromUint32x4Bits(i), expected(i, ArrayBuffer));
    if (typeof SharedArrayBuffer != "undefined")
      assertEqX16(Int8x16.fromUint32x4Bits(i), expected(i, SharedArrayBuffer));
  }
}

function testInt16x8FromFloat32x4Bits() {
  function expected(v, Buffer) {
    var f32 = new Float32Array(new Buffer(16));
    var i16 = new Int16Array(f32.buffer);
    var asArr = simdToArray(v);
    for (var i = 0; i < 4; i++) f32[i] = asArr[i];
    return [i16[0], i16[1], i16[2], i16[3], i16[4], i16[5], i16[6], i16[7]];
  }

  var vals = [[1, -2, 3, -4], [Infinity, -Infinity, NaN, -0]];

  for (var v of vals) {
    var f = Float32x4(...v);
    assertEqX8(Int16x8.fromFloat32x4Bits(f), expected(f, ArrayBuffer));
    if (typeof SharedArrayBuffer != "undefined")
      assertEqX8(Int16x8.fromFloat32x4Bits(f), expected(f, SharedArrayBuffer));
  }
}

function testInt16x8FromFloat64x2Bits() {
  function expected(v, Buffer) {
    var f64 = new Float64Array(new Buffer(16));
    var i16 = new Int16Array(f64.buffer);
    f64[0] = Float64x2.extractLane(v, 0);
    f64[1] = Float64x2.extractLane(v, 1);
    return [i16[0], i16[1], i16[2], i16[3], i16[4], i16[5], i16[6], i16[7]];
  }

  var vals = [[1, -2], [-3, 4], [Infinity, -Infinity], [NaN, -0]];

  for (var v of vals) {
    var f = Float64x2(...v);
    assertEqX8(Int16x8.fromFloat64x2Bits(f), expected(f, ArrayBuffer));
    if (typeof SharedArrayBuffer != "undefined")
      assertEqX8(Int16x8.fromFloat64x2Bits(f), expected(f, SharedArrayBuffer));
  }
}

function testInt16x8FromInt8x16Bits() {
  function expected(v, Buffer) {
    var i8 = new Int8Array(new Buffer(16));
    var i16 = new Int16Array(i8.buffer);
    var asArr = simdToArray(v);
    for (var i = 0; i < 16; i++) i8[i] = asArr[i];
    return [i16[0], i16[1], i16[2], i16[3], i16[4], i16[5], i16[6], i16[7]];
  }

  var vals = [[0, 1, -2, 3, -4, 5, INT8_MIN, INT8_MAX, -6, 7, -8, 9, -10, 11, -12, 13]];

  for (var v of vals) {
    var i = Int8x16(...v);
    assertEqX8(Int16x8.fromInt8x16Bits(i), expected(i, ArrayBuffer));
    if (typeof SharedArrayBuffer != "undefined")
      assertEqX8(Int16x8.fromInt8x16Bits(i), expected(i, SharedArrayBuffer));
  }
}

function testInt16x8FromUint8x16Bits() {
  function expected(v, Buffer) {
    var u8 = new Uint8Array(new Buffer(16));
    var i16 = new Int16Array(u8.buffer);
    var asArr = simdToArray(v);
    for (var i = 0; i < 16; i++) u8[i] = asArr[i];
    return [i16[0], i16[1], i16[2], i16[3], i16[4], i16[5], i16[6], i16[7]];
  }

  var vals = [[0, 1, -2, 3, -4, UINT8_MAX, INT8_MIN, INT8_MAX, -6, 7, -8, 9, -10, 11, -12, 13]];

  for (var v of vals) {
    var i = Uint8x16(...v);
    assertEqX8(Int16x8.fromUint8x16Bits(i), expected(i, ArrayBuffer));
    if (typeof SharedArrayBuffer != "undefined")
      assertEqX8(Int16x8.fromUint8x16Bits(i), expected(i, SharedArrayBuffer));
  }
}

function testInt16x8FromUint16x8Bits() {
  function expected(v, Buffer) {
    var u16 = new Uint16Array(new Buffer(16));
    var i16 = new Int16Array(u16.buffer);
    var asArr = simdToArray(v);
    for (var i = 0; i < 8; i++) u16[i] = asArr[i];
    return [i16[0], i16[1], i16[2], i16[3], i16[4], i16[5], i16[6], i16[7]];
  }

  var vals = [[1, 2, 3, 4, 5, 6, 7, 8],
              [0, UINT16_MAX, 0, UINT16_MAX, 0, UINT16_MAX, 0, UINT16_MAX]];

  for (var v of vals) {
    var i = Uint16x8(...v);
    assertEqX8(Int16x8.fromUint16x8Bits(i), expected(i, ArrayBuffer));
    if (typeof SharedArrayBuffer != "undefined")
      assertEqX8(Int16x8.fromUint16x8Bits(i), expected(i, SharedArrayBuffer));
  }
}

function testInt16x8FromInt32x4Bits() {
  function expected(v, Buffer) {
    var i32 = new Int32Array(new Buffer(16));
    var i16 = new Int16Array(i32.buffer);
    var asArr = simdToArray(v);
    for (var i = 0; i < 4; i++) i32[i] = asArr[i];
    return [i16[0], i16[1], i16[2], i16[3], i16[4], i16[5], i16[6], i16[7]];
  }

  var vals = [[1, -2, -3, 4], [INT16_MAX, INT16_MIN, INT32_MAX, INT32_MIN]];

  for (var v of vals) {
    var i = Int32x4(...v);
    assertEqX8(Int16x8.fromInt32x4Bits(i), expected(i, ArrayBuffer));
    if (typeof SharedArrayBuffer != "undefined")
      assertEqX8(Int16x8.fromInt32x4Bits(i), expected(i, SharedArrayBuffer));
  }
}

function testInt16x8FromUint32x4Bits() {
  function expected(v, Buffer) {
    var u32 = new Uint32Array(new Buffer(16));
    var i16 = new Int16Array(u32.buffer);
    var asArr = simdToArray(v);
    for (var i = 0; i < 4; i++) u32[i] = asArr[i];
    return [i16[0], i16[1], i16[2], i16[3], i16[4], i16[5], i16[6], i16[7]];
  }

  var vals = [[1, -2, -3, 4], [INT16_MAX, INT16_MIN, INT32_MAX, INT32_MIN]];

  for (var v of vals) {
    var i = Uint32x4(...v);
    assertEqX8(Int16x8.fromUint32x4Bits(i), expected(i, ArrayBuffer));
    if (typeof SharedArrayBuffer != "undefined")
      assertEqX8(Int16x8.fromUint32x4Bits(i), expected(i, SharedArrayBuffer));
  }
}

function testUint32x4FromInt8x16Bits() {
  function expected(v, Buffer) {
    var i8 = new Int8Array(new Buffer(16));
    var u32 = new Uint32Array(i8.buffer);
    var asArr = simdToArray(v);
    for (var i = 0; i < 16; i++) i8[i] = asArr[i];
    return [u32[0], u32[1], u32[2], u32[3]];
  }

  var vals = [[1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16],
              [INT8_MIN, INT8_MAX, INT8_MIN, INT8_MAX, INT8_MIN, INT8_MAX, INT8_MIN, INT8_MAX,
               INT8_MAX, INT8_MIN, INT8_MAX, INT8_MIN, INT8_MAX, INT8_MIN, INT8_MAX, INT8_MIN]];

  for (var v of vals) {
    var i = Int8x16(...v);
    assertEqX4(Uint32x4.fromInt8x16Bits(i), expected(i, ArrayBuffer));
    if (typeof SharedArrayBuffer != "undefined")
      assertEqX4(Uint32x4.fromInt8x16Bits(i), expected(i, SharedArrayBuffer));
  }
}

function testUint32x4FromUint8x16Bits() {
  function expected(v, Buffer) {
    var u8 = new Uint8Array(new Buffer(16));
    var u32 = new Uint32Array(u8.buffer);
    var asArr = simdToArray(v);
    for (var i = 0; i < 16; i++) u8[i] = asArr[i];
    return [u32[0], u32[1], u32[2], u32[3]];
  }

  var vals = [[1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16],
              [0, UINT8_MAX, 0, UINT8_MAX, 0, UINT8_MAX, 0, UINT8_MAX,
               UINT8_MAX, 0, UINT8_MAX, 0, UINT8_MAX, 0, UINT8_MAX, 0]];

  for (var v of vals) {
    var i = Uint8x16(...v);
    assertEqX4(Uint32x4.fromUint8x16Bits(i), expected(i, ArrayBuffer));
    if (typeof SharedArrayBuffer != "undefined")
      assertEqX4(Uint32x4.fromUint8x16Bits(i), expected(i, SharedArrayBuffer));
  }
}

function testUint32x4FromInt16x8Bits() {
  function expected(v, Buffer) {
    var i16 = new Int16Array(new Buffer(16));
    var u32 = new Uint32Array(i16.buffer);
    var asArr = simdToArray(v);
    for (var i = 0; i < 8; i++) i16[i] = asArr[i];
    return [u32[0], u32[1], u32[2], u32[3]];
  }

  var vals = [[1, 2, 3, 4, 5, 6, 7, 8],
              [INT16_MIN, INT16_MAX, INT16_MIN, INT16_MAX, INT16_MIN, INT16_MAX, INT16_MIN, INT16_MAX]];

  for (var v of vals) {
    var i = Int16x8(...v);
    assertEqX4(Uint32x4.fromInt16x8Bits(i), expected(i, ArrayBuffer));
    if (typeof SharedArrayBuffer != "undefined")
      assertEqX4(Uint32x4.fromInt16x8Bits(i), expected(i, SharedArrayBuffer));
  }
}

function testUint32x4FromUint16x8Bits() {
  function expected(v, Buffer) {
    var u16 = new Uint16Array(new Buffer(16));
    var u32 = new Uint32Array(u16.buffer);
    var asArr = simdToArray(v);
    for (var i = 0; i < 8; i++) u16[i] = asArr[i];
    return [u32[0], u32[1], u32[2], u32[3]];
  }

  var vals = [[1, 2, 3, 4, 5, 6, 7, 8],
              [0, UINT16_MAX, 0, UINT16_MAX, 0, UINT16_MAX, 0, UINT16_MAX]];

  for (var v of vals) {
    var i = Uint16x8(...v);
    assertEqX4(Uint32x4.fromUint16x8Bits(i), expected(i, ArrayBuffer));
    if (typeof SharedArrayBuffer != "undefined")
      assertEqX4(Uint32x4.fromUint16x8Bits(i), expected(i, SharedArrayBuffer));
  }
}

function testUint32x4FromInt32x4Bits() {
  function expected(v, Buffer) {
    var i32 = new Int32Array(new Buffer(16));
    var u32 = new Uint32Array(i32.buffer);
    var asArr = simdToArray(v);
    for (var i = 0; i < 8; i++) i32[i] = asArr[i];
    return [u32[0], u32[1], u32[2], u32[3]];
  }

  var vals = [[0, 1, -2, 3], [INT8_MIN, UINT32_MAX, INT32_MIN, INT32_MAX]];

  for (var v of vals) {
    var i = Int32x4(...v);
    assertEqX4(Uint32x4.fromInt32x4Bits(i), expected(i, ArrayBuffer));
    if (typeof SharedArrayBuffer != "undefined")
      assertEqX4(Uint32x4.fromInt32x4Bits(i), expected(i, SharedArrayBuffer));
  }
}

function testUint8x16FromFloat32x4Bits() {
  function expected(v, Buffer) {
    var f32 = new Float32Array(new Buffer(16));
    var u8 = new Uint8Array(f32.buffer);
    var asArr = simdToArray(v);
    for (var i = 0; i < 4; i++) f32[i] = asArr[i];
    return [u8[0], u8[1], u8[2], u8[3], u8[4], u8[5], u8[6], u8[7],
            u8[8], u8[9], u8[10], u8[11], u8[12], u8[13], u8[14], u8[15]];
  }

  var vals = [[1, -2, 3, -4], [Infinity, -Infinity, NaN, -0]];

  for (var v of vals) {
    var f = Float32x4(...v);
    assertEqX16(Uint8x16.fromFloat32x4Bits(f), expected(f, ArrayBuffer));
    if (typeof SharedArrayBuffer != "undefined")
      assertEqX16(Uint8x16.fromFloat32x4Bits(f), expected(f, SharedArrayBuffer));
  }
}

function testUint8x16FromFloat64x2Bits() {
  function expected(v, Buffer) {
    var f64 = new Float64Array(new Buffer(16));
    var u8 = new Uint8Array(f64.buffer);
    f64[0] = Float64x2.extractLane(v, 0);
    f64[1] = Float64x2.extractLane(v, 1);
    return [u8[0], u8[1], u8[2], u8[3], u8[4], u8[5], u8[6], u8[7],
            u8[8], u8[9], u8[10], u8[11], u8[12], u8[13], u8[14], u8[15]];
  }
  var vals = [[1, -2], [-3, 4], [Infinity, -Infinity], [NaN, -0]];

  for (var v of vals) {
    var f = Float64x2(...v);
    assertEqX16(Uint8x16.fromFloat64x2Bits(f), expected(f, ArrayBuffer));
    if (typeof SharedArrayBuffer != "undefined")
      assertEqX16(Uint8x16.fromFloat64x2Bits(f), expected(f, SharedArrayBuffer));
  }
}

function testUint8x16FromInt8x16Bits() {
  function expected(v, Buffer) {
    var i8 = new Int8Array(new Buffer(16));
    var u8 = new Uint8Array(i8.buffer);
    var asArr = simdToArray(v);
    for (var i = 0; i < 16; i++) i8[i] = asArr[i];
    return [u8[0], u8[1], u8[2], u8[3], u8[4], u8[5], u8[6], u8[7],
            u8[8], u8[9], u8[10], u8[11], u8[12], u8[13], u8[14], u8[15]];
  }

  var vals = [[0, 1, -2, 3, -4, 5, INT8_MIN, UINT8_MAX, -6, 7, INT8_MAX, 9, -10, 11, -12, 13]];

  for (var v of vals) {
    var i = Int8x16(...v);
    assertEqX16(Uint8x16.fromInt8x16Bits(i), expected(i, ArrayBuffer));
    if (typeof SharedArrayBuffer != "undefined")
      assertEqX16(Uint8x16.fromInt8x16Bits(i), expected(i, SharedArrayBuffer));
  }
}

function testUint8x16FromInt16x8Bits() {
  function expected(v, Buffer) {
    var i16 = new Int16Array(new Buffer(16));
    var u8 = new Uint8Array(i16.buffer);
    var asArr = simdToArray(v);
    for (var i = 0; i < 8; i++) i16[i] = asArr[i];
    return [u8[0], u8[1], u8[2], u8[3], u8[4], u8[5], u8[6], u8[7],
            u8[8], u8[9], u8[10], u8[11], u8[12], u8[13], u8[14], u8[15]];
  }

  var vals = [[0, 1, -2, 3, INT8_MIN, INT8_MAX, INT16_MIN, INT16_MAX]];
  for (var v of vals) {
    var i = Int16x8(...v);
    assertEqX16(Uint8x16.fromInt16x8Bits(i), expected(i, ArrayBuffer));
    if (typeof SharedArrayBuffer != "undefined")
      assertEqX16(Uint8x16.fromInt16x8Bits(i), expected(i, SharedArrayBuffer));
  }
}

function testUint8x16FromUint16x8Bits() {
  function expected(v, Buffer) {
    var u16 = new Uint16Array(new Buffer(16));
    var u8 = new Uint8Array(u16.buffer);
    var asArr = simdToArray(v);
    for (var i = 0; i < 8; i++) u16[i] = asArr[i];
    return [u8[0], u8[1], u8[2], u8[3], u8[4], u8[5], u8[6], u8[7],
            u8[8], u8[9], u8[10], u8[11], u8[12], u8[13], u8[14], u8[15]];
  }

  var vals = [[0, 1, -2, UINT16_MAX, INT8_MIN, INT8_MAX, INT16_MIN, INT16_MAX]];
  for (var v of vals) {
    var i = Uint16x8(...v);
    assertEqX16(Uint8x16.fromUint16x8Bits(i), expected(i, ArrayBuffer));
    if (typeof SharedArrayBuffer != "undefined")
      assertEqX16(Uint8x16.fromUint16x8Bits(i), expected(i, SharedArrayBuffer));
  }
}

function testUint8x16FromInt32x4Bits() {
  function expected(v, Buffer) {
    var i32 = new Int32Array(new Buffer(16));
    var u8 = new Uint8Array(i32.buffer);
    var asArr = simdToArray(v);
    for (var i = 0; i < 4; i++) i32[i] = asArr[i];
    return [u8[0], u8[1], u8[2], u8[3], u8[4], u8[5], u8[6], u8[7],
            u8[8], u8[9], u8[10], u8[11], u8[12], u8[13], u8[14], u8[15]];
  }

  var vals = [[0, 1, -2, 3], [INT8_MIN, INT8_MAX, INT32_MIN, INT32_MAX]];
  for (var v of vals) {
    var i = Int32x4(...v);
    assertEqX16(Uint8x16.fromInt32x4Bits(i), expected(i, ArrayBuffer));
    if (typeof SharedArrayBuffer != "undefined")
      assertEqX16(Uint8x16.fromInt32x4Bits(i), expected(i, SharedArrayBuffer));
  }
}

function testUint8x16FromUint32x4Bits() {
  function expected(v, Buffer) {
    var u32 = new Uint32Array(new Buffer(16));
    var u8 = new Uint8Array(u32.buffer);
    var asArr = simdToArray(v);
    for (var i = 0; i < 4; i++) u32[i] = asArr[i];
    return [u8[0], u8[1], u8[2], u8[3], u8[4], u8[5], u8[6], u8[7],
            u8[8], u8[9], u8[10], u8[11], u8[12], u8[13], u8[14], u8[15]];
  }

  var vals = [[0, 1, -2, 3], [INT8_MIN, INT8_MAX, INT32_MIN, INT32_MAX]];
  for (var v of vals) {
    var i = Uint32x4(...v);
    assertEqX16(Uint8x16.fromUint32x4Bits(i), expected(i, ArrayBuffer));
    if (typeof SharedArrayBuffer != "undefined")
      assertEqX16(Uint8x16.fromUint32x4Bits(i), expected(i, SharedArrayBuffer));
  }
}

function testUint16x8FromFloat32x4Bits() {
  function expected(v, Buffer) {
    var f32 = new Float32Array(new Buffer(16));
    var u16 = new Uint16Array(f32.buffer);
    var asArr = simdToArray(v);
    for (var i = 0; i < 4; i++) f32[i] = asArr[i];
    return [u16[0], u16[1], u16[2], u16[3], u16[4], u16[5], u16[6], u16[7]];
  }

  var vals = [[1, -2, 3, -4], [Infinity, -Infinity, NaN, -0]];

  for (var v of vals) {
    var f = Float32x4(...v);
    assertEqX8(Uint16x8.fromFloat32x4Bits(f), expected(f, ArrayBuffer));
    if (typeof SharedArrayBuffer != "undefined")
      assertEqX8(Uint16x8.fromFloat32x4Bits(f), expected(f, SharedArrayBuffer));
  }
}

function testUint16x8FromFloat64x2Bits() {
  function expected(v, Buffer) {
    var f64 = new Float64Array(new Buffer(16));
    var u16 = new Uint16Array(f64.buffer);
    f64[0] = Float64x2.extractLane(v, 0);
    f64[1] = Float64x2.extractLane(v, 1);
    return [u16[0], u16[1], u16[2], u16[3], u16[4], u16[5], u16[6], u16[7]];
  }

  var vals = [[1, -2], [-3, 4], [Infinity, -Infinity], [NaN, -0]];

  for (var v of vals) {
    var f = Float64x2(...v);
    assertEqX8(Uint16x8.fromFloat64x2Bits(f), expected(f, ArrayBuffer));
    if (typeof SharedArrayBuffer != "undefined")
      assertEqX8(Uint16x8.fromFloat64x2Bits(f), expected(f, SharedArrayBuffer));
  }
}

function testUint16x8FromInt8x16Bits() {
  function expected(v, Buffer) {
    var i8 = new Int8Array(new Buffer(16));
    var u16 = new Uint16Array(i8.buffer);
    var asArr = simdToArray(v);
    for (var i = 0; i < 16; i++) i8[i] = asArr[i];
    return [u16[0], u16[1], u16[2], u16[3], u16[4], u16[5], u16[6], u16[7]];
  }

  var vals = [[0, 1, -2, 3, -4, 5, INT8_MIN, INT8_MAX, -6, 7, -8, 9, -10, 11, -12, 13]];

  for (var v of vals) {
    var i = Int8x16(...v);
    assertEqX8(Uint16x8.fromInt8x16Bits(i), expected(i, ArrayBuffer));
    if (typeof SharedArrayBuffer != "undefined")
      assertEqX8(Uint16x8.fromInt8x16Bits(i), expected(i, SharedArrayBuffer));
  }
}

function testUint16x8FromUint8x16Bits() {
  function expected(v, Buffer) {
    var u8 = new Uint8Array(new Buffer(16));
    var u16 = new Uint16Array(u8.buffer);
    var asArr = simdToArray(v);
    for (var i = 0; i < 16; i++) u8[i] = asArr[i];
    return [u16[0], u16[1], u16[2], u16[3], u16[4], u16[5], u16[6], u16[7]];
  }

  var vals = [[0, 1, -2, 3, -4, UINT8_MAX, INT8_MIN, INT8_MAX, -6, 7, -8, 9, -10, 11, -12, 13]];

  for (var v of vals) {
    var i = Uint8x16(...v);
    assertEqX8(Uint16x8.fromUint8x16Bits(i), expected(i, ArrayBuffer));
    if (typeof SharedArrayBuffer != "undefined")
      assertEqX8(Uint16x8.fromUint8x16Bits(i), expected(i, SharedArrayBuffer));
  }
}

function testUint16x8FromInt16x8Bits() {
  function expected(v, Buffer) {
    var i16 = new Int16Array(new Buffer(16));
    var u16 = new Uint16Array(i16.buffer);
    var asArr = simdToArray(v);
    for (var i = 0; i < 8; i++) i16[i] = asArr[i];
    return [u16[0], u16[1], u16[2], u16[3], u16[4], u16[5], u16[6], u16[7]];
  }

  var vals = [[1, 2, 3, 4, 5, 6, 7, 8],
              [INT16_MIN, UINT16_MAX, INT16_MAX, UINT16_MAX, 0, UINT16_MAX, 0, UINT16_MAX]];

  for (var v of vals) {
    var i = Int16x8(...v);
    assertEqX8(Uint16x8.fromInt16x8Bits(i), expected(i, ArrayBuffer));
    if (typeof SharedArrayBuffer != "undefined")
      assertEqX8(Uint16x8.fromInt16x8Bits(i), expected(i, SharedArrayBuffer));
  }
}

function testUint16x8FromInt32x4Bits() {
  function expected(v, Buffer) {
    var i32 = new Int32Array(new Buffer(16));
    var u16 = new Uint16Array(i32.buffer);
    var asArr = simdToArray(v);
    for (var i = 0; i < 4; i++) i32[i] = asArr[i];
    return [u16[0], u16[1], u16[2], u16[3], u16[4], u16[5], u16[6], u16[7]];
  }

  var vals = [[1, -2, -3, 4], [INT16_MAX, INT16_MIN, INT32_MAX, INT32_MIN]];

  for (var v of vals) {
    var i = Int32x4(...v);
    assertEqX8(Uint16x8.fromInt32x4Bits(i), expected(i, ArrayBuffer));
    if (typeof SharedArrayBuffer != "undefined")
      assertEqX8(Uint16x8.fromInt32x4Bits(i), expected(i, SharedArrayBuffer));
  }
}

function testUint16x8FromUint32x4Bits() {
  function expected(v, Buffer) {
    var u32 = new Uint32Array(new Buffer(16));
    var u16 = new Uint16Array(u32.buffer);
    var asArr = simdToArray(v);
    for (var i = 0; i < 4; i++) u32[i] = asArr[i];
    return [u16[0], u16[1], u16[2], u16[3], u16[4], u16[5], u16[6], u16[7]];
  }

  var vals = [[1, -2, -3, 4], [INT16_MAX, INT16_MIN, INT32_MAX, INT32_MIN]];

  for (var v of vals) {
    var i = Uint32x4(...v);
    assertEqX8(Uint16x8.fromUint32x4Bits(i), expected(i, ArrayBuffer));
    if (typeof SharedArrayBuffer != "undefined")
      assertEqX8(Uint16x8.fromUint32x4Bits(i), expected(i, SharedArrayBuffer));
  }
}

function test() {
  testFloat32x4FromFloat64x2Bits();
  testFloat32x4FromInt8x16Bits();
  testFloat32x4FromInt16x8Bits();
  testFloat32x4FromInt32x4();
  testFloat32x4FromInt32x4Bits();
  testFloat32x4FromUint8x16Bits();
  testFloat32x4FromUint16x8Bits();
  testFloat32x4FromUint32x4();
  testFloat32x4FromUint32x4Bits();

  testFloat64x2FromFloat32x4Bits();
  testFloat64x2FromInt8x16Bits();
  testFloat64x2FromInt16x8Bits();
  testFloat64x2FromInt32x4Bits();
  testFloat64x2FromUint8x16Bits();
  testFloat64x2FromUint16x8Bits();
  testFloat64x2FromUint32x4Bits();

  testInt8x16FromFloat32x4Bits();
  testInt8x16FromFloat64x2Bits();
  testInt8x16FromInt16x8Bits();
  testInt8x16FromInt32x4Bits();
  testInt8x16FromUint8x16Bits();
  testInt8x16FromUint16x8Bits();
  testInt8x16FromUint32x4Bits();

  testInt16x8FromFloat32x4Bits();
  testInt16x8FromFloat64x2Bits();
  testInt16x8FromInt8x16Bits();
  testInt16x8FromInt32x4Bits();
  testInt16x8FromUint8x16Bits();
  testInt16x8FromUint16x8Bits();
  testInt16x8FromUint32x4Bits();

  testInt32x4FromFloat32x4();
  testInt32x4FromFloat32x4Bits();
  testInt32x4FromFloat64x2Bits();
  testInt32x4FromInt8x16Bits();
  testInt32x4FromInt16x8Bits();
  testInt32x4FromUint8x16Bits();
  testInt32x4FromUint16x8Bits();
  testInt32x4FromUint32x4Bits();

  testUint8x16FromFloat32x4Bits();
  testUint8x16FromFloat64x2Bits();
  testUint8x16FromInt8x16Bits();
  testUint8x16FromInt16x8Bits();
  testUint8x16FromInt32x4Bits();
  testUint8x16FromUint16x8Bits();
  testUint8x16FromUint32x4Bits();

  testUint16x8FromFloat32x4Bits();
  testUint16x8FromFloat64x2Bits();
  testUint16x8FromInt8x16Bits();
  testUint16x8FromInt16x8Bits();
  testUint16x8FromInt32x4Bits();
  testUint16x8FromUint8x16Bits();
  testUint16x8FromUint32x4Bits();

  testUint32x4FromFloat32x4();
  testUint32x4FromFloat32x4Bits();
  testUint32x4FromFloat64x2Bits();
  testUint32x4FromInt8x16Bits();
  testUint32x4FromInt16x8Bits();
  testUint32x4FromInt32x4Bits();
  testUint32x4FromUint8x16Bits();
  testUint32x4FromUint16x8Bits();

  if (typeof reportCompare === "function") {
    reportCompare(true, true);
  }
}

test();
