setJitCompilerOption("ion.warmup.trigger", 50);

function f() {
    var f1 = SIMD.float32x4(1, 2, 3, 4);
    var f2 = SIMD.float32x4(4, 3, 2, 1);
    var r = SIMD.float32x4(0, 0, 0, 0);
    for (var i = 0; i < 150; i++) {
        r = SIMD.float32x4.div(f1, f2);
        r = SIMD.float32x4.max(f1, r);
        r = SIMD.float32x4.min(r, f2);
        r = SIMD.float32x4.maxNum(f1, r);
        r = SIMD.float32x4.minNum(r, f2);
    }
    return r;
}

f();
