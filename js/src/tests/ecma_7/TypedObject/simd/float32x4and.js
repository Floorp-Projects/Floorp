// |reftest| skip-if(!this.hasOwnProperty("SIMD"))
var BUGNUMBER = 996076;
var float32x4 = SIMD.float32x4;
var int32x4 = SIMD.int32x4;

var summary = 'float32x4 and';

var andf = (function() {
    var i = new Int32Array(3);
    var f = new Float32Array(i.buffer);
    return function(x, y) {
        f[0] = x;
        f[1] = y;
        i[2] = i[0] & i[1];
        return f[2];
    }
})();

function test() {
  print(BUGNUMBER + ": " + summary);

  var a = float32x4(1, 2, 3, 4);
  var b = float32x4(10, 20, 30, 40);
  var c = SIMD.float32x4.and(a, b);
  assertEq(c.x, andf(1, 10));
  assertEq(c.y, andf(2, 20));
  assertEq(c.z, andf(3, 30));
  assertEq(c.w, andf(4, 40));

  var d = float32x4(1.51, 2.98, 3.65, 4.34);
  var e = float32x4(10.29, 20.12, 30.79, 40.41);
  var f = SIMD.float32x4.and(d, e);
  assertEq(f.x, andf(1.51, 10.29));
  assertEq(f.y, andf(2.98, 20.12));
  assertEq(f.z, andf(3.65, 30.79));
  assertEq(f.w, andf(4.34, 40.41));

  var g = float32x4(NaN, -0, Infinity, -Infinity);
  var h = float32x4(NaN, -0, -Infinity, Infinity);
  var i = SIMD.float32x4.and(g, h);
  assertEq(i.x, NaN);
  assertEq(i.y, -0);
  assertEq(i.z, Infinity);
  assertEq(i.w, Infinity);

  if (typeof reportCompare === "function")
    reportCompare(true, true);
}

test();

