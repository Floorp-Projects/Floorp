load(libdir + 'simd.js');

setJitCompilerOption("ion.warmup.trigger", 50);

function f() {
    var i4 = SIMD.int32x4(1, -2, 3, -4);

    var v = Math.fround(13.37);
    var f4 = SIMD.float32x4(13.37, NaN, Infinity, -0);

    for (var i = 0; i < 150; i++) {
        assertEq(i4.x, 1);
        assertEq(i4.y, -2);
        assertEq(i4.z, 3);
        assertEq(i4.w, -4);

        assertEq(i4.signMask, 0b1010);

        assertEq(f4.x, v);
        assertEq(f4.y, NaN);
        assertEq(f4.z, Infinity);
        assertEq(f4.w, -0);

        assertEq(f4.signMask, 0b1000);
    }
}

f();

