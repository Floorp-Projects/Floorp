if (!this.hasOwnProperty("SIMD"))
  quit();

var float64x2 = SIMD.float64x2;
function test() {
  var a = float64x2(1, 2);
}
test();
test();
