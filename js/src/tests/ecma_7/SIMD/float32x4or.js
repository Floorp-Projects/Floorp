// |reftest| skip-if(!this.hasOwnProperty("SIMD"))
var BUGNUMBER = 996076;
var float32x4 = SIMD.float32x4;
var int32x4 = SIMD.int32x4;

var summary = 'float32x4 and';

var orf = (function() {
    var i = new Int32Array(3);
    var f = new Float32Array(i.buffer);
    return function(x, y) {
        f[0] = x;
        f[1] = y;
        i[2] = i[0] | i[1];
        return f[2];
    }
})();

function test() {
  print(BUGNUMBER + ": " + summary);

  var a = float32x4(1, 2, 3, 4);
  var b = float32x4(10, 20, 30, 40);
  var c = SIMD.float32x4.or(a, b);
  assertEq(c.x, orf(1, 10));
  assertEq(c.y, orf(2, 20));
  assertEq(c.z, orf(3, 30));
  assertEq(c.w, orf(4, 40));

  var d = float32x4(1.12, 2.39, 3.83, 4.57);
  var e = float32x4(10.76, 20.41, 30.96, 40.23);
  var f = SIMD.float32x4.or(d, e);
  assertEq(f.x, orf(1.12, 10.76));
  assertEq(f.y, orf(2.39, 20.41));
  assertEq(f.z, orf(3.83, 30.96));
  assertEq(f.w, orf(4.57, 40.23));

  var g = float32x4(NaN, -0, Infinity, -Infinity);
  var h = float32x4(5, 5, -Infinity, Infinity);
  var i = SIMD.float32x4.or(g, h);
  assertEq(i.x, orf(NaN, 5));
  assertEq(i.y, orf(-0, 5));
  assertEq(i.z, orf(Infinity, -Infinity));
  assertEq(i.w, orf(-Infinity, Infinity));

  if (typeof reportCompare === "function")
    reportCompare(true, true);
}

test();

