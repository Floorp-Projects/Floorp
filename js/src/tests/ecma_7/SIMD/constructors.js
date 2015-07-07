// |reftest| skip-if(!this.hasOwnProperty("SIMD"))

var float64x2 = SIMD.float64x2;
var float32x4 = SIMD.float32x4;
var int8x16 = SIMD.int8x16;
var int16x8 = SIMD.int16x8;
var int32x4 = SIMD.int32x4;

function TestInt8x16Ctor() {
    // Constructors.
    assertEqX16(int8x16(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16),   [1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16]);
    assertEqX16(int8x16(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15),       [1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,0]);
    assertEqX16(int8x16(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14),           [1,2,3,4,5,6,7,8,9,10,11,12,13,14,0,0]);
    assertEqX16(int8x16(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13),               [1,2,3,4,5,6,7,8,9,10,11,12,13,0,0,0]);
    assertEqX16(int8x16(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12),                   [1,2,3,4,5,6,7,8,9,10,11,12,0,0,0,0]);
    assertEqX16(int8x16(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11),                       [1,2,3,4,5,6,7,8,9,10,11,0,0,0,0,0]);
    assertEqX16(int8x16(1, 2, 3, 4, 5, 6, 7, 8, 9, 10),                           [1,2,3,4,5,6,7,8,9,10,0,0,0,0,0,0]);
    assertEqX16(int8x16(1, 2, 3, 4, 5, 6, 7, 8, 9),                               [1,2,3,4,5,6,7,8,9,0,0,0,0,0,0,0]);
    assertEqX16(int8x16(1, 2, 3, 4, 5, 6, 7, 8),                                  [1,2,3,4,5,6,7,8,0,0,0,0,0,0,0,0]);
    assertEqX16(int8x16(1, 2, 3, 4, 5, 6, 7),                                     [1,2,3,4,5,6,7,0,0,0,0,0,0,0,0,0]);
    assertEqX16(int8x16(1, 2, 3, 4, 5, 6),                                        [1,2,3,4,5,6,0,0,0,0,0,0,0,0,0,0]);
    assertEqX16(int8x16(1, 2, 3, 4, 5),                                           [1,2,3,4,5,0,0,0,0,0,0,0,0,0,0,0]);
    assertEqX16(int8x16(1, 2, 3, 4),                                              [1,2,3,4,0,0,0,0,0,0,0,0,0,0,0,0]);
    assertEqX16(int8x16(1, 2, 3),                                                 [1,2,3,0,0,0,0,0,0,0,0,0,0,0,0,0]);
    assertEqX16(int8x16(1, 2),                                                    [1,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0]);
    assertEqX16(int8x16(1),                                                       [1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0]);
    assertEqX16(int8x16(),                                                        [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0]);
    assertEqX16(int8x16(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17), [1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16]);
    assertEqX16(int8x16(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18), [1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16]);
}

function TestInt16x8Ctor() {
    // Constructors.
    assertEqX8(int16x8(1, 2, 3, 4, 5, 6, 7, 8),        [1,2,3,4,5,6,7,8]);
    assertEqX8(int16x8(1, 2, 3, 4, 5, 6, 7),           [1,2,3,4,5,6,7,0]);
    assertEqX8(int16x8(1, 2, 3, 4, 5, 6),              [1,2,3,4,5,6,0,0]);
    assertEqX8(int16x8(1, 2, 3, 4, 5),                 [1,2,3,4,5,0,0,0]);
    assertEqX8(int16x8(1, 2, 3, 4),                    [1,2,3,4,0,0,0,0]);
    assertEqX8(int16x8(1, 2, 3),                       [1,2,3,0,0,0,0,0]);
    assertEqX8(int16x8(1, 2),                          [1,2,0,0,0,0,0,0]);
    assertEqX8(int16x8(1),                             [1,0,0,0,0,0,0,0]);
    assertEqX8(int16x8(),                              [0,0,0,0,0,0,0,0]);
    assertEqX8(int16x8(1, 2, 3, 4, 5, 6, 7, 8, 9),     [1,2,3,4,5,6,7,8]);
    assertEqX8(int16x8(1, 2, 3, 4, 5, 6, 7, 8, 9, 10), [1,2,3,4,5,6,7,8]);
}

function TestInt32x4Ctor() {
    // Constructors.
    assertEqX4(int32x4(1, 2, 3, 4),         [1,2,3,4]);
    assertEqX4(int32x4(1, 2, 3),            [1,2,3,0]);
    assertEqX4(int32x4(1, 2),               [1,2,0,0]);
    assertEqX4(int32x4(1),                  [1,0,0,0]);
    assertEqX4(int32x4(),                   [0,0,0,0]);
    assertEqX4(int32x4(1, 2, 3, 4, 5),      [1,2,3,4]);
    assertEqX4(int32x4(1, 2, 3, 4, 5, 6),   [1,2,3,4]);
}

function TestFloat32x4Ctor() {
    assertEqX4(float32x4(1, 2, 3, 4),       [1,2,3,4]);
    assertEqX4(float32x4(1, 2, 3),          [1,2,3,NaN]);
    assertEqX4(float32x4(1, 2),             [1,2,NaN,NaN]);
    assertEqX4(float32x4(1),                [1,NaN,NaN,NaN]);
    assertEqX4(float32x4(),                 [NaN,NaN,NaN,NaN]);
    assertEqX4(float32x4(1, 2, 3, 4, 5),    [1,2,3,4]);
    assertEqX4(float32x4(1, 2, 3, 4, 5, 6), [1,2,3,4]);
}

function TestFloat64x2Ctor() {
    assertEqX2(float64x2(1, 2),             [1,2]);
    assertEqX2(float64x2(1),                [1,NaN]);
    assertEqX2(float64x2(),                 [NaN,NaN]);
    assertEqX2(float64x2(1, 2, 3),          [1,2]);
    assertEqX2(float64x2(1, 2, 3, 4),       [1,2]);
    assertEqX2(float64x2(1, 2, 3, 4, 5),    [1,2]);
    assertEqX2(float64x2(1, 2, 3, 4, 5),    [1,2]);
    assertEqX2(float64x2(1, 2, 3, 4, 5, 6), [1,2]);
}

function test() {
    TestFloat32x4Ctor();
    TestFloat64x2Ctor();
    TestInt8x16Ctor();
    TestInt16x8Ctor();
    TestInt32x4Ctor();
    if (typeof reportCompare === "function")
        reportCompare(true, true);
}

test();

