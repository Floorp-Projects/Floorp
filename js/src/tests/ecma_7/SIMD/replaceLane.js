// |reftest| skip-if(!this.hasOwnProperty("SIMD"))
var float32x4 = SIMD.float32x4;
var int8x16 = SIMD.int8x16;
var int16x8 = SIMD.int16x8;
var int32x4 = SIMD.int32x4;
var float64x2 = SIMD.float64x2;

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

  var float32x4inputs = [
      [float32x4(1, 2, 3, 4), 5],
      [float32x4(1.87, 2.08, 3.84, 4.17), Math.fround(13.37)],
      [float32x4(NaN, -0, Infinity, -Infinity), 0]
  ];
  testType('float32x4', float32x4inputs);

  var v = float32x4inputs[1][0];
  assertEqX4(float32x4.replaceLane(v, 0), replaceLane0(simdToArray(v), NaN));
  assertEqX4(float32x4.replaceLane(v, 0, good), replaceLane0(simdToArray(v), good | 0));
  assertThrowsInstanceOf(() => float32x4.replaceLane(v, 0, bad), TestError);
  assertThrowsInstanceOf(() => float32x4.replaceLane(v, 4, good), TypeError);
  assertThrowsInstanceOf(() => float32x4.replaceLane(v, 1.1, good), TypeError);

  var float64x2inputs = [
      [float64x2(1, 2), 5],
      [float64x2(1.87, 2.08), Math.fround(13.37)],
      [float64x2(NaN, -0), 0]
  ];
  testType('float64x2', float64x2inputs);

  var v = float64x2inputs[1][0];
  assertEqX4(float64x2.replaceLane(v, 0), replaceLane0(simdToArray(v), NaN));
  assertEqX4(float64x2.replaceLane(v, 0, good), replaceLane0(simdToArray(v), good | 0));
  assertThrowsInstanceOf(() => float64x2.replaceLane(v, 0, bad), TestError);
  assertThrowsInstanceOf(() => float64x2.replaceLane(v, 2, good), TypeError);
  assertThrowsInstanceOf(() => float64x2.replaceLane(v, 1.1, good), TypeError);

  var int8x16inputs = [[int8x16(0, 1, 2, 3, 4, 5, 6, 7, -1, -2, -3, -4, -5, -6, INT8_MIN, INT8_MAX), 17]];
  testType('int8x16', int8x16inputs);

  var v = int8x16inputs[0][0];
  assertEqX16(int8x16.replaceLane(v, 0), replaceLane0(simdToArray(v), 0));
  assertEqX16(int8x16.replaceLane(v, 0, good), replaceLane0(simdToArray(v), good | 0));
  assertThrowsInstanceOf(() => int8x16.replaceLane(v, 0, bad), TestError);
  assertThrowsInstanceOf(() => int8x16.replaceLane(v, 16, good), TypeError);
  assertThrowsInstanceOf(() => int8x16.replaceLane(v, 1.1, good), TypeError);

  var int16x8inputs = [[int16x8(0, 1, 2, 3, -1, -2, INT16_MIN, INT16_MAX), 9]];
  testType('int16x8', int16x8inputs);

  var v = int16x8inputs[0][0];
  assertEqX8(int16x8.replaceLane(v, 0), replaceLane0(simdToArray(v), 0));
  assertEqX8(int16x8.replaceLane(v, 0, good), replaceLane0(simdToArray(v), good | 0));
  assertThrowsInstanceOf(() => int16x8.replaceLane(v, 0, bad), TestError);
  assertThrowsInstanceOf(() => int16x8.replaceLane(v, 8, good), TypeError);
  assertThrowsInstanceOf(() => int16x8.replaceLane(v, 1.1, good), TypeError);

  var int32x4inputs = [
      [int32x4(1, 2, 3, 4), 5],
      [int32x4(INT32_MIN, INT32_MAX, 3, 4), INT32_MIN],
  ];
  testType('int32x4', int32x4inputs);

  var v = int32x4inputs[1][0];
  assertEqX4(int32x4.replaceLane(v, 0), replaceLane0(simdToArray(v), 0));
  assertEqX4(int32x4.replaceLane(v, 0, good), replaceLane0(simdToArray(v), good | 0));
  assertThrowsInstanceOf(() => int32x4.replaceLane(v, 0, bad), TestError);
  assertThrowsInstanceOf(() => int32x4.replaceLane(v, 4, good), TypeError);
  assertThrowsInstanceOf(() => int32x4.replaceLane(v, 1.1, good), TypeError);

  if (typeof reportCompare === "function")
    reportCompare(true, true);
}

test();
