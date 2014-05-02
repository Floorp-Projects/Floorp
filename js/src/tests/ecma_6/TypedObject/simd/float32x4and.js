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

  // FIXME -- Bug 948379: Amend to check for correctness of border cases.

  var a = float32x4(1, 2, 3, 4);
  var b = float32x4(10, 20, 30, 40);
  var c = SIMD.float32x4.and(a, b);
  assertEq(c.x, andf(1, 10));
  assertEq(c.y, andf(2, 20));
  assertEq(c.z, andf(3, 30));
  assertEq(c.w, andf(4, 40));

  if (typeof reportCompare === "function")
    reportCompare(true, true);
}

test();

