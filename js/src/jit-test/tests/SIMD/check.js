load(libdir + 'simd.js');

setJitCompilerOption("ion.warmup.trigger", 50);

function f() {
    var f1 = SIMD.Float32x4(1, 2, 3, 4);
    var i1 = SIMD.Int32x4(1, 2, -3, 4);
    var i = 0;
    try {
        for (; i < 150; i++) {
            if (i > 148)
                i1 = f1;
            assertEqVec(SIMD.Int32x4.check(i1), i1);
            assertEqVec(SIMD.Float32x4.check(f1), f1);
        }
    } catch (ex) {
        assertEq(i, 149);
        assertEq(ex instanceof TypeError, true);
    }
}

f();

