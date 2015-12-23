// |reftest| skip-if(!this.hasOwnProperty("SIMD"))

var Float64x2 = SIMD.Float64x2;
var Float32x4 = SIMD.Float32x4;
var Int8x16 = SIMD.Int8x16;
var Int16x8 = SIMD.Int16x8;
var Int32x4 = SIMD.Int32x4;
var Bool8x16 = SIMD.Bool8x16;
var Bool16x8 = SIMD.Bool16x8;
var Bool32x4 = SIMD.Bool32x4;
var Bool64x2 = SIMD.Bool64x2;

function TestInt8x16Ctor() {
    // Constructors.
    assertEqX16(Int8x16(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16),   [1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16]);
    assertEqX16(Int8x16(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15),       [1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,0]);
    assertEqX16(Int8x16(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14),           [1,2,3,4,5,6,7,8,9,10,11,12,13,14,0,0]);
    assertEqX16(Int8x16(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13),               [1,2,3,4,5,6,7,8,9,10,11,12,13,0,0,0]);
    assertEqX16(Int8x16(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12),                   [1,2,3,4,5,6,7,8,9,10,11,12,0,0,0,0]);
    assertEqX16(Int8x16(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11),                       [1,2,3,4,5,6,7,8,9,10,11,0,0,0,0,0]);
    assertEqX16(Int8x16(1, 2, 3, 4, 5, 6, 7, 8, 9, 10),                           [1,2,3,4,5,6,7,8,9,10,0,0,0,0,0,0]);
    assertEqX16(Int8x16(1, 2, 3, 4, 5, 6, 7, 8, 9),                               [1,2,3,4,5,6,7,8,9,0,0,0,0,0,0,0]);
    assertEqX16(Int8x16(1, 2, 3, 4, 5, 6, 7, 8),                                  [1,2,3,4,5,6,7,8,0,0,0,0,0,0,0,0]);
    assertEqX16(Int8x16(1, 2, 3, 4, 5, 6, 7),                                     [1,2,3,4,5,6,7,0,0,0,0,0,0,0,0,0]);
    assertEqX16(Int8x16(1, 2, 3, 4, 5, 6),                                        [1,2,3,4,5,6,0,0,0,0,0,0,0,0,0,0]);
    assertEqX16(Int8x16(1, 2, 3, 4, 5),                                           [1,2,3,4,5,0,0,0,0,0,0,0,0,0,0,0]);
    assertEqX16(Int8x16(1, 2, 3, 4),                                              [1,2,3,4,0,0,0,0,0,0,0,0,0,0,0,0]);
    assertEqX16(Int8x16(1, 2, 3),                                                 [1,2,3,0,0,0,0,0,0,0,0,0,0,0,0,0]);
    assertEqX16(Int8x16(1, 2),                                                    [1,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0]);
    assertEqX16(Int8x16(1),                                                       [1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0]);
    assertEqX16(Int8x16(),                                                        [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0]);
    assertEqX16(Int8x16(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17), [1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16]);
    assertEqX16(Int8x16(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18), [1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16]);
}

function TestInt16x8Ctor() {
    // Constructors.
    assertEqX8(Int16x8(1, 2, 3, 4, 5, 6, 7, 8),        [1,2,3,4,5,6,7,8]);
    assertEqX8(Int16x8(1, 2, 3, 4, 5, 6, 7),           [1,2,3,4,5,6,7,0]);
    assertEqX8(Int16x8(1, 2, 3, 4, 5, 6),              [1,2,3,4,5,6,0,0]);
    assertEqX8(Int16x8(1, 2, 3, 4, 5),                 [1,2,3,4,5,0,0,0]);
    assertEqX8(Int16x8(1, 2, 3, 4),                    [1,2,3,4,0,0,0,0]);
    assertEqX8(Int16x8(1, 2, 3),                       [1,2,3,0,0,0,0,0]);
    assertEqX8(Int16x8(1, 2),                          [1,2,0,0,0,0,0,0]);
    assertEqX8(Int16x8(1),                             [1,0,0,0,0,0,0,0]);
    assertEqX8(Int16x8(),                              [0,0,0,0,0,0,0,0]);
    assertEqX8(Int16x8(1, 2, 3, 4, 5, 6, 7, 8, 9),     [1,2,3,4,5,6,7,8]);
    assertEqX8(Int16x8(1, 2, 3, 4, 5, 6, 7, 8, 9, 10), [1,2,3,4,5,6,7,8]);
}

function TestInt32x4Ctor() {
    // Constructors.
    assertEqX4(Int32x4(1, 2, 3, 4),         [1,2,3,4]);
    assertEqX4(Int32x4(1, 2, 3),            [1,2,3,0]);
    assertEqX4(Int32x4(1, 2),               [1,2,0,0]);
    assertEqX4(Int32x4(1),                  [1,0,0,0]);
    assertEqX4(Int32x4(),                   [0,0,0,0]);
    assertEqX4(Int32x4(1, 2, 3, 4, 5),      [1,2,3,4]);
    assertEqX4(Int32x4(1, 2, 3, 4, 5, 6),   [1,2,3,4]);
}

function TestFloat32x4Ctor() {
    assertEqX4(Float32x4(1, 2, 3, 4),       [1,2,3,4]);
    assertEqX4(Float32x4(1, 2, 3),          [1,2,3,NaN]);
    assertEqX4(Float32x4(1, 2),             [1,2,NaN,NaN]);
    assertEqX4(Float32x4(1),                [1,NaN,NaN,NaN]);
    assertEqX4(Float32x4(),                 [NaN,NaN,NaN,NaN]);
    assertEqX4(Float32x4(1, 2, 3, 4, 5),    [1,2,3,4]);
    assertEqX4(Float32x4(1, 2, 3, 4, 5, 6), [1,2,3,4]);
}

function TestFloat64x2Ctor() {
    assertEqX2(Float64x2(1, 2),             [1,2]);
    assertEqX2(Float64x2(1),                [1,NaN]);
    assertEqX2(Float64x2(),                 [NaN,NaN]);
    assertEqX2(Float64x2(1, 2, 3),          [1,2]);
    assertEqX2(Float64x2(1, 2, 3, 4),       [1,2]);
    assertEqX2(Float64x2(1, 2, 3, 4, 5),    [1,2]);
    assertEqX2(Float64x2(1, 2, 3, 4, 5),    [1,2]);
    assertEqX2(Float64x2(1, 2, 3, 4, 5, 6), [1,2]);
}

function TestBool8x16Ctor() {
    assertEqX16(Bool8x16(false, false, true, true, false, false, true, true, false, false, true, true, false, false, true, true),
                       [false, false, true, true, false, false, true, true, false, false, true, true, false, false, true, true]);
    assertEqX16(Bool8x16(false, true, true, false, false, true, true, false, false, true, true, false, false, true, true),
                       [false, true, true, false, false, true, true, false, false, true, true, false, false, true, true, false]);
    assertEqX16(Bool8x16(true, true, false, false, true, true, false, false, true, true, false, false, true, true),
                       [true, true, false, false, true, true, false, false, true, true, false, false, true, true, false, false]);
    assertEqX16(Bool8x16(true, false, false, true, true, false, false, true, true, false, false, true, true),
                       [true, false, false, true, true, false, false, true, true, false, false, true, true, false, false, false]);
    assertEqX16(Bool8x16(false, false, true, true, false, false, true, true, false, false, true, true),
                       [false, false, true, true, false, false, true, true, false, false, true, true, false, false, false, false]);
    assertEqX16(Bool8x16(false, true, true, false, false, true, true, false, false, true, true),
                       [false, true, true, false, false, true, true, false, false, true, true, false, false, false, false, false]);
    assertEqX16(Bool8x16(true, true, false, false, true, true, false, false, true, true),
                       [true, true, false, false, true, true, false, false, true, true, false, false, false, false, false, false]);
    assertEqX16(Bool8x16(true, false, false, true, true, false, false, true, true),
                       [true, false, false, true, true, false, false, true, true, false, false, false, false, false, false, false]);
    assertEqX16(Bool8x16(false, false, true, true, false, false, true, true),
                       [false, false, true, true, false, false, true, true, false, false, false, false, false, false, false, false]);
    assertEqX16(Bool8x16(false, true, true, false, false, true, true),
                       [false, true, true, false, false, true, true, false, false, false, false, false, false, false, false, false]);
    assertEqX16(Bool8x16(true, true, false, false, true, true),
                       [true, true, false, false, true, true, false, false, false, false, false, false, false, false, false, false]);
    assertEqX16(Bool8x16(true, false, false, true, true),
                       [true, false, false, true, true, false, false, false, false, false, false, false, false, false, false, false]);
    assertEqX16(Bool8x16(false, false, true, true),
                       [false, false, true, true, false, false, false, false, false, false, false, false, false, false, false, false]);
    assertEqX16(Bool8x16(false, true, true),
                       [false, true, true, false, false, false, false, false, false, false, false, false, false, false, false, false]);
    assertEqX16(Bool8x16(true, true),
                       [true, true, false, false, false, false, false, false, false, false, false, false, false, false, false, false]);
    assertEqX16(Bool8x16(true),
                       [true, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false]);
    assertEqX16(Bool8x16(),
                       [false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false]);
    assertEqX16(Bool8x16(false, false, true, true, false, false, true, true, false, false, true, true, false, false, true, true, false),
                       [false, false, true, true, false, false, true, true, false, false, true, true, false, false, true, true]);
    assertEqX16(Bool8x16(false, false, true, true, false, false, true, true, false, false, true, true, false, false, true, true, false, true),
                       [false, false, true, true, false, false, true, true, false, false, true, true, false, false, true, true]);
}

function TestBool16x8Ctor() {
    assertEqX8(Bool16x8(false, false, true, true, false, false, true, true),        [false, false, true, true, false, false, true, true]);
    assertEqX8(Bool16x8(false, true, true, false, false, true, true),               [false, true, true, false, false, true, true, false]);
    assertEqX8(Bool16x8(true, true, false, false, true, true),                      [true, true, false, false, true, true, false, false]);
    assertEqX8(Bool16x8(true, false, false, true, true),                            [true, false, false, true, true, false, false, false]);
    assertEqX8(Bool16x8(false, false, true, true),                                  [false, false, true, true, false, false, false, false]);
    assertEqX8(Bool16x8(false, true, true),                                         [false, true, true, false, false, false, false, false]);
    assertEqX8(Bool16x8(true, true),                                                [true, true, false, false, false, false, false, false]);
    assertEqX8(Bool16x8(true),                                                      [true, false, false, false, false, false, false, false]);
    assertEqX8(Bool16x8(),                                                          [false, false, false, false, false, false, false, false]);
    assertEqX8(Bool16x8(false, false, true, true, false, false, true, true, true),  [false, false, true, true, false, false, true, true]);
    assertEqX8(Bool16x8(false, false, true, true, false, false, true, true, true, true), [false, false, true, true, false, false, true, true]);
}

function TestBool32x4Ctor() {
    assertEqX4(Bool32x4(false, false, true, true),              [false, false, true, true]);
    assertEqX4(Bool32x4(false, false, true),                    [false, false, true, false]);
    assertEqX4(Bool32x4(false, true),                           [false, true, false, false]);
    assertEqX4(Bool32x4(true),                                  [true, false, false, false]);
    assertEqX4(Bool32x4(),                                      [false, false, false, false]);
    assertEqX4(Bool32x4(false, false, true, true, false),       [false, false, true, true]);
    assertEqX4(Bool32x4(false, false, true, true, false, true), [false, false, true, true]);
}

function TestBool64x2Ctor() {
    assertEqX2(Bool64x2(false, true),             [false, true]);
    assertEqX2(Bool64x2(true),                    [true, false]);
    assertEqX2(Bool64x2(),                        [false, false]);
    assertEqX2(Bool64x2(false, true, true),       [false, true]);
    assertEqX2(Bool64x2(false, true, true, true), [false, true]);
}

function test() {
    TestFloat32x4Ctor();
    TestFloat64x2Ctor();
    TestInt8x16Ctor();
    TestInt16x8Ctor();
    TestInt32x4Ctor();
    TestBool8x16Ctor();
    TestBool16x8Ctor();
    TestBool32x4Ctor();
    TestBool64x2Ctor();
    if (typeof reportCompare === "function")
        reportCompare(true, true);
}

test();

