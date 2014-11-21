// |reftest| skip-if(!this.hasOwnProperty("SIMD"))
var BUGNUMBER = 946042;

var float32x4 = SIMD.float32x4;

function testMaxFloat32(v, w) {
    return testBinaryFunc(v, w, float32x4.max, (x, y) => Math.fround(Math.max(x, y)));
}
function testMinFloat32(v, w) {
    return testBinaryFunc(v, w, float32x4.min, (x, y) => Math.fround(Math.min(x, y)));
}

function maxNum(x, y) {
    if (x != x)
        return y;
    if (y != y)
        return x;
    return Math.max(x, y);
}

function minNum(x, y) {
    if (x != x)
        return y;
    if (y != y)
        return x;
    return Math.min(x, y);
}

function testMaxNumFloat32(v, w) {
    return testBinaryFunc(v, w, float32x4.maxNum, maxNum);
}
function testMinNumFloat32(v, w) {
    return testBinaryFunc(v, w, float32x4.minNum, minNum);
}

function test() {
  print(BUGNUMBER + ": " + summary);

  for ([v, w] of [[float32x4(1, 20, 30, 4), float32x4(10, 2, 3, 40)],
                  [float32x4(9.999, 2.1234, 30.4443, 4), float32x4(10, 2.1233, 30.4444, 4.0001)],
                  [float32x4(NaN, -Infinity, +Infinity, -0), float32x4(13.37, 42.42, NaN, 0)]])
  {
      testMinFloat32(v, w);
      testMaxFloat32(v, w);
      testMinNumFloat32(v, w);
      testMaxNumFloat32(v, w);
  }

  if (typeof reportCompare === "function")
    reportCompare(true, true);
}

test();

