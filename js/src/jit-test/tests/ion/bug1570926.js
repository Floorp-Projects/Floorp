if (getJitCompilerOptions()["baseline.warmup.trigger"] > 10) {
    setJitCompilerOption("baseline.warmup.trigger", 10);
}
if (getJitCompilerOptions()["ion.warmup.trigger"] > 20) {
    setJitCompilerOption("ion.warmup.trigger", 20);
}
setJitCompilerOption("offthread-compilation.enable", 0);

var arr = [1, 2, 3, 4, 5];
function f(x) {
    for (var i = x; i < 5; i++) {
        arr[i - 2];
    }
}
for (var i = 0; i < 15; i++) {
    f(2);
}
assertEq(f(0), undefined);
