if (!this.hasOwnProperty("SIMD"))
  quit();

setJitCompilerOption("baseline.warmup.trigger", 10);
setJitCompilerOption("ion.warmup.trigger", 30);

function test_1(i) {
  if (i >= 40)
    return;
  var a = SIMD.float32x4(1.1, 2.2, 3.3, 4.6);
  SIMD.int32x4.fromFloat32x4(a);
  test_1(i + 1);
}
test_1(0);


var float32x4 = SIMD.float32x4;
function test_2() {
    var Array = float32x4.array(3);
    var array = new Array([
        float32x4(1, 2, 3, 4),
        float32x4(5, 6, 7, 8),
        float32x4(9, 10, 11, 12)
    ]);
    if (typeof reportCompare === "function")
        reportCompare(true, true);
}
test_2();
evaluate("test_2(); test_2();", {
    compileAndGo: true
});
