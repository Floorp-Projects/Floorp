// |reftest| skip-if(!this.hasOwnProperty("SIMD"))

var float64x2 = SIMD.float64x2;
var float32x4 = SIMD.float32x4;
var int32x4 = SIMD.int32x4;

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
    TestInt32x4Ctor();
    TestFloat64x2Ctor();
    if (typeof reportCompare === "function")
        reportCompare(true, true);
}

test();

