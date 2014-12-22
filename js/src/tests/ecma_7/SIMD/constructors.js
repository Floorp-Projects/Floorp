// |reftest| skip-if(!this.hasOwnProperty("SIMD"))

var float32x4 = SIMD.float32x4;
var int32x4 = SIMD.int32x4;

function test() {

    // Constructors.
    assertEqX4(int32x4(1, 2, 3, 4),         [1,2,3,4]);
    assertEqX4(int32x4(1, 2, 3),            [1,2,3,0]);
    assertEqX4(int32x4(1, 2),               [1,2,0,0]);
    // The 1-argument form is reserved for coercions.
    assertEqX4(int32x4(),                   [0,0,0,0]);
    assertEqX4(int32x4(1, 2, 3, 4, 5),      [1,2,3,4]);
    assertEqX4(int32x4(1, 2, 3, 4, 5, 6),   [1,2,3,4]);

    assertEqX4(float32x4(1, 2, 3, 4),       [1,2,3,4]);
    assertEqX4(float32x4(1, 2, 3),          [1,2,3,NaN]);
    assertEqX4(float32x4(1, 2),             [1,2,NaN,NaN]);
    // The 1-argument form is reserved for coercions.
    assertEqX4(float32x4(),                 [NaN,NaN,NaN,NaN]);
    assertEqX4(float32x4(1, 2, 3, 4, 5),    [1,2,3,4]);
    assertEqX4(float32x4(1, 2, 3, 4, 5, 6), [1,2,3,4]);

    // Constructors used as coercion.
    var x = int32x4(1, 2, 3, 4);
    var y = int32x4(x);

    assertEq(x, y);

    assertEq(y.x, x.x);
    assertEq(y.x, 1);
    assertEq(y.y, x.y);
    assertEq(y.y, 2);
    assertEq(y.z, x.z);
    assertEq(y.z, 3);
    assertEq(y.w, x.w);
    assertEq(y.w, 4);

    assertThrowsInstanceOf(() => int32x4(3), TypeError);
    assertThrowsInstanceOf(() => int32x4(float32x4(1,2,3,4)), TypeError);
    assertThrowsInstanceOf(() => int32x4('pony x 4'), TypeError);

    var x = float32x4(NaN, 13.37, -Infinity, 4);
    var y = float32x4(x);

    assertEq(x, y);

    assertEq(y.x, x.x);
    assertEq(y.x, Math.fround(NaN));
    assertEq(y.y, x.y);
    assertEq(y.y, Math.fround(13.37));
    assertEq(y.z, x.z);
    assertEq(y.z, Math.fround(-Infinity));
    assertEq(y.w, x.w);
    assertEq(y.w, Math.fround(4));

    assertThrowsInstanceOf(() => float32x4(3), TypeError);
    assertThrowsInstanceOf(() => float32x4(int32x4(1,2,3,4)), TypeError);
    assertThrowsInstanceOf(() => float32x4('pony x 4'), TypeError);

    if (typeof reportCompare === "function")
        reportCompare(true, true);
}

test();
