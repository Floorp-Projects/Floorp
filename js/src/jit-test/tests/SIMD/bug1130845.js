if (!this.hasOwnProperty("SIMD"))
  quit();

var Int32x4 = SIMD.Int32x4;
function test() {
  var a = Int32x4();
  var b = Int32x4(10, 20, 30, 40);
  var c = SIMD.Int32x4.and(a, b);
  assertEq(Int32x4.extractLane(c, 0), 0);
  return 0;
}
test();
var u = [], v = [];
for (var j=0; j<u.length; ++j)
    v[test()] = t;
