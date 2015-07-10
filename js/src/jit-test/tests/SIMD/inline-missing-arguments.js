load(libdir + 'simd.js');

setJitCompilerOption("ion.warmup.trigger", 50);

function test(i) {
    assertEqX4(SIMD.Int32x4(),              [0, 0, 0, 0]);
    assertEqX4(SIMD.Int32x4(i),             [i, 0, 0, 0]);
    assertEqX4(SIMD.Int32x4(i, 1),          [i, 1, 0, 0]);
    assertEqX4(SIMD.Int32x4(i, 1, 2),       [i, 1, 2, 0]);
    assertEqX4(SIMD.Int32x4(i, 1, 2, 3),    [i, 1, 2, 3]);
    assertEqX4(SIMD.Int32x4(i, 1, 2, 3, 4), [i, 1, 2, 3]);

    assertEqX4(SIMD.Float32x4(),                [NaN, NaN, NaN, NaN]);
    assertEqX4(SIMD.Float32x4(i),               [i,   NaN, NaN, NaN]);
    assertEqX4(SIMD.Float32x4(i, 1),            [i,   1,   NaN, NaN]);
    assertEqX4(SIMD.Float32x4(i, 1, 2),         [i,   1,   2,   NaN]);
    assertEqX4(SIMD.Float32x4(i, 1, 2, 3),      [i,   1,   2,   3  ]);
    assertEqX4(SIMD.Float32x4(i, 1, 2, 3, 4),   [i,   1,   2,   3  ]);
}

for(var i=0; i<300; i++) {
    test(i);
}
