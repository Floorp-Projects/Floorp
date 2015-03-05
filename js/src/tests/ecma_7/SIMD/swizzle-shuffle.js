// |reftest| skip-if(!this.hasOwnProperty("SIMD"))

/*
 * Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/
 */

var float32x4 = SIMD.float32x4;
var float64x2 = SIMD.float64x2;
var int32x4 = SIMD.int32x4;

function swizzle2(arr, x, y) {
    return [arr[x], arr[y]];
}

function swizzle4(arr, x, y, z, w) {
    return [arr[x], arr[y], arr[z], arr[w]];
}

function getNumberOfLanesFromType(type) {
    switch (type) {
      case float32x4:
      case int32x4:
        return 4;
      case float64x2:
        return 2;
    }
    throw new TypeError("Unknown SIMD type.");
}

function testSwizzleForType(type) {
    var lanes = getNumberOfLanesFromType(type);
    var v = lanes == 4 ? type(1, 2, 3, 4) : type(1, 2);

    assertThrowsInstanceOf(() => type.swizzle()               , TypeError);
    assertThrowsInstanceOf(() => type.swizzle(v, 0)           , TypeError);
    assertThrowsInstanceOf(() => type.swizzle(v, 0, 1, 2)     , TypeError);
    assertThrowsInstanceOf(() => type.swizzle(0, 1, 2, 3, v)  , TypeError);

    // Test all possible swizzles.
    if (lanes == 4) {
        var x, y, z, w;
        for (var i = 0; i < Math.pow(4, 4); i++) {
            [x, y, z, w] = [i & 3, (i >> 2) & 3, (i >> 4) & 3, (i >> 6) & 3];
            assertEqVec(type.swizzle(v, x, y, z, w), swizzle4(simdToArray(v), x, y, z, w));
        }
    } else {
        assertEq(lanes, 2);
        var x, y;
        for (var i = 0; i < Math.pow(2, 2); i++) {
          [x, y] = [x & 1, (y >> 1) & 1];
          assertEqVec(type.swizzle(v, x, y), swizzle2(simdToArray(v), x, y));
        }
    }

    // Test that we throw if an lane argument isn't an int32 or isn't in bounds.
    if (lanes == 4) {
        assertThrowsInstanceOf(() => type.swizzle(v, 0, 0, 0, 0.5), TypeError);
        assertThrowsInstanceOf(() => type.swizzle(v, 0, 0, 0, {}), TypeError);
        assertThrowsInstanceOf(() => type.swizzle(v, 0, 0, 0, {valueOf: function(){return 42}}), TypeError);
        assertThrowsInstanceOf(() => type.swizzle(v, 0, 0, 0, "one"), TypeError);
        assertThrowsInstanceOf(() => type.swizzle(v, 0, 0, 0, null), TypeError);
        assertThrowsInstanceOf(() => type.swizzle(v, 0, 0, 0, undefined), TypeError);
        assertThrowsInstanceOf(() => type.swizzle(v, 0, 0, 0, true), TypeError);

        // In bounds is [0, 3]
        assertThrowsInstanceOf(() => type.swizzle(v, 0, 0, 0, -1), TypeError);
        assertThrowsInstanceOf(() => type.swizzle(v, 0, 0, 0, 4), TypeError);
    } else {
        assertEq(lanes, 2);

        assertThrowsInstanceOf(() => type.swizzle(v, 0, 0.5), TypeError);
        assertThrowsInstanceOf(() => type.swizzle(v, 0, {}), TypeError);
        assertThrowsInstanceOf(() => type.swizzle(v, 0, {valueOf: function(){return 42}}), TypeError);
        assertThrowsInstanceOf(() => type.swizzle(v, 0, "one"), TypeError);
        assertThrowsInstanceOf(() => type.swizzle(v, 0, null), TypeError);
        assertThrowsInstanceOf(() => type.swizzle(v, 0, undefined), TypeError);
        assertThrowsInstanceOf(() => type.swizzle(v, 0, true), TypeError);

        // In bounds is [0, 1]
        assertThrowsInstanceOf(() => type.swizzle(v, 0, -1), TypeError);
        assertThrowsInstanceOf(() => type.swizzle(v, 0, 2), TypeError);
    }
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

function testSwizzleFloat64x2() {
    var v = float64x2(1, 2);

    assertThrowsInstanceOf(function() {
        float32x4.swizzle(v, 0, 0, 0, 0);
    }, TypeError);

    testSwizzleForType(float64x2);
}

function shuffle2(lhsa, rhsa, x, y) {
    return [(x < 2 ? lhsa : rhsa)[x % 2],
            (y < 2 ? lhsa : rhsa)[y % 2]];
}
function shuffle4(lhsa, rhsa, x, y, z, w) {
    return [(x < 4 ? lhsa : rhsa)[x % 4],
            (y < 4 ? lhsa : rhsa)[y % 4],
            (z < 4 ? lhsa : rhsa)[z % 4],
            (w < 4 ? lhsa : rhsa)[w % 4]];
}

function testShuffleForType(type) {
    var lanes = getNumberOfLanesFromType(type);
    var lhs, rhs;
    if (lanes == 4) {
        lhs = type(1, 2, 3, 4);
        rhs = type(5, 6, 7, 8);
    } else {
        assertEq(lanes, 2);
        lhs = type(1, 2);
        rhs = type(3, 4);
    }

    assertThrowsInstanceOf(() => type.shuffle(lhs)                   , TypeError);
    assertThrowsInstanceOf(() => type.shuffle(lhs, rhs)              , TypeError);
    assertThrowsInstanceOf(() => type.shuffle(lhs, rhs, 0)           , TypeError);
    assertThrowsInstanceOf(() => type.shuffle(lhs, rhs, 0, 1, 2)     , TypeError);
    assertThrowsInstanceOf(() => type.shuffle(lhs, 0, 1, 2, 7, rhs)  , TypeError);

    // Test all possible shuffles.
    var x, y, z, w;
    if (lanes == 4) {
        var x, y, z, w;
        for (var i = 0; i < Math.pow(8, 4); i++) {
            [x, y, z, w] = [i & 7, (i >> 3) & 7, (i >> 6) & 7, (i >> 9) & 7];
            assertEqVec(type.shuffle(lhs, rhs, x, y, z, w),
                        shuffle4(simdToArray(lhs), simdToArray(rhs), x, y, z, w));
        }
    } else {
        assertEq(lanes, 2);
        var x, y;
        for (var i = 0; i < Math.pow(4, 2); i++) {
            [x, y] = [i & 3, (i >> 3) & 3];
            assertEqVec(type.shuffle(lhs, rhs, x, y),
                        shuffle2(simdToArray(lhs), simdToArray(rhs), x, y));
        }
    }

    // Test that we throw if an lane argument isn't an int32 or isn't in bounds.
    if (lanes == 4) {
        assertThrowsInstanceOf(() => type.shuffle(lhs, rhs, 0, 0, 0, 0.5), TypeError);
        assertThrowsInstanceOf(() => type.shuffle(lhs, rhs, 0, 0, 0, {}), TypeError);
        assertThrowsInstanceOf(() => type.shuffle(lhs, rhs, 0, 0, 0, {valueOf: function(){return 42}}), TypeError);
        assertThrowsInstanceOf(() => type.shuffle(lhs, rhs, 0, 0, 0, "one"), TypeError);
        assertThrowsInstanceOf(() => type.shuffle(lhs, rhs, 0, 0, 0, null), TypeError);
        assertThrowsInstanceOf(() => type.shuffle(lhs, rhs, 0, 0, 0, undefined), TypeError);
        assertThrowsInstanceOf(() => type.shuffle(lhs, rhs, 0, 0, 0, true), TypeError);

        // In bounds is [0, 7]
        assertThrowsInstanceOf(() => type.shuffle(lhs, rhs, 0, 0, 0, -1), TypeError);
        assertThrowsInstanceOf(() => type.shuffle(lhs, rhs, 0, 0, 0, 8), TypeError);
    } else {
        assertEq(lanes, 2);

        assertThrowsInstanceOf(() => type.shuffle(lhs, rhs, 0, 0.5), TypeError);
        assertThrowsInstanceOf(() => type.shuffle(lhs, rhs, 0, {}), TypeError);
        assertThrowsInstanceOf(() => type.shuffle(lhs, rhs, 0, {valueOf: function(){return 42}}), TypeError);
        assertThrowsInstanceOf(() => type.shuffle(lhs, rhs, 0, "one"), TypeError);
        assertThrowsInstanceOf(() => type.shuffle(lhs, rhs, 0, null), TypeError);
        assertThrowsInstanceOf(() => type.shuffle(lhs, rhs, 0, undefined), TypeError);
        assertThrowsInstanceOf(() => type.shuffle(lhs, rhs, 0, true), TypeError);

        // In bounds is [0, 3]
        assertThrowsInstanceOf(() => type.shuffle(lhs, rhs, 0, -1), TypeError);
        assertThrowsInstanceOf(() => type.shuffle(lhs, rhs, 0, 4), TypeError);
    }
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

function testShuffleFloat64x2() {
    var v = float64x2(1, 2);

    assertThrowsInstanceOf(function() {
        float32x4.shuffle(v, v, 0, 0, 0, 0);
    }, TypeError);

    testShuffleForType(float64x2);
}

testSwizzleInt32x4();
testSwizzleFloat32x4();
testSwizzleFloat64x2();
testShuffleInt32x4();
testShuffleFloat32x4();
testShuffleFloat64x2();

if (typeof reportCompare === "function")
    reportCompare(true, true);
