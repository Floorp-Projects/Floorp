setJitCompilerOption("ion.warmup.trigger", 50);

function f() {
    var f1 = SIMD.float32x4(1, 2, 3, 4);
    var f2 = SIMD.float32x4(4, 3, 2, 1);
    var r = SIMD.float32x4(0, 0, 0, 0);
    for (var i = 0; i < 150; i++) {
        r = SIMD.float32x4.and(f1, f2);
        r = SIMD.float32x4.or(f1, r);
        r = SIMD.float32x4.xor(r, f2);
    }
    return r;
}

f();
