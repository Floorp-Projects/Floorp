load(libdir + 'simd.js');

setJitCompilerOption("ion.warmup.trigger", 50);

function f() {
    var i4 = SIMD.Int32x4(1, -2, 3, -4);

    var v = Math.fround(13.37);
    var f4 = SIMD.Float32x4(13.37, NaN, Infinity, -0);

    for (var i = 0; i < 150; i++) {
        assertEq(SIMD.Int32x4.extractLane(i4, 0), 1);
        assertEq(SIMD.Int32x4.extractLane(i4, 1), -2);
        assertEq(SIMD.Int32x4.extractLane(i4, 2), 3);
        assertEq(SIMD.Int32x4.extractLane(i4, 3), -4);

        assertEq(i4.signMask, 0b1010);

        assertEq(SIMD.Float32x4.extractLane(f4, 0), v);
        assertEq(SIMD.Float32x4.extractLane(f4, 1), NaN);
        assertEq(SIMD.Float32x4.extractLane(f4, 2), Infinity);
        assertEq(SIMD.Float32x4.extractLane(f4, 3), -0);

        assertEq(f4.signMask, 0b1000);
    }
}

f();

