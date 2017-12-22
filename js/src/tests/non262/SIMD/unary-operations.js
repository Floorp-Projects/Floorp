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

function testFloat32x4abs() {
  function absf(a) {
    return Math.abs(Math.fround(a));
  }

  var vals = [
    [-1, 2, -3, 4],
    [-1.63, 2.46, -3.17, 4.94],
    [NaN, -0, Infinity, -Infinity]
  ];
  for (var v of vals) {
    assertEqX4(Float32x4.abs(Float32x4(...v)), v.map(absf));
  }
}

function testFloat32x4neg() {
  function negf(a) {
    return -1 * Math.fround(a);
  }

  var vals = [
    [1, 2, 3, 4],
    [0.999, -0.001, 3.78, 4.05],
    [NaN, -0, Infinity, -Infinity]
  ];
  for (var v of vals) {
    assertEqX4(Float32x4.neg(Float32x4(...v)), v.map(negf));
  }
}

function testFloat32x4reciprocalApproximation() {
  function reciprocalf(a) {
    return Math.fround(1 / Math.fround(a));
  }

  var vals = [
    [[1, 0.5, 0.25, 0.125], [1, 2, 4, 8]],
    [[1.6, 0.8, 0.4, 0.2], [1.6, 0.8, 0.4, 0.2].map(reciprocalf)],
    [[NaN, -0, Infinity, -Infinity], [NaN, -Infinity, 0, -0]]
  ];

  for (var [v,w] of vals) {
    assertEqX4(Float32x4.reciprocalApproximation(Float32x4(...v)), w);
  }
}

function testFloat32x4reciprocalSqrtApproximation() {
  function reciprocalsqrtf(a) {
    assertEq(Math.fround(a), a);
    return Math.fround(1 / Math.fround(Math.sqrt(a)));
  }

  var vals = [
    [[1, 1, 0.25, 0.25], [1, 1, 2, 2]],
    [[25, 16, 6.25, 1.5625], [25, 16, 6.25, 1.5625].map(reciprocalsqrtf)],
    [[NaN, -0, Infinity, -Infinity], [NaN, -0, Infinity, -Infinity].map(reciprocalsqrtf)],
    [[Math.pow(2, 32), Math.pow(2, -32), +0, Math.pow(2, -148)],
     [Math.pow(2, -16), Math.pow(2, 16), Infinity, Math.pow(2, 74)]]
  ];

  for (var [v,w] of vals) {
    assertEqX4(Float32x4.reciprocalSqrtApproximation(Float32x4(...v)), w);
  }
}

function testFloat32x4sqrt() {
  function sqrtf(a) {
    return Math.fround(Math.sqrt(Math.fround(a)));
  }

  var vals = [
    [[1, 4, 9, 16], [1, 2, 3, 4]],
    [[2.7225, 7.3441, 9.4249, -1], [2.7225, 7.3441, 9.4249, -1].map(sqrtf)],
    [[NaN, -0, Infinity, -Infinity], [NaN, -0, Infinity, NaN]]
  ];

  for (var [v,w] of vals) {
    assertEqX4(Float32x4.sqrt(Float32x4(...v)), w);
  }
}

function testInt8x16neg() {
  var vals = [
    [[1, 2, 3, 4, 5, 6, 7, 8, -1, -2, -3, -4, -5, INT8_MAX, INT8_MIN, 0],
     [-1, -2, -3, -4, -5, -6, -7, -8, 1, 2, 3, 4, 5, -INT8_MAX, INT8_MIN, 0]]
  ];
  for (var [v,w] of vals) {
    assertEqX16(Int8x16.neg(Int8x16(...v)), w);
  }
}

function testInt8x16not() {
  var vals = [
    [[1, 2, 3, 4, 5, 6, 7, -1, -2, -3, -4, -5, -6, 0, INT8_MIN, INT8_MAX],
     [1, 2, 3, 4, 5, 6, 7, -1, -2, -3, -4, -5, -6, 0, INT8_MIN, INT8_MAX].map((x) => ~x << 24 >> 24)]
  ];
  for (var [v,w] of vals) {
    assertEqX16(Int8x16.not(Int8x16(...v)), w);
  }
}

function testInt16x8neg() {
  var vals = [
    [[1, 2, 3, -1, -2, 0, INT16_MIN, INT16_MAX],
     [-1, -2, -3, 1, 2, 0, INT16_MIN, -INT16_MAX]]
  ];
  for (var [v,w] of vals) {
    assertEqX8(Int16x8.neg(Int16x8(...v)), w);
  }
}

function testInt16x8not() {
  var vals = [
    [[1, 2, 3, -1, -2, 0, INT16_MIN, INT16_MAX],
     [1, 2, 3, -1, -2, 0, INT16_MIN, INT16_MAX].map((x) => ~x << 16 >> 16)]
  ];
  for (var [v,w] of vals) {
    assertEqX8(Int16x8.not(Int16x8(...v)), w);
  }
}

function testInt32x4neg() {
  var valsExp = [
    [[1, 2, 3, 4], [-1, -2, -3, -4]],
    [[INT32_MAX, INT32_MIN, -0, 0], [-INT32_MAX | 0, -INT32_MIN | 0, 0, 0]]
  ];
  for (var [v,w] of valsExp) {
    assertEqX4(Int32x4.neg(Int32x4(...v)), w);
  }
}

function testInt32x4not() {
  var valsExp = [
    [[1, 2, 3, 4], [-2, -3, -4, -5]],
    [[INT32_MAX, INT32_MIN, 0, 0], [~INT32_MAX | 0, ~INT32_MIN | 0, ~0 | 0,  ~0 | 0]]
  ];
  for (var [v,w] of valsExp) {
    assertEqX4(Int32x4.not(Int32x4(...v)), w);
  }
}

function testUint8x16neg() {
  var vals = [
    [[  1,   2,   3,   4,   5,   6,   7, 0, -1, -2, -3, -4, UINT8_MAX,   INT8_MAX, 0, 0],
     [255, 254, 253, 252, 251, 250, 249, 0, 1,   2,  3,  4,         1, INT8_MAX+2, 0, 0]]
  ];
  for (var [v,w] of vals) {
    assertEqX16(Uint8x16.neg(Uint8x16(...v)), w);
  }
}

function testUint8x16not() {
  var vals = [
    [[1, 2, 3, 4, 5, 6, 7, -1, -2, -3, -4, -5, -6, 0, INT8_MIN, INT8_MAX],
     [1, 2, 3, 4, 5, 6, 7, -1, -2, -3, -4, -5, -6, 0, INT8_MIN, INT8_MAX].map((x) => ~x << 24 >>> 24)]
  ];
  for (var [v,w] of vals) {
    assertEqX16(Uint8x16.not(Uint8x16(...v)), w);
  }
}

function testUint16x8neg() {
  var vals = [
    [[1, 2, UINT16_MAX, -1, -2, 0, INT16_MIN, INT16_MAX],
     [1, 2, UINT16_MAX, -1, -2, 0, INT16_MIN, INT16_MAX].map((x) => -x << 16 >>> 16)]
  ];
  for (var [v,w] of vals) {
    assertEqX8(Uint16x8.neg(Uint16x8(...v)), w);
  }
}

function testUint16x8not() {
  var vals = [
    [[1, 2, UINT16_MAX, -1, -2, 0, INT16_MIN, INT16_MAX],
     [1, 2, UINT16_MAX, -1, -2, 0, INT16_MIN, INT16_MAX].map((x) => ~x << 16 >>> 16)]
  ];
  for (var [v,w] of vals) {
    assertEqX8(Uint16x8.not(Uint16x8(...v)), w);
  }
}

function testUint32x4neg() {
  var valsExp = [
    [[1, 2, 3, 4], [-1 >>> 0, -2 >>> 0, -3 >>> 0, -4 >>> 0]],
    [[INT32_MAX, INT32_MIN, -0, 0], [-INT32_MAX >>> 0, -INT32_MIN >>> 0, 0, 0]]
  ];
  for (var [v,w] of valsExp) {
    assertEqX4(Uint32x4.neg(Uint32x4(...v)), w);
  }
}

function testUint32x4not() {
  var valsExp = [
    [[1, 2, 3, 4], [~1 >>> 0, ~2 >>> 0, ~3 >>> 0, ~4 >>> 0]],
    [[INT32_MAX, INT32_MIN, UINT32_MAX, 0], [~INT32_MAX >>> 0, ~INT32_MIN >>> 0, 0,  ~0 >>> 0]]
  ];
  for (var [v,w] of valsExp) {
    assertEqX4(Uint32x4.not(Uint32x4(...v)), w);
  }
}

function testBool8x16not() {
  var valsExp = [
    [[true, false, true, false, true, false, true, false, true, false, true, false, true, false, true, false],
     [false, true, false, true, false, true, false, true, false, true, false, true, false, true, false, true]],
    [[true, true, false, false, true, true, false, false, true, true, false, false, true, true, false, false],
    [false, false, true, true, false, false, true, true, false, false, true, true, false, false, true, true]]
  ];
  for (var [v,w] of valsExp) {
    assertEqX16(Bool8x16.not(Bool8x16(...v)), w);
  }
}

function testBool8x16allTrue() {
  var valsExp = [
    [[false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false], false],
    [[true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true], true],
    [[false, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true], false],
    [[true, false, true, true, true, true, true, true, true, true, true, true, true, true, true, true], false],
    [[true, false, true, true, true, true, true, true, true, true, true, true, true, true, false, true], false],
    [[true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, false], false],
  ];
  for (var [v,w] of valsExp) {
    assertEq(Bool8x16.allTrue(Bool8x16(...v)), w);
  }
}

function testBool8x16anyTrue() {
  var valsExp = [
    [[false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false], false],
    [[true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true], true],
    [[false, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true], true],
    [[true, false, true, true, true, true, true, true, true, true, true, true, true, true, true, true], true],
    [[true, false, true, true, true, true, true, true, true, true, true, true, true, true, false, true], true],
    [[true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, false], true],
  ];
  for (var [v,w] of valsExp) {
    assertEq(Bool8x16.anyTrue(Bool8x16(...v)), w);
  }
}

function testBool16x8not() {
  var valsExp = [
    [[true, false, true, false, true, false, true, false], [false, true, false, true, false, true, false, true]],
    [[true, true, false, false, true, true, false, false], [false, false, true, true, false, false, true, true]]
  ];
  for (var [v,w] of valsExp) {
    assertEqX8(Bool16x8.not(Bool16x8(...v)), w);
  }
}

function testBool16x8allTrue() {
  var valsExp = [
    [[false, false, false, false, false, false, false, false], false],
    [[true, true, true, true, true, true, true, true], true],
    [[false, true, true, true, true, true, true, true], false],
    [[true, false, true, true, true, true, true, true], false],
    [[true, true, true, true, true, true, false, true], false],
    [[true, true, true, true, true, true, true, false], false],
  ];
  for (var [v,w] of valsExp) {
    assertEq(Bool16x8.allTrue(Bool16x8(...v)), w);
  }
}

function testBool16x8anyTrue() {
  var valsExp = [
    [[false, false, false, false, false, false, false, false], false],
    [[true, true, true, true, true, true, true, true], true],
    [[false, false, false, false, false, false, false, true], true],
    [[false, false, false, false, false, false, true, false], true],
    [[false, true, false, false, false, false, false, true], true],
    [[true, false, false, false, false, false, false, false], true],
  ];
  for (var [v,w] of valsExp) {
    assertEq(Bool16x8.anyTrue(Bool16x8(...v)), w);
  }
}

function testBool32x4not() {
  var valsExp = [
    [[true, false, true, false], [false, true, false, true]],
    [[true, true, false, false], [false, false, true, true]]
  ];
  for (var [v,w] of valsExp) {
    assertEqX4(Bool32x4.not(Bool32x4(...v)), w);
  }
}

function testBool32x4allTrue() {
  var valsExp = [
    [[false, false, false, false], false],
    [[true, false, true, false], false],
    [[true, true, true, true], true],
    [[true, true, false, false], false]
  ];
  for (var [v,w] of valsExp) {
    assertEq(Bool32x4.allTrue(Bool32x4(...v)), w);
  }
}

function testBool32x4anyTrue() {
  var valsExp = [
    [[false, false, false, false], false],
    [[true, false, true, false], true],
    [[true, true, true, true], true],
    [[true, true, false, false], true]
  ];
  for (var [v,w] of valsExp) {
    assertEq(Bool32x4.anyTrue(Bool32x4(...v)), w);
  }
}

function testBool64x2not() {
  var valsExp = [
    [[false, false], [true, true]],
    [[false, true], [true, false]],
    [[true, false], [false, true]],
    [[true, true], [false, false]]
  ];
  for (var [v,w] of valsExp) {
    assertEqX2(Bool64x2.not(Bool64x2(...v)), w);
  }
}

function testBool64x2allTrue() {
  var valsExp = [
    [[false, false], false],
    [[false, true], false],
    [[true, false], false],
    [[true, true], true]
  ];
  for (var [v,w] of valsExp) {
    assertEq(Bool64x2.allTrue(Bool64x2(...v)), w);
  }
}

function testBool64x2anyTrue() {
  var valsExp = [
    [[false, false], false],
    [[false, true], true],
    [[true, false], true],
    [[true, true], true]
  ];
  for (var [v,w] of valsExp) {
    assertEq(Bool64x2.anyTrue(Bool64x2(...v)), w);
  }
}

function test() {
  testFloat32x4abs();
  testFloat32x4neg();
  testFloat32x4reciprocalApproximation();
  testFloat32x4reciprocalSqrtApproximation();
  testFloat32x4sqrt();

  testInt8x16neg();
  testInt8x16not();

  testInt16x8neg();
  testInt16x8not();

  testInt32x4neg();
  testInt32x4not();

  testUint8x16neg();
  testUint8x16not();

  testUint16x8neg();
  testUint16x8not();

  testUint32x4neg();
  testUint32x4not();

  testBool8x16not();
  testBool8x16allTrue();
  testBool8x16anyTrue();

  testBool16x8not();
  testBool16x8allTrue();
  testBool16x8anyTrue();

  testBool32x4not();
  testBool32x4allTrue();
  testBool32x4anyTrue();

  testBool64x2not();
  testBool64x2allTrue();
  testBool64x2anyTrue();


  if (typeof reportCompare === "function") {
    reportCompare(true, true);
  }
}

test();
