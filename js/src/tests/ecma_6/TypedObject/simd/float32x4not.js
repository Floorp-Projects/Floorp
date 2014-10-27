// |reftest| skip-if(!this.hasOwnProperty("SIMD"))
var BUGNUMBER = 996076;
var float32x4 = SIMD.float32x4;
var int32x4 = SIMD.int32x4;

var summary = 'float32x4 not';

var notf = (function() {
    var i = new Int32Array(1);
    var f = new Float32Array(i.buffer);
    return function(x) {
        f[0] = x;
        i[0] = ~i[0];
        return f[0];
    }
})();

function test() {
  print(BUGNUMBER + ": " + summary);

  var a = float32x4(2, 13, -37, 4.2);
  var c = SIMD.float32x4.not(a);
  assertEq(c.x, notf(2));
  assertEq(c.y, notf(13));
  assertEq(c.z, notf(-37));
  assertEq(c.w, notf(4.2));

  var d = float32x4(2.897, 13.245, -37.781, 5.28);
  var f = SIMD.float32x4.not(d);
  assertEq(f.x, notf(2.897));
  assertEq(f.y, notf(13.245));
  assertEq(f.z, notf(-37.781));
  assertEq(f.w, notf(5.28));

  var g = float32x4(NaN, -0, Infinity, -Infinity);
  var i = SIMD.float32x4.not(g);
  assertEq(i.x, notf(NaN));
  assertEq(i.y, notf(-0));
  assertEq(i.z, notf(Infinity));
  assertEq(i.w, notf(-Infinity));

  if (typeof reportCompare === "function")
    reportCompare(true, true);
}

test();

