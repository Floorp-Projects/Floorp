load(libdir + 'simd.js');

setJitCompilerOption("ion.warmup.trigger", 50);

function test(i) {
    assertEqX4(SIMD.Int32x4(),              [0, 0, 0, 0]);
    assertEqX4(SIMD.Int32x4(i),             [i, 0, 0, 0]);
    assertEqX4(SIMD.Int32x4(i, 1),          [i, 1, 0, 0]);
    assertEqX4(SIMD.Int32x4(i, 1, 2),       [i, 1, 2, 0]);
    assertEqX4(SIMD.Int32x4(i, 1, 2, 3),    [i, 1, 2, 3]);
    assertEqX4(SIMD.Int32x4(i, 1, 2, 3, 4), [i, 1, 2, 3]);

    assertEqVecArr(SIMD.Int16x8(),              [0, 0, 0, 0, 0, 0, 0, 0]);
    assertEqVecArr(SIMD.Int16x8(i),             [i, 0, 0, 0, 0, 0, 0, 0]);
    assertEqVecArr(SIMD.Int16x8(i, 1),          [i, 1, 0, 0, 0, 0, 0, 0]);
    assertEqVecArr(SIMD.Int16x8(i, 1, 2),       [i, 1, 2, 0, 0, 0, 0, 0]);
    assertEqVecArr(SIMD.Int16x8(i, 1, 2, 3),    [i, 1, 2, 3, 0, 0, 0, 0]);
    assertEqVecArr(SIMD.Int16x8(i, 1, 2, 3, 4), [i, 1, 2, 3, 4, 0, 0, 0]);
    assertEqVecArr(SIMD.Int16x8(i, 1, 2, 3, 4, 5, 6),
                               [i, 1, 2, 3, 4, 5, 6, 0]);
    j = i & 32
    assertEqVecArr(SIMD.Int8x16(),              [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]);
    assertEqVecArr(SIMD.Int8x16(j),             [j, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]);
    assertEqVecArr(SIMD.Int8x16(j, 1),          [j, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]);
    assertEqVecArr(SIMD.Int8x16(j, 1, 2),       [j, 1, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]);
    assertEqVecArr(SIMD.Int8x16(j, 1, 2, 3),    [j, 1, 2, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]);
    assertEqVecArr(SIMD.Int8x16(j, 1, 2, 3, 4), [j, 1, 2, 3, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]);
    assertEqVecArr(SIMD.Int8x16(j, 1, 2, 3, 4, 5, 6),
                               [j, 1, 2, 3, 4, 5, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0]);
    assertEqVecArr(SIMD.Int8x16(j, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12),
                               [j, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 0, 0, 0]);

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

    assertEqVecArr(SIMD.Bool16x8(),
                                [false, false, false, false, false, false, false, false]);
    assertEqVecArr(SIMD.Bool16x8(b),
                                [b,     false, false, false, false, false, false, false]);
    assertEqVecArr(SIMD.Bool16x8(b,     true),
                                [b,     true,  false, false, false, false, false, false]);
    assertEqVecArr(SIMD.Bool16x8(b,     false, true),
                                [b,     false, true,  false, false, false, false, false]);
    assertEqVecArr(SIMD.Bool16x8(b,     false, true,  true),
                                [b,     false, true,  true,  false, false, false, false]);
    assertEqVecArr(SIMD.Bool16x8(b,     false, true,  true,  true),
                                [b,     false, true,  true,  true,  false, false, false]);
    assertEqVecArr(SIMD.Bool16x8(b,     false, true,  true,  true,  true),
                                [b,     false, true,  true,  true,  true,  false, false]);

    assertEqVecArr(SIMD.Bool8x16(),
                                [false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false]);
    assertEqVecArr(SIMD.Bool8x16(b),
                                [b,     false, false, false, false, false, false, false, false, false, false, false, false, false, false, false]);
    assertEqVecArr(SIMD.Bool8x16(b,     true),
                                [b,     true,  false, false, false, false, false, false, false, false, false, false, false, false, false, false]);
    assertEqVecArr(SIMD.Bool8x16(b,     false, true),
                                [b,     false, true,  false, false, false, false, false, false, false, false, false, false, false, false, false]);
    assertEqVecArr(SIMD.Bool8x16(b,     false, true,  true),
                                [b,     false, true,  true,  false, false, false, false, false, false, false, false, false, false, false, false]);
    assertEqVecArr(SIMD.Bool8x16(b,     false, true,  true,  true),
                                [b,     false, true,  true,  true,  false, false, false, false, false, false, false, false, false, false, false]);
    assertEqVecArr(SIMD.Bool8x16(b,     false, true,  true,  true,  true,  false, true,  true,  true),
                                [b,     false, true,  true,  true,  true,  false, true,  true,  true,  false, false, false, false, false, false]);
}

for(var i=0; i<300; i++) {
    test(i);
}
