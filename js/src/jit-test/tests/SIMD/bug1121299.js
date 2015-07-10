if (!this.hasOwnProperty("SIMD"))
  quit();

setJitCompilerOption("baseline.warmup.trigger", 10);
setJitCompilerOption("ion.warmup.trigger", 30);

function test_1(i) {
  if (i >= 40)
    return;
  var a = SIMD.Float32x4(1.1, 2.2, 3.3, 4.6);
  SIMD.Int32x4.fromFloat32x4(a);
  test_1(i + 1);
}
test_1(0);


var Float32x4 = SIMD.Float32x4;
function test_2() {
    var Array = Float32x4.array(3);
    var array = new Array([
        Float32x4(1, 2, 3, 4),
        Float32x4(5, 6, 7, 8),
        Float32x4(9, 10, 11, 12)
    ]);
    if (typeof reportCompare === "function")
        reportCompare(true, true);
}
test_2();
evaluate("test_2(); test_2();", {
    isRunOnce: true,
});
