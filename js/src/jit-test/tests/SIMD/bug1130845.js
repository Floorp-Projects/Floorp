if (!this.hasOwnProperty("SIMD"))
  quit();

var int32x4 = SIMD.int32x4;
function test() {
  var a = int32x4();
  var b = int32x4(10, 20, 30, 40);
  var c = SIMD.int32x4.and(a, b);
  assertEq(c.x, 0);
  return 0;
}
test();
var u = [], v = [];
for (var j=0; j<u.length; ++j)
    v[test()] = t;
