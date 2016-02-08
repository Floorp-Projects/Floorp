load(libdir + 'simd.js');

setJitCompilerOption("ion.warmup.trigger", 50);

function f() {
    var i4 = SIMD.Int32x4(1, -2, 3, -4);
    var u4 = SIMD.Uint32x4(1, -2, 3, 0x88000000);
    var b4 = SIMD.Bool32x4(true, true, false, true);


    var bt4 = SIMD.Bool32x4(true, true, true, true);
    var bf4 = SIMD.Bool32x4(false, false, false, false);

    var v = Math.fround(13.37);
    var f4 = SIMD.Float32x4(13.37, NaN, Infinity, -0);

    for (var i = 0; i < 150; i++) {
        assertEq(SIMD.Int32x4.extractLane(i4, 0), 1);
        assertEq(SIMD.Int32x4.extractLane(i4, 1), -2);
        assertEq(SIMD.Int32x4.extractLane(i4, 2), 3);
        assertEq(SIMD.Int32x4.extractLane(i4, 3), -4);

        assertEq(SIMD.Uint32x4.extractLane(u4, 0), 1);
        assertEq(SIMD.Uint32x4.extractLane(u4, 1), -2 >>> 0);
        assertEq(SIMD.Uint32x4.extractLane(u4, 2), 3);
        assertEq(SIMD.Uint32x4.extractLane(u4, 3), 0x88000000);

        assertEq(SIMD.Float32x4.extractLane(f4, 0), v);
        assertEq(SIMD.Float32x4.extractLane(f4, 1), NaN);
        assertEq(SIMD.Float32x4.extractLane(f4, 2), Infinity);
        assertEq(SIMD.Float32x4.extractLane(f4, 3), -0);

        assertEq(SIMD.Bool32x4.extractLane(b4, 0), true);
        assertEq(SIMD.Bool32x4.extractLane(b4, 1), true);
        assertEq(SIMD.Bool32x4.extractLane(b4, 2), false);
        assertEq(SIMD.Bool32x4.extractLane(b4, 3), true);

        assertEq(SIMD.Bool32x4.anyTrue(b4), true);
        assertEq(SIMD.Bool32x4.allTrue(b4), false);

        assertEq(SIMD.Bool32x4.anyTrue(bt4), true);
        assertEq(SIMD.Bool32x4.allTrue(bt4), true);
        assertEq(SIMD.Bool32x4.anyTrue(bf4), false);
        assertEq(SIMD.Bool32x4.allTrue(bf4), false);
    }
}

f();
