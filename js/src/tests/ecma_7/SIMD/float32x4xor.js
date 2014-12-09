// |reftest| skip-if(!this.hasOwnProperty("SIMD"))
var BUGNUMBER = 996076;
var float32x4 = SIMD.float32x4;
var int32x4 = SIMD.int32x4;

var summary = 'float32x4 and';

var xorf = (function() {
    var i = new Int32Array(3);
    var f = new Float32Array(i.buffer);
    return function(x, y) {
        f[0] = x;
        f[1] = y;
        i[2] = i[0] ^ i[1];
        return f[2];
    }
})();

function test() {
  print(BUGNUMBER + ": " + summary);

  var a = float32x4(1, 2, 3, 4);
  var b = float32x4(10, 20, 30, 40);
  var c = SIMD.float32x4.xor(a, b);
  assertEq(c.x, xorf(1, 10));
  assertEq(c.y, xorf(2, 20));
  assertEq(c.z, xorf(3, 30));
  assertEq(c.w, xorf(4, 40));

  var d = float32x4(1.07, 2.62, 3.79, 4.15);
  var e = float32x4(10.38, 20.47, 30.44, 40.16);
  var f = SIMD.float32x4.xor(d, e);
  assertEq(f.x, xorf(1.07, 10.38));
  assertEq(f.y, xorf(2.62, 20.47));
  assertEq(f.z, xorf(3.79, 30.44));
  assertEq(f.w, xorf(4.15, 40.16));

  var g = float32x4(NaN, -0, Infinity, -Infinity);
  var h = float32x4(-0, Infinity, -Infinity, NaN);
  var i = SIMD.float32x4.xor(g, h);
  assertEq(i.x, xorf(NaN, -0));
  assertEq(i.y, xorf(-0, Infinity));
  assertEq(i.z, xorf(Infinity, -Infinity));
  assertEq(i.w, xorf(-Infinity, NaN));

  if (typeof reportCompare === "function")
    reportCompare(true, true);
}

test();

