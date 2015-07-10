// |reftest| skip-if(!this.hasOwnProperty("SIMD"))
var Float32x4 = SIMD.Float32x4;
var Int8x16 = SIMD.Int8x16;
var Int16x8 = SIMD.Int16x8;
var Int32x4 = SIMD.Int32x4;

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

function testFloat32x4not() {
  var notf = (function() {
    var i = new Int32Array(1);
    var f = new Float32Array(i.buffer);
    return function(x) {
      f[0] = x;
      i[0] = ~i[0];
      return f[0];
    };
  })();

  var vals = [
    [2, 13, -37, 4.2],
    [2.897, 13.245, -37.781, 5.28],
    [NaN, -0, Infinity, -Infinity]
  ];

  for (var v of vals) {
    assertEqX4(Float32x4.not(Float32x4(...v)), v.map(notf));
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

function test() {
  testFloat32x4abs();
  testFloat32x4neg();
  testFloat32x4not();
  testFloat32x4reciprocalApproximation();
  testFloat32x4reciprocalSqrtApproximation();
  testFloat32x4sqrt();

  testInt8x16neg();
  testInt8x16not();

  testInt16x8neg();
  testInt16x8not();

  testInt32x4neg();
  testInt32x4not();

  if (typeof reportCompare === "function") {
    reportCompare(true, true);
  }
}

test();
