// |reftest| skip-if(!this.hasOwnProperty("SIMD"))
var float32x4 = SIMD.float32x4;
var float64x2 = SIMD.float64x2;
var int32x4 = SIMD.int32x4;

function testFloat32x4FromFloat64x2() {
  function expected(v) {
    return [...(v.map(Math.fround)), 0, 0];
  }

  var vals = [
    [1, 2],
    [-0, NaN],
    [Infinity, -Infinity],
    [Math.pow(2, 25) - 1, -Math.pow(25)],
    [Math.pow(2, 1000), Math.pow(2, -1000)]
  ];

  for (var v of vals) {
    assertEqX4(float32x4.fromFloat64x2(float64x2(...v)), expected(v));
  }
}

function testFloat32x4FromFloat64x2Bits() {
  var valsExp = [
    [[2.000000473111868, 512.0001225471497], [1.0, 2.0, 3.0, 4.0]],
    [[-0, NaN], [0, -0, 0, NaN]],
    [[Infinity, -Infinity], [0, NaN, 0, NaN]]
  ];

  for (var [v,w] of valsExp) {
    assertEqX4(float32x4.fromFloat64x2Bits(float64x2(...v)), w);
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
    assertEqX4(float32x4.fromInt32x4(int32x4(...v)), expected(v));
  }
}

function testFloat32x4FromInt32x4Bits() {
  var valsExp = [
    [[100, 200, 300, 400], [1.401298464324817e-43, 2.802596928649634e-43, 4.203895392974451e-43, 5.605193857299268e-43]],
    [[INT32_MIN, INT32_MAX, 0, 0], [-0, NaN, 0, 0]]
  ];

  for (var [v,w] of valsExp) {
    assertEqX4(float32x4.fromInt32x4Bits(int32x4(...v)), w);
  }
}

function testFloat64x2FromFloat32x4() {
  function expected(v) {
    return v.slice(0, 2).map(Math.fround);
  }

  var vals = [
    [100, 200, 300, 400],
    [NaN, -0, NaN, -0],
    [Infinity, -Infinity, Infinity, -Infinity],
    [13.37, 12.853, 49.97, 53.124]
  ];

  for (var v of vals) {
    assertEqX2(float64x2.fromFloat32x4(float32x4(...v)), expected(v));
  }
}

function testFloat64x2FromFloat32x4Bits() {
  var valsExp = [
    [[0, 1.875, 0, 2], [1.0, 2.0]],
    [[NaN, -0, Infinity, -Infinity], [-1.058925634e-314, -1.404448428688076e+306]]
  ];

  for (var [v,w] of valsExp) {
    assertEqX2(float64x2.fromFloat32x4Bits(float32x4(...v)), w);
  }
}

function testFloat64x2FromInt32x4() {
  function expected(v) {
    return v.slice(0, 2);
  }

  var vals = [
    [1, 2, 3, 4],
    [INT32_MAX, INT32_MIN, 0, 0]
  ];

  for (var v of vals) {
    assertEqX2(float64x2.fromInt32x4(int32x4(...v)), expected(v));
  }
}

function testFloat64x2FromInt32x4Bits() {
  var valsExp = [
    [[0x00000000, 0x3ff00000, 0x0000000, 0x40000000], [1.0, 2.0]],
    [[0xabcdef12, 0x3ff00000, 0x21fedcba, 0x40000000], [1.0000006400213732, 2.0000002532866263]]
  ];

  for (var [v,w] of valsExp) {
    assertEqX2(float64x2.fromInt32x4Bits(int32x4(...v)), w);
  }
}

function testInt32x4FromFloat32x4() {
  var valsExp = [
    [[1.1, 2.2, 3.3, 4.6], [1, 2, 3, 4]],
    [[NaN, -0, Infinity, -Infinity], [0, 0, 0, 0]]
  ];

  for (var [v,w] of valsExp) {
    assertEqX4(int32x4.fromFloat32x4(float32x4(...v)), w);
  }
}

function testInt32x4FromFloat32x4Bits() {
  var valsExp = [
    [[1, 2, 3, 4], [0x3f800000 | 0, 0x40000000 | 0, 0x40400000 | 0, 0x40800000 | 0]],
    [[NaN, -0, Infinity, -Infinity], [0x7fc00000 | 0, 0x80000000 | 0, 0x7f800000 | 0, 0xff800000 | 0]]
  ];

  for (var [v,w] of valsExp) {
    assertEqX4(int32x4.fromFloat32x4Bits(float32x4(...v)), w);
  }
}

function testInt32x4FromFloat64x2() {
  var valsExp = [
    [[1, 2.2], [1, 2, 0, 0]],
    [[NaN, -0], [0, 0, 0, 0]],
    [[Infinity, -Infinity], [0, 0, 0, 0]],
    [[Math.pow(2, 31), -Math.pow(2, 31) - 1], [INT32_MIN, INT32_MAX, 0, 0]]
  ];

  for (var [v,w] of valsExp) {
    assertEqX4(int32x4.fromFloat64x2(float64x2(...v)), w);
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
    assertEqX4(int32x4.fromFloat64x2Bits(float64x2(...v)), w);
  }
}

function test() {
  testFloat32x4FromFloat64x2();
  testFloat32x4FromFloat64x2Bits();
  testFloat32x4FromInt32x4();
  testFloat32x4FromInt32x4Bits();

  testFloat64x2FromFloat32x4();
  testFloat64x2FromFloat32x4Bits();
  testFloat64x2FromInt32x4();
  testFloat64x2FromInt32x4Bits();

  testInt32x4FromFloat32x4();
  testInt32x4FromFloat32x4Bits();
  testInt32x4FromFloat64x2();
  testInt32x4FromFloat64x2Bits();

  if (typeof reportCompare === "function") {
    reportCompare(true, true);
  }
}

test();
