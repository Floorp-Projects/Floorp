// |reftest| skip-if(!this.hasOwnProperty("SIMD"))
var Float32x4 = SIMD.Float32x4;
var Int8x16 = SIMD.Int8x16;
var Int16x8 = SIMD.Int16x8;
var Int32x4 = SIMD.Int32x4;
var Float64x2 = SIMD.Float64x2;

function replaceLaneN(laneIndex, arr, value) {
    var copy = arr.slice();
    assertEq(laneIndex <= arr.length, true);
    copy[laneIndex] = value;
    return copy;
}

var replaceLane0 = replaceLaneN.bind(null, 0);
var replaceLane1 = replaceLaneN.bind(null, 1);
var replaceLane2 = replaceLaneN.bind(null, 2);
var replaceLane3 = replaceLaneN.bind(null, 3);
var replaceLane4 = replaceLaneN.bind(null, 4);
var replaceLane5 = replaceLaneN.bind(null, 5);
var replaceLane6 = replaceLaneN.bind(null, 6);
var replaceLane7 = replaceLaneN.bind(null, 7);
var replaceLane8 = replaceLaneN.bind(null, 8);
var replaceLane9 = replaceLaneN.bind(null, 9);
var replaceLane10 = replaceLaneN.bind(null, 10);
var replaceLane11 = replaceLaneN.bind(null, 11);
var replaceLane12 = replaceLaneN.bind(null, 12);
var replaceLane13 = replaceLaneN.bind(null, 13);
var replaceLane14 = replaceLaneN.bind(null, 14);
var replaceLane15 = replaceLaneN.bind(null, 15);

function testReplaceLane(vec, scalar, simdFunc, func) {
    var varr = simdToArray(vec);
    var observed = simdToArray(simdFunc(vec, scalar));
    var expected = func(varr, scalar);
    for (var i = 0; i < observed.length; i++)
        assertEq(observed[i], expected[i]);
}

function test() {
  function testType(type, inputs) {
      var length = simdToArray(inputs[0][0]).length;
      for (var [vec, s] of inputs) {
          testReplaceLane(vec, s, (x,y) => SIMD[type].replaceLane(x, 0, y), replaceLane0);
          testReplaceLane(vec, s, (x,y) => SIMD[type].replaceLane(x, 1, y), replaceLane1);
          if (length <= 2)
              continue;
          testReplaceLane(vec, s, (x,y) => SIMD[type].replaceLane(x, 2, y), replaceLane2);
          testReplaceLane(vec, s, (x,y) => SIMD[type].replaceLane(x, 3, y), replaceLane3);
          if (length <= 4)
              continue;
          testReplaceLane(vec, s, (x,y) => SIMD[type].replaceLane(x, 4, y), replaceLane4);
          testReplaceLane(vec, s, (x,y) => SIMD[type].replaceLane(x, 5, y), replaceLane5);
          testReplaceLane(vec, s, (x,y) => SIMD[type].replaceLane(x, 6, y), replaceLane6);
          testReplaceLane(vec, s, (x,y) => SIMD[type].replaceLane(x, 7, y), replaceLane7);
          if (length <= 8)
              continue;
          testReplaceLane(vec, s, (x,y) => SIMD[type].replaceLane(x, 8, y), replaceLane8);
          testReplaceLane(vec, s, (x,y) => SIMD[type].replaceLane(x, 9, y), replaceLane9);
          testReplaceLane(vec, s, (x,y) => SIMD[type].replaceLane(x, 10, y), replaceLane10);
          testReplaceLane(vec, s, (x,y) => SIMD[type].replaceLane(x, 11, y), replaceLane11);
          testReplaceLane(vec, s, (x,y) => SIMD[type].replaceLane(x, 12, y), replaceLane12);
          testReplaceLane(vec, s, (x,y) => SIMD[type].replaceLane(x, 13, y), replaceLane13);
          testReplaceLane(vec, s, (x,y) => SIMD[type].replaceLane(x, 14, y), replaceLane14);
          testReplaceLane(vec, s, (x,y) => SIMD[type].replaceLane(x, 15, y), replaceLane15);
      }
  }

  function TestError(){};
  var good = {valueOf: () => 42};
  var bad = {valueOf: () => {throw new TestError(); }};

  var Float32x4inputs = [
      [Float32x4(1, 2, 3, 4), 5],
      [Float32x4(1.87, 2.08, 3.84, 4.17), Math.fround(13.37)],
      [Float32x4(NaN, -0, Infinity, -Infinity), 0]
  ];
  testType('Float32x4', Float32x4inputs);

  var v = Float32x4inputs[1][0];
  assertEqX4(Float32x4.replaceLane(v, 0), replaceLane0(simdToArray(v), NaN));
  assertEqX4(Float32x4.replaceLane(v, 0, good), replaceLane0(simdToArray(v), good | 0));
  assertThrowsInstanceOf(() => Float32x4.replaceLane(v, 0, bad), TestError);
  assertThrowsInstanceOf(() => Float32x4.replaceLane(v, 4, good), TypeError);
  assertThrowsInstanceOf(() => Float32x4.replaceLane(v, 1.1, good), TypeError);

  var Float64x2inputs = [
      [Float64x2(1, 2), 5],
      [Float64x2(1.87, 2.08), Math.fround(13.37)],
      [Float64x2(NaN, -0), 0]
  ];
  testType('Float64x2', Float64x2inputs);

  var v = Float64x2inputs[1][0];
  assertEqX2(Float64x2.replaceLane(v, 0), replaceLane0(simdToArray(v), NaN));
  assertEqX2(Float64x2.replaceLane(v, 0, good), replaceLane0(simdToArray(v), good | 0));
  assertThrowsInstanceOf(() => Float64x2.replaceLane(v, 0, bad), TestError);
  assertThrowsInstanceOf(() => Float64x2.replaceLane(v, 2, good), TypeError);
  assertThrowsInstanceOf(() => Float64x2.replaceLane(v, 1.1, good), TypeError);

  var Int8x16inputs = [[Int8x16(0, 1, 2, 3, 4, 5, 6, 7, -1, -2, -3, -4, -5, -6, INT8_MIN, INT8_MAX), 17]];
  testType('Int8x16', Int8x16inputs);

  var v = Int8x16inputs[0][0];
  assertEqX16(Int8x16.replaceLane(v, 0), replaceLane0(simdToArray(v), 0));
  assertEqX16(Int8x16.replaceLane(v, 0, good), replaceLane0(simdToArray(v), good | 0));
  assertThrowsInstanceOf(() => Int8x16.replaceLane(v, 0, bad), TestError);
  assertThrowsInstanceOf(() => Int8x16.replaceLane(v, 16, good), TypeError);
  assertThrowsInstanceOf(() => Int8x16.replaceLane(v, 1.1, good), TypeError);

  var Int16x8inputs = [[Int16x8(0, 1, 2, 3, -1, -2, INT16_MIN, INT16_MAX), 9]];
  testType('Int16x8', Int16x8inputs);

  var v = Int16x8inputs[0][0];
  assertEqX8(Int16x8.replaceLane(v, 0), replaceLane0(simdToArray(v), 0));
  assertEqX8(Int16x8.replaceLane(v, 0, good), replaceLane0(simdToArray(v), good | 0));
  assertThrowsInstanceOf(() => Int16x8.replaceLane(v, 0, bad), TestError);
  assertThrowsInstanceOf(() => Int16x8.replaceLane(v, 8, good), TypeError);
  assertThrowsInstanceOf(() => Int16x8.replaceLane(v, 1.1, good), TypeError);

  var Int32x4inputs = [
      [Int32x4(1, 2, 3, 4), 5],
      [Int32x4(INT32_MIN, INT32_MAX, 3, 4), INT32_MIN],
  ];
  testType('Int32x4', Int32x4inputs);

  var v = Int32x4inputs[1][0];
  assertEqX4(Int32x4.replaceLane(v, 0), replaceLane0(simdToArray(v), 0));
  assertEqX4(Int32x4.replaceLane(v, 0, good), replaceLane0(simdToArray(v), good | 0));
  assertThrowsInstanceOf(() => Int32x4.replaceLane(v, 0, bad), TestError);
  assertThrowsInstanceOf(() => Int32x4.replaceLane(v, 4, good), TypeError);
  assertThrowsInstanceOf(() => Int32x4.replaceLane(v, 1.1, good), TypeError);

  if (typeof reportCompare === "function")
    reportCompare(true, true);
}

test();
