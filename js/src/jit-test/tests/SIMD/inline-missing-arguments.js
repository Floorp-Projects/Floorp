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

    var b = i % 2 > 0 ;
    assertEqX4(SIMD.Bool32x4(),                           [false, false, false, false]);
    assertEqX4(SIMD.Bool32x4(b),                          [b,     false, false, false]);
    assertEqX4(SIMD.Bool32x4(b, true),                    [b,     true,  false, false]);
    assertEqX4(SIMD.Bool32x4(b, false, true),             [b,     false, true,  false]);
    assertEqX4(SIMD.Bool32x4(b, false, true, true),       [b,     false, true,  true ]);
    assertEqX4(SIMD.Bool32x4(b, false, true, true, true), [b,     false, true,  true ]);
}

for(var i=0; i<300; i++) {
    test(i);
}
