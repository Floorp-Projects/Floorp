// |reftest| skip-if(!this.hasOwnProperty("SIMD"))
var Float32x4 = SIMD.Float32x4;
var Int8x16 = SIMD.Int8x16;
var Int16x8 = SIMD.Int16x8;
var Int32x4 = SIMD.Int32x4;
var Uint8x16 = SIMD.Uint8x16;
var Uint16x8 = SIMD.Uint16x8;
var Uint32x4 = SIMD.Uint32x4;
var Bool8x16 = SIMD.Bool8x16;
var Bool16x8 = SIMD.Bool16x8;
var Bool32x4 = SIMD.Bool32x4;
var Bool64x2 = SIMD.Bool64x2;

// Float32x4.
function testFloat32x4add() {
  function addf(a, b) {
    return Math.fround(Math.fround(a) + Math.fround(b));
  }

  var vals = [
    [[1, 2, 3, 4], [10, 20, 30, 40]],
    [[1.57, 2.27, 3.57, 4.19], [10.31, 20.49, 30.41, 40.72]],
    [[NaN, -0, Infinity, -Infinity], [0, -0, -Infinity, -Infinity]]
  ];

  for (var [v,w] of vals) {
    testBinaryFunc(Float32x4(...v), Float32x4(...w), Float32x4.add, addf);
  }
}

function testFloat32x4div() {
  function divf(a, b) {
    return Math.fround(Math.fround(a) / Math.fround(b));
  }

  var vals = [
    [[1, 2, 3, 4], [10, 20, 30, 40]],
    [[1.26, 2.03, 3.17, 4.59], [11.025, 17.3768, 29.1957, 46.4049]],
    [[0, -0, Infinity, -Infinity], [1, 1, -Infinity, Infinity]]
  ];

  for (var [v,w] of vals) {
    testBinaryFunc(Float32x4(...v), Float32x4(...w), Float32x4.div, divf);
  }
}

function testFloat32x4mul() {
  function mulf(a, b) {
    return Math.fround(Math.fround(a) * Math.fround(b));
  }

  var vals = [
    [[1, 2, 3, 4], [10, 20, 30, 40]],
    [[1.66, 2.57, 3.73, 4.12], [10.67, 20.68, 30.02, 40.58]],
    [[NaN, -0, Infinity, -Infinity], [NaN, -0, -Infinity, 0]]
  ];

  for (var [v,w] of vals) {
    testBinaryFunc(Float32x4(...v), Float32x4(...w), Float32x4.mul, mulf);
  }
}

function testFloat32x4sub() {
  function subf(a, b) {
    return Math.fround(Math.fround(a) - Math.fround(b));
  }

  var vals = [
    [[1, 2, 3, 4], [10, 20, 30, 40]],
    [[1.34, 2.95, 3.17, 4.29], [10.18, 20.43, 30.63, 40.38]],
    [[NaN, -0, -Infinity, -Infinity], [NaN, -0, Infinity, -Infinity]]
  ];

  for (var [v,w] of vals) {
    testBinaryFunc(Float32x4(...v), Float32x4(...w), Float32x4.sub, subf);
  }
}

// Helper for saturating arithmetic.
// See SIMD.js, 5.1.25 Saturate(descriptor, x)
function saturate(lower, upper, x) {
    x = x | 0;
    if (x > upper)
        return upper;
    if (x < lower)
        return lower;
    return x;
}

var i8x16vals = [
  [[1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16],
   [10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 110, 120, 130, 140, 150, 160]],
  [[INT8_MAX, INT8_MIN, INT8_MAX, INT8_MIN, INT8_MAX, INT8_MIN, INT8_MAX, INT8_MIN, -2, -3, -4, -5, -6, -7, -8, -9],
   [1, 1, -1, -1, INT8_MAX, INT8_MAX, INT8_MIN, INT8_MIN, 8, 9, 10, 11, 12, 13, 14, 15]]
];

// Int8x16.
function testInt8x16add() {
  function addi(a, b) {
    return (a + b) << 24 >> 24;
  }

  for (var [v,w] of i8x16vals) {
    testBinaryFunc(Int8x16(...v), Int8x16(...w), Int8x16.add, addi);
  }
}

function testInt8x16and() {
  function andi(a, b) {
    return (a & b) << 24 >> 24;
  }

  for (var [v,w] of i8x16vals) {
    testBinaryFunc(Int8x16(...v), Int8x16(...w), Int8x16.and, andi);
  }
}

function testInt8x16mul() {
  function muli(x, y) {
    return (x * y) << 24 >> 24;
  }

  for (var [v,w] of i8x16vals) {
    testBinaryFunc(Int8x16(...v), Int8x16(...w), Int8x16.mul, muli);
  }
}

function testInt8x16or() {
  function ori(a, b) {
    return (a | b) << 24 >> 24;
  }

  for (var [v,w] of i8x16vals) {
    testBinaryFunc(Int8x16(...v), Int8x16(...w), Int8x16.or, ori);
  }
}

function testInt8x16sub() {
  function subi(a, b) {
    return (a - b) << 24 >> 24;
  }

  for (var [v,w] of i8x16vals) {
    testBinaryFunc(Int8x16(...v), Int8x16(...w), Int8x16.sub, subi);
  }
}

function testInt8x16xor() {
  function xori(a, b) {
    return (a ^ b) << 24 >> 24;
  }

  for (var [v,w] of i8x16vals) {
    testBinaryFunc(Int8x16(...v), Int8x16(...w), Int8x16.xor, xori);
  }
}

function testInt8x16addSaturate() {
  function satadd(a, b) {
    return saturate(INT8_MIN, INT8_MAX, a + b);
  }

  for (var [v,w] of i8x16vals) {
    testBinaryFunc(Int8x16(...v), Int8x16(...w), Int8x16.addSaturate, satadd);
  }
}

function testInt8x16subSaturate() {
  function satsub(a, b) {
    return saturate(INT8_MIN, INT8_MAX, a - b);
  }

  for (var [v,w] of i8x16vals) {
    testBinaryFunc(Int8x16(...v), Int8x16(...w), Int8x16.subSaturate, satsub);
  }
}

// Uint8x16.
function testUint8x16add() {
  function addi(a, b) {
    return (a + b) << 24 >>> 24;
  }

  for (var [v,w] of i8x16vals) {
    testBinaryFunc(Uint8x16(...v), Uint8x16(...w), Uint8x16.add, addi);
  }
}

function testUint8x16and() {
  function andi(a, b) {
    return (a & b) << 24 >>> 24;
  }

  for (var [v,w] of i8x16vals) {
    testBinaryFunc(Uint8x16(...v), Uint8x16(...w), Uint8x16.and, andi);
  }
}

function testUint8x16mul() {
  function muli(x, y) {
    return (x * y) << 24 >>> 24;
  }

  for (var [v,w] of i8x16vals) {
    testBinaryFunc(Uint8x16(...v), Uint8x16(...w), Uint8x16.mul, muli);
  }
}

function testUint8x16or() {
  function ori(a, b) {
    return (a | b) << 24 >>> 24;
  }

  for (var [v,w] of i8x16vals) {
    testBinaryFunc(Uint8x16(...v), Uint8x16(...w), Uint8x16.or, ori);
  }
}

function testUint8x16sub() {
  function subi(a, b) {
    return (a - b) << 24 >>> 24;
  }

  for (var [v,w] of i8x16vals) {
    testBinaryFunc(Uint8x16(...v), Uint8x16(...w), Uint8x16.sub, subi);
  }
}

function testUint8x16xor() {
  function xori(a, b) {
    return (a ^ b) << 24 >>> 24;
  }

  for (var [v,w] of i8x16vals) {
    testBinaryFunc(Uint8x16(...v), Uint8x16(...w), Uint8x16.xor, xori);
  }
}

function testUint8x16addSaturate() {
  function satadd(a, b) {
    return saturate(0, UINT8_MAX, a + b);
  }

  for (var [v,w] of i8x16vals) {
    testBinaryFunc(Uint8x16(...v), Uint8x16(...w), Uint8x16.addSaturate, satadd);
  }
}

function testUint8x16subSaturate() {
  function satsub(a, b) {
    return saturate(0, UINT8_MAX, a - b);
  }

  for (var [v,w] of i8x16vals) {
    testBinaryFunc(Uint8x16(...v), Uint8x16(...w), Uint8x16.subSaturate, satsub);
  }
}

var i16x8vals = [
  [[1, 2, 3, 4, 5, 6, 7, 8],
   [10, 20, 30, 40, 50, 60, 70, 80]],
  [[INT16_MAX, INT16_MIN, INT16_MAX, INT16_MIN, INT16_MAX, INT16_MIN, INT16_MAX, INT16_MIN],
   [1, 1, -1, -1, INT16_MAX, INT16_MAX, INT16_MIN, INT16_MIN]]
];

// Int16x8.
function testInt16x8add() {
  function addi(a, b) {
    return (a + b) << 16 >> 16;
  }

  for (var [v,w] of i16x8vals) {
    testBinaryFunc(Int16x8(...v), Int16x8(...w), Int16x8.add, addi);
  }
}

function testInt16x8and() {
  function andi(a, b) {
    return (a & b) << 16 >> 16;
  }

  for (var [v,w] of i16x8vals) {
    testBinaryFunc(Int16x8(...v), Int16x8(...w), Int16x8.and, andi);
  }
}

function testInt16x8mul() {
  function muli(x, y) {
    return (x * y) << 16 >> 16;
  }

  for (var [v,w] of i16x8vals) {
    testBinaryFunc(Int16x8(...v), Int16x8(...w), Int16x8.mul, muli);
  }
}

function testInt16x8or() {
  function ori(a, b) {
    return (a | b) << 16 >> 16;
  }

  for (var [v,w] of i16x8vals) {
    testBinaryFunc(Int16x8(...v), Int16x8(...w), Int16x8.or, ori);
  }
}

function testInt16x8sub() {
  function subi(a, b) {
    return (a - b) << 16 >> 16;
  }

  for (var [v,w] of i16x8vals) {
    testBinaryFunc(Int16x8(...v), Int16x8(...w), Int16x8.sub, subi);
  }
}

function testInt16x8xor() {
  function xori(a, b) {
    return (a ^ b) << 16 >> 16;
  }

  for (var [v,w] of i16x8vals) {
    testBinaryFunc(Int16x8(...v), Int16x8(...w), Int16x8.xor, xori);
  }
}

function testInt16x8addSaturate() {
  function satadd(a, b) {
    return saturate(INT16_MIN, INT16_MAX, a + b);
  }

  for (var [v,w] of i16x8vals) {
    testBinaryFunc(Int16x8(...v), Int16x8(...w), Int16x8.addSaturate, satadd);
  }
}

function testInt16x8subSaturate() {
  function satsub(a, b) {
    return saturate(INT16_MIN, INT16_MAX, a - b);
  }

  for (var [v,w] of i16x8vals) {
    testBinaryFunc(Int16x8(...v), Int16x8(...w), Int16x8.subSaturate, satsub);
  }
}

// Uint16x8.
function testUint16x8add() {
  function addi(a, b) {
    return (a + b) << 16 >>> 16;
  }

  for (var [v,w] of i16x8vals) {
    testBinaryFunc(Uint16x8(...v), Uint16x8(...w), Uint16x8.add, addi);
  }
}

function testUint16x8and() {
  function andi(a, b) {
    return (a & b) << 16 >>> 16;
  }

  for (var [v,w] of i16x8vals) {
    testBinaryFunc(Uint16x8(...v), Uint16x8(...w), Uint16x8.and, andi);
  }
}

function testUint16x8mul() {
  function muli(x, y) {
    return (x * y) << 16 >>> 16;
  }

  for (var [v,w] of i16x8vals) {
    testBinaryFunc(Uint16x8(...v), Uint16x8(...w), Uint16x8.mul, muli);
  }
}

function testUint16x8or() {
  function ori(a, b) {
    return (a | b) << 16 >>> 16;
  }

  for (var [v,w] of i16x8vals) {
    testBinaryFunc(Uint16x8(...v), Uint16x8(...w), Uint16x8.or, ori);
  }
}

function testUint16x8sub() {
  function subi(a, b) {
    return (a - b) << 16 >>> 16;
  }

  for (var [v,w] of i16x8vals) {
    testBinaryFunc(Uint16x8(...v), Uint16x8(...w), Uint16x8.sub, subi);
  }
}

function testUint16x8xor() {
  function xori(a, b) {
    return (a ^ b) << 16 >>> 16;
  }

  for (var [v,w] of i16x8vals) {
    testBinaryFunc(Uint16x8(...v), Uint16x8(...w), Uint16x8.xor, xori);
  }
}

function testUint16x8addSaturate() {
  function satadd(a, b) {
    return saturate(0, UINT16_MAX, a + b);
  }

  for (var [v,w] of i16x8vals) {
    testBinaryFunc(Uint16x8(...v), Uint16x8(...w), Uint16x8.addSaturate, satadd);
  }
}

function testUint16x8subSaturate() {
  function satsub(a, b) {
    return saturate(0, UINT16_MAX, a - b);
  }

  for (var [v,w] of i16x8vals) {
    testBinaryFunc(Uint16x8(...v), Uint16x8(...w), Uint16x8.subSaturate, satsub);
  }
}

var i32x4vals = [
  [[1, 2, 3, 4], [10, 20, 30, 40]],
  [[INT32_MAX, INT32_MIN, INT32_MAX, INT32_MIN], [1, -1, 0, 0]],
  [[INT32_MAX, INT32_MIN, INT32_MAX, INT32_MIN], [INT32_MIN, INT32_MAX, INT32_MAX, INT32_MIN]],
  [[INT32_MAX, INT32_MIN, INT32_MAX, INT32_MIN], [-1, -1, INT32_MIN, INT32_MIN]],
  [[INT32_MAX, INT32_MIN, INT32_MAX, INT32_MIN], [-1, 1, INT32_MAX, INT32_MIN]],
  [[UINT32_MAX, 0, UINT32_MAX, 0], [1, -1, 0, 0]],
  [[UINT32_MAX, 0, UINT32_MAX, 0], [-1, -1, INT32_MIN, INT32_MIN]],
  [[UINT32_MAX, 0, UINT32_MAX, 0], [1, -1, 0, 0]],
  [[UINT32_MAX, 0, UINT32_MAX, 0], [-1, 1, INT32_MAX, INT32_MIN]]
];

// Int32x4.
function testInt32x4add() {
  function addi(a, b) {
    return (a + b) | 0;
  }

  for (var [v,w] of i32x4vals) {
    testBinaryFunc(Int32x4(...v), Int32x4(...w), Int32x4.add, addi);
  }
}

function testInt32x4and() {
  function andi(a, b) {
    return (a & b) | 0;
  }

  for (var [v,w] of i32x4vals) {
    testBinaryFunc(Int32x4(...v), Int32x4(...w), Int32x4.and, andi);
  }
}

function testInt32x4mul() {
  function muli(x, y) {
    // Deal with lost precision in the 53-bit double mantissa.
    // Compute two 48-bit products. Truncate and combine them.
    var hi = (x * (y >>> 16)) | 0;
    var lo = (x * (y & 0xffff)) | 0;
    return (lo + (hi << 16)) | 0;
  }

  for (var [v,w] of i32x4vals) {
    testBinaryFunc(Int32x4(...v), Int32x4(...w), Int32x4.mul, muli);
  }
}

function testInt32x4or() {
  function ori(a, b) {
    return (a | b) | 0;
  }

  for (var [v,w] of i32x4vals) {
    testBinaryFunc(Int32x4(...v), Int32x4(...w), Int32x4.or, ori);
  }
}

function testInt32x4sub() {
  function subi(a, b) {
    return (a - b) | 0;
  }

  for (var [v,w] of i32x4vals) {
    testBinaryFunc(Int32x4(...v), Int32x4(...w), Int32x4.sub, subi);
  }
}

function testInt32x4xor() {
  function xori(a, b) {
    return (a ^ b) | 0;
  }

  for (var [v,w] of i32x4vals) {
    testBinaryFunc(Int32x4(...v), Int32x4(...w), Int32x4.xor, xori);
  }
}

// Uint32x4.
function testUint32x4add() {
  function addi(a, b) {
    return (a + b) >>> 0;
  }

  for (var [v,w] of i32x4vals) {
    testBinaryFunc(Uint32x4(...v), Uint32x4(...w), Uint32x4.add, addi);
  }
}

function testUint32x4and() {
  function andi(a, b) {
    return (a & b) >>> 0;
  }

  for (var [v,w] of i32x4vals) {
    testBinaryFunc(Uint32x4(...v), Uint32x4(...w), Uint32x4.and, andi);
  }
}

function testUint32x4mul() {
  function muli(x, y) {
    // Deal with lost precision in the 53-bit double mantissa.
    // Compute two 48-bit products. Truncate and combine them.
    var hi = (x * (y >>> 16)) >>> 0;
    var lo = (x * (y & 0xffff)) >>> 0;
    return (lo + (hi << 16)) >>> 0;
  }

  for (var [v,w] of i32x4vals) {
    testBinaryFunc(Uint32x4(...v), Uint32x4(...w), Uint32x4.mul, muli);
  }
}

function testUint32x4or() {
  function ori(a, b) {
    return (a | b) >>> 0;
  }

  for (var [v,w] of i32x4vals) {
    testBinaryFunc(Uint32x4(...v), Uint32x4(...w), Uint32x4.or, ori);
  }
}

function testUint32x4sub() {
  function subi(a, b) {
    return (a - b) >>> 0;
  }

  for (var [v,w] of i32x4vals) {
    testBinaryFunc(Uint32x4(...v), Uint32x4(...w), Uint32x4.sub, subi);
  }
}

function testUint32x4xor() {
  function xori(a, b) {
    return (a ^ b) >>> 0;
  }

  for (var [v,w] of i32x4vals) {
    testBinaryFunc(Uint32x4(...v), Uint32x4(...w), Uint32x4.xor, xori);
  }
}

var b8x16vals = [
  [[true, true, true, true, false, false, false, false, true, true, true, true, false, false, false, false],
   [false, true, false, true, false, true, false, true, true, true, true, true, false, false, false, false]]
];

function testBool8x16and() {
  function andb(a, b) {
    return a && b;
  }

  for (var [v,w] of b8x16vals) {
    testBinaryFunc(Bool8x16(...v), Bool8x16(...w), Bool8x16.and, andb);
  }
}

function testBool8x16or() {
  function orb(a, b) {
    return a || b;
  }

  for (var [v,w] of b8x16vals) {
    testBinaryFunc(Bool8x16(...v), Bool8x16(...w), Bool8x16.or, orb);
  }
}

function testBool8x16xor() {
  function xorb(a, b) {
    return a != b;
  }

  for (var [v,w] of b8x16vals) {
    testBinaryFunc(Bool8x16(...v), Bool8x16(...w), Bool8x16.xor, xorb);
  }
}

var b16x8vals = [
  [[true, true, true, true, false, false, false, false],
   [false, true, false, true, false, true, false, true]]
];

function testBool16x8and() {
  function andb(a, b) {
    return a && b;
  }

  for (var [v,w] of b16x8vals) {
    testBinaryFunc(Bool16x8(...v), Bool16x8(...w), Bool16x8.and, andb);
  }
}

function testBool16x8or() {
  function orb(a, b) {
    return a || b;
  }

  for (var [v,w] of b16x8vals) {
    testBinaryFunc(Bool16x8(...v), Bool16x8(...w), Bool16x8.or, orb);
  }
}

function testBool16x8xor() {
  function xorb(a, b) {
    return a != b;
  }

  for (var [v,w] of b16x8vals) {
    testBinaryFunc(Bool16x8(...v), Bool16x8(...w), Bool16x8.xor, xorb);
  }
}

var b32x4vals = [
  [[true, true, false, false], [false, true, false, true]]
];

function testBool32x4and() {
  function andb(a, b) {
    return a && b;
  }

  for (var [v,w] of b32x4vals) {
    testBinaryFunc(Bool32x4(...v), Bool32x4(...w), Bool32x4.and, andb);
  }
}

function testBool32x4or() {
  function orb(a, b) {
    return a || b;
  }

  for (var [v,w] of b32x4vals) {
    testBinaryFunc(Bool32x4(...v), Bool32x4(...w), Bool32x4.or, orb);
  }
}

function testBool32x4xor() {
  function xorb(a, b) {
    return a != b;
  }

  for (var [v,w] of b32x4vals) {
    testBinaryFunc(Bool32x4(...v), Bool32x4(...w), Bool32x4.xor, xorb);
  }
}

var b64x2vals = [
  [[false, false], [false, true], [true, false], [true, true]]
];

function testBool64x2and() {
  function andb(a, b) {
    return a && b;
  }

  for (var [v,w] of b64x2vals) {
    testBinaryFunc(Bool64x2(...v), Bool64x2(...w), Bool64x2.and, andb);
  }
}

function testBool64x2or() {
  function orb(a, b) {
    return a || b;
  }

  for (var [v,w] of b64x2vals) {
    testBinaryFunc(Bool64x2(...v), Bool64x2(...w), Bool64x2.or, orb);
  }
}

function testBool64x2xor() {
  function xorb(a, b) {
    return a != b;
  }

  for (var [v,w] of b64x2vals) {
    testBinaryFunc(Bool64x2(...v), Bool64x2(...w), Bool64x2.xor, xorb);
  }
}

function test() {
  testFloat32x4add();
  testFloat32x4div();
  testFloat32x4mul();
  testFloat32x4sub();

  testInt8x16add();
  testInt8x16and();
  testInt8x16mul();
  testInt8x16or();
  testInt8x16sub();
  testInt8x16xor();
  testInt8x16addSaturate();
  testInt8x16subSaturate();

  testUint8x16add();
  testUint8x16and();
  testUint8x16mul();
  testUint8x16or();
  testUint8x16sub();
  testUint8x16xor();
  testUint8x16addSaturate();
  testUint8x16subSaturate();

  testInt16x8add();
  testInt16x8and();
  testInt16x8mul();
  testInt16x8or();
  testInt16x8sub();
  testInt16x8xor();
  testInt16x8addSaturate();
  testInt16x8subSaturate();

  testUint16x8add();
  testUint16x8and();
  testUint16x8mul();
  testUint16x8or();
  testUint16x8sub();
  testUint16x8xor();
  testUint16x8addSaturate();
  testUint16x8subSaturate();

  testInt32x4add();
  testInt32x4and();
  testInt32x4mul();
  testInt32x4or();
  testInt32x4sub();
  testInt32x4xor();

  testUint32x4add();
  testUint32x4and();
  testUint32x4mul();
  testUint32x4or();
  testUint32x4sub();
  testUint32x4xor();

  testBool8x16and();
  testBool8x16or();
  testBool8x16xor();

  testBool16x8and();
  testBool16x8or();
  testBool16x8xor();

  testBool32x4and();
  testBool32x4or();
  testBool32x4xor();

  testBool64x2and();
  testBool64x2or();
  testBool64x2xor();

  if (typeof reportCompare === "function") {
    reportCompare(true, true);
  }
}

test();
