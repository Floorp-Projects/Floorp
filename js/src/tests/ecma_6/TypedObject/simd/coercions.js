// |reftest| skip-if(!this.hasOwnProperty("SIMD"))
var BUGNUMBER = 1061229;
var float32x4 = SIMD.float32x4;
var int32x4 = SIMD.int32x4;
var {StructType, int32} = TypedObject;
var summary = 'constructors used as coercions';

/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

function assertCaught(f) {
    var caught = false;
    var args = Array.slice(arguments, 1);
    try {
        f.apply(null, args);
    } catch (e) {
        caught = true;
        print(e)
    }
    assertEq(caught, true);
}

function test() {
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

    assertCaught(int32x4, 3);
    assertCaught(int32x4, float32x4(1, 2, 3, 4));
    assertCaught(int32x4, 'pony x 4');

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

    assertCaught(float32x4, 3);
    assertCaught(float32x4, int32x4(1, 2, 3, 4));
    assertCaught(float32x4, 'pony x 4');

    if (typeof reportCompare === "function")
        reportCompare(true, true);
}

test();
