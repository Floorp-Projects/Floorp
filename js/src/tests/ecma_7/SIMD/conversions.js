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

  // Test rounding to nearest, break tie with even
  var f1 = makeFloat(0, 127, 0);
  assertEq(f1, 1);

  var f2 = makeFloat(0, 127, 1);
  var d = makeDouble(0, 1023, 0x0000020000000);
  assertEq(f2, d);

  var mid = makeDouble(0, 1023, 0x0000010000000);
  assertEq((1 + d) / 2, mid);

  var nextMid = makeDouble(0, 1023, 0x0000010000001);

  // mid is halfway between f1 and f2 => tie to even, which is f1
  var v = float64x2(mid, nextMid);
  assertEqX4(float32x4.fromFloat64x2(v), [f1, f2, 0, 0]);

  var f3 = makeFloat(0, 127, 2);
  var d = makeDouble(0, 1023, 0x0000040000000);
  assertEq(f3, d);

  mid = makeDouble(0, 1023, 0x0000030000000);
  assertEq((f2 + f3) / 2, mid);

  // same here. tie to even, which is f3 here
  nextMid = makeDouble(0, 1023, 0x0000030000001);
  var v = float64x2(mid, nextMid);
  assertEqX4(float32x4.fromFloat64x2(v), [f3, f3, 0, 0]);

  // Test boundaries
  var biggestFloat = makeFloat(0, 127 + 127, 0x7fffff);
  assertEq(makeDouble(0, 1023 + 127, 0xfffffe0000000), biggestFloat);

  var lowestFloat = makeFloat(1, 127 + 127, 0x7fffff);
  assertEq(makeDouble(1, 1023 + 127, 0xfffffe0000000), lowestFloat);

  var v = float64x2(lowestFloat, biggestFloat);
  assertEqX4(float32x4.fromFloat64x2(v), [lowestFloat, biggestFloat, 0, 0]);

  var v = float64x2(makeDouble(0, 1023 + 127, 0xfffffe0000001), makeDouble(0, 1023 + 127, 0xffffff0000000));
  assertEqX4(float32x4.fromFloat64x2(v), [biggestFloat, Infinity, 0, 0]);
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

  // Check that rounding to nearest, even is applied.
  {
      var num = makeFloat(0, 150 + 2, 0);
      var next = makeFloat(0, 150 + 2, 1);
      assertEq(num + 4, next);

      v = float32x4.fromInt32x4(int32x4(num, num + 1, num + 2, num + 3));
      assertEqX4(v, [num, num, /* even */ num, next]);
  }

  {
      var num = makeFloat(0, 150 + 2, 1);
      var next = makeFloat(0, 150 + 2, 2);
      assertEq(num + 4, next);

      v = float32x4.fromInt32x4(int32x4(num, num + 1, num + 2, num + 3));
      assertEqX4(v, [num, num, /* even */ next, next]);
  }

  {
      var last = makeFloat(0, 157, 0x7fffff);

      assertEq(last, Math.fround(last), "float");
      assertEq(last < Math.pow(2, 31), true, "less than 2**31");
      assertEq(last | 0, last, "it should be an integer, as exponent >= 150");

      var diff = (Math.pow(2, 31) - 1) - last;
      v = float32x4.fromInt32x4(int32x4(Math.pow(2, 31) - 1,
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
  var d = float32x4(1.1, 2.2, 3.3, 4.6);
  assertEqX4(int32x4.fromFloat32x4(d), [1, 2, 3, 4]);

  var d = float32x4(NaN, 0, 0, 0);
  assertThrowsInstanceOf(() => SIMD.int32x4.fromFloat32x4(d), RangeError);

  var d = float32x4(Infinity, 0, 0, 0);
  assertThrowsInstanceOf(() => SIMD.int32x4.fromFloat32x4(d), RangeError);

  var d = float32x4(-Infinity, 0, 0, 0);
  assertThrowsInstanceOf(() => SIMD.int32x4.fromFloat32x4(d), RangeError);

  // Test high boundaries: float(0, 157, 0x7fffff) < INT32_MAX < float(0, 158, 0)
  var d = float32x4(makeFloat(0, 127 + 31, 0), 0, 0, 0);
  assertThrowsInstanceOf(() => SIMD.int32x4.fromFloat32x4(d), RangeError);

  var lastFloat = makeFloat(0, 127 + 30, 0x7FFFFF);
  var d = float32x4(lastFloat, 0, 0, 0);
  var e = SIMD.int32x4.fromFloat32x4(d);
  assertEqX4(e, [lastFloat, 0, 0, 0]);

  // Test low boundaries
  assertEq(makeFloat(1, 127 + 31, 0), INT32_MIN);
  var d = float32x4(makeFloat(1, 127 + 31, 0), 0, 0, 0);
  var e = SIMD.int32x4.fromFloat32x4(d);
  assertEqX4(e, [INT32_MIN, 0, 0, 0]);

  var d = float32x4(makeFloat(1, 127 + 31, 1), 0, 0, 0);
  assertThrowsInstanceOf(() => SIMD.int32x4.fromFloat32x4(d), RangeError);
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
  assertEqX4(int32x4.fromFloat64x2(float64x2(1, 2.2)), [1, 2, 0, 0]);

  var g = float64x2(Infinity, 0);
  assertThrowsInstanceOf(() => int32x4.fromFloat64x2(g), RangeError);

  var g = float64x2(-Infinity, 0);
  assertThrowsInstanceOf(() => int32x4.fromFloat64x2(g), RangeError);

  var g = float64x2(NaN, 0);
  assertThrowsInstanceOf(() => int32x4.fromFloat64x2(g), RangeError);

  // Testing high boundaries
  // double(0, 1023 + 30, 0) < INT32_MAX < double(0, 1023 + 31, 0), so the
  // lowest exactly representable quantity at this scale is 2**(-52 + 30) ==
  // 2**-22.
  assertEq(makeDouble(0, 1023 + 30, 0) + Math.pow(2, -22), makeDouble(0, 1023 + 30, 1));
  assertEq(makeDouble(0, 1023 + 30, 0) + Math.pow(2, -23), makeDouble(0, 1023 + 30, 0));

  var g = float64x2(INT32_MAX, 0);
  assertEqX4(int32x4.fromFloat64x2(g), [INT32_MAX, 0, 0, 0]);

  var g = float64x2(INT32_MAX + Math.pow(2, -22), 0);
  assertThrowsInstanceOf(() => int32x4.fromFloat64x2(g), RangeError);

  // Testing low boundaries
  assertEq(makeDouble(1, 1023 + 31, 0), INT32_MIN);

  var g = float64x2(makeDouble(1, 1023 + 31, 0), 0);
  assertEqX4(int32x4.fromFloat64x2(g), [INT32_MIN, 0, 0, 0]);

  var g = float64x2(makeDouble(1, 1023 + 31, 1), 0);
  assertThrowsInstanceOf(() => int32x4.fromFloat64x2(g), RangeError);

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
