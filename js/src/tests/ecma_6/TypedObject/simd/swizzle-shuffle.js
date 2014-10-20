// |reftest| skip-if(!this.hasOwnProperty("SIMD"))

/*
 * Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/
 */

var float32x4 = SIMD.float32x4;
var int32x4 = SIMD.int32x4;

function assertEq4(v, arr) {
    assertEq(v.x, arr[0]);
    assertEq(v.y, arr[1]);
    assertEq(v.z, arr[2]);
    assertEq(v.w, arr[3]);
}

function simdToArray(v) {
    return [v.x, v.y, v.z, v.w];
}

function swizzle(arr, x, y, z, w) {
    return [arr[x], arr[y], arr[z], arr[w]];
}

function testSwizzleForType(type) {
    var v = type(1,2,3,4);

    assertThrowsInstanceOf(() => type.swizzle()               , TypeError);
    assertThrowsInstanceOf(() => type.swizzle(v, 0)           , TypeError);
    assertThrowsInstanceOf(() => type.swizzle(v, 0, 1)        , TypeError);
    assertThrowsInstanceOf(() => type.swizzle(v, 0, 1, 2)     , TypeError);
    assertThrowsInstanceOf(() => type.swizzle(v, 0, 1, 2, 4)  , TypeError);
    assertThrowsInstanceOf(() => type.swizzle(v, 0, 1, 2, -1) , TypeError);
    assertThrowsInstanceOf(() => type.swizzle(0, 1, 2, 3, v)  , TypeError);

    // Test all possible swizzles.
    var x, y, z, w;
    for (var i = 0; i < Math.pow(4, 4); i++) {
        [x, y, z, w] = [i & 3, (i >> 2) & 3, (i >> 4) & 3, (i >> 6) & 3];
        assertEq4(type.swizzle(v, x, y, z, w), swizzle(simdToArray(v), x, y, z, w));
    }

    // Test that the lane inputs are converted into an int32.
    // n.b, order of evaluation of args is left-to-right.
    var obj = {
        x: 0,
        valueOf: function() { return this.x++ }
    };
    assertEq4(type.swizzle(v, obj, obj, obj, obj), swizzle(simdToArray(v), 0, 1, 2, 3));

    // Object for which ToInt32 will fail.
    obj = {
        valueOf: function() { throw new Error; }
    };
    assertThrowsInstanceOf(() => type.swizzle(v, 0, 1, 2, obj), Error);
}

function testSwizzleInt32x4() {
    var v = int32x4(1, 2, 3, 4);

    assertThrowsInstanceOf(function() {
        float32x4.swizzle(v, 0, 0, 0, 0);
    }, TypeError);

    testSwizzleForType(int32x4);
}

function testSwizzleFloat32x4() {
    var v = float32x4(1, 2, 3, 4);

    assertThrowsInstanceOf(function() {
        int32x4.swizzle(v, 0, 0, 0, 0);
    }, TypeError);

    testSwizzleForType(float32x4);
}

function shuffle(lhsa, rhsa, x, y, z, w) {
    return [(x < 4 ? lhsa : rhsa)[x % 4],
            (y < 4 ? lhsa : rhsa)[y % 4],
            (z < 4 ? lhsa : rhsa)[z % 4],
            (w < 4 ? lhsa : rhsa)[w % 4]];
}

function testShuffleForType(type) {
    var lhs = type(1,2,3,4);
    var rhs = type(5,6,7,8);

    assertThrowsInstanceOf(() => type.shuffle(lhs)                   , TypeError);
    assertThrowsInstanceOf(() => type.shuffle(lhs, rhs)              , TypeError);
    assertThrowsInstanceOf(() => type.shuffle(lhs, rhs, 0)           , TypeError);
    assertThrowsInstanceOf(() => type.shuffle(lhs, rhs, 0, 1)        , TypeError);
    assertThrowsInstanceOf(() => type.shuffle(lhs, rhs, 0, 1, 2)     , TypeError);
    assertThrowsInstanceOf(() => type.shuffle(lhs, rhs, 0, 1, 2, -1) , TypeError);
    assertThrowsInstanceOf(() => type.shuffle(lhs, rhs, 0, 1, 2, 8)  , TypeError);
    assertThrowsInstanceOf(() => type.shuffle(lhs, 0, 1, 2, 7, rhs)  , TypeError);

    // Test all possible shuffles.
    var x, y, z, w;
    for (var i = 0; i < Math.pow(8, 4); i++) {
        [x, y, z, w] = [i & 7, (i >> 3) & 7, (i >> 6) & 7, (i >> 9) & 7];
        assertEq4(type.shuffle(lhs, rhs, x, y, z, w),
                  shuffle(simdToArray(lhs), simdToArray(rhs), x, y, z, w));
    }

    // Test that the lane inputs are converted into an int32.
    // n.b, order of evaluation of args is left-to-right.
    var obj = {
        x: 0,
        valueOf: function() { return this.x++ }
    };
    assertEq4(type.shuffle(lhs, rhs, obj, obj, obj, obj),
              shuffle(simdToArray(lhs),simdToArray(rhs), 0, 1, 2, 3));

    // Object for which ToInt32 will fail.
    obj = {
        valueOf: function() { throw new Error; }
    };
    assertThrowsInstanceOf(() => type.shuffle(lhs, rhs, 0, 1, 2, obj), Error);
}

function testShuffleInt32x4() {
    var v = int32x4(1, 2, 3, 4);

    assertThrowsInstanceOf(function() {
        float32x4.shuffle(v, v, 0, 0, 0, 0);
    }, TypeError);

    testShuffleForType(int32x4);
}

function testShuffleFloat32x4() {
    var v = float32x4(1, 2, 3, 4);

    assertThrowsInstanceOf(function() {
        int32x4.shuffle(v, v, 0, 0, 0, 0);
    }, TypeError);

    testShuffleForType(float32x4);
}

testSwizzleInt32x4();
testSwizzleFloat32x4();
testShuffleInt32x4();
testShuffleFloat32x4();

if (typeof reportCompare === "function")
    reportCompare(true, true);
