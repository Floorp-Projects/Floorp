// |reftest| skip-if(!this.hasOwnProperty("SIMD"))
var float32x4 = SIMD.float32x4;
var int32x4 = SIMD.int32x4;
var float64x2 = SIMD.float64x2;


function replaceLane0(arr, x) {
    if (arr.length == 2)
        return [x, arr[1]];
    return [x, arr[1], arr[2], arr[3]];
}
function replaceLane1(arr, x) {
    if (arr.length == 2)
        return [arr[0], x];
    return [arr[0], x, arr[2], arr[3]];
}
function replaceLane2(arr, x) {
    return [arr[0], arr[1], x, arr[3]];
}
function replaceLane3(arr, x) {
    return [arr[0], arr[1], arr[2], x];
}

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
