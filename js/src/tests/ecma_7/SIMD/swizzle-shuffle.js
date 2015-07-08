// |reftest| skip-if(!this.hasOwnProperty("SIMD"))

/*
 * Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/
 */

var float32x4 = SIMD.float32x4;
var float64x2 = SIMD.float64x2;
var int8x16 = SIMD.int8x16;
var int16x8 = SIMD.int16x8;
var int32x4 = SIMD.int32x4;

function swizzle2(arr, x, y) {
    return [arr[x], arr[y]];
}

function swizzle4(arr, x, y, z, w) {
    return [arr[x], arr[y], arr[z], arr[w]];
}

function swizzle8(arr, s0, s1, s2, s3, s4, s5, s6, s7) {
    return [arr[s0], arr[s1], arr[s2], arr[s3], arr[s4], arr[s5], arr[s6], arr[s7]];
}

function swizzle16(arr, s0, s1, s2, s3, s4, s5, s6, s7,
                   s8, s9, s10, s11, s12, s13, s14, s15) {
    return [arr[s0], arr[s1], arr[s2], arr[s3], arr[s4], arr[s5], arr[s6], arr[s7],
            arr[s8], arr[s9], arr[s10], arr[s11], arr[s12], arr[s13], arr[s14], arr[s15]];
}

function getNumberOfLanesFromType(type) {
    switch (type) {
      case int8x16:
        return 16;
      case int16x8:
        return 8;
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
    var v;
    switch (lanes) {
      case 2:
        v = type(1, 2);
        break;
      case 4:
        v = type(1, 2, 3, 4);
        break;
      case 8:
        v = type(1, 2, 3, 4, 5, 6, 7, 8);
        break;
      case 16:
        v = type(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
        break;
    }

    assertThrowsInstanceOf(() => type.swizzle()               , TypeError);
    assertThrowsInstanceOf(() => type.swizzle(v, 0)           , TypeError);
    assertThrowsInstanceOf(() => type.swizzle(v, 0, 1, 2)     , TypeError);
    assertThrowsInstanceOf(() => type.swizzle(0, 1, 2, 3, v)  , TypeError);

    // Test all possible swizzles.
    if (lanes == 2) {
        var x, y;
        for (var i = 0; i < Math.pow(2, 2); i++) {
          [x, y] = [x & 1, (y >> 1) & 1];
          assertEqVec(type.swizzle(v, x, y), swizzle2(simdToArray(v), x, y));
        }
    } else if (lanes == 4) {
        var x, y, z, w;
        for (var i = 0; i < Math.pow(4, 4); i++) {
            [x, y, z, w] = [i & 3, (i >> 2) & 3, (i >> 4) & 3, (i >> 6) & 3];
            assertEqVec(type.swizzle(v, x, y, z, w), swizzle4(simdToArray(v), x, y, z, w));
        }
    } else if (lanes == 8) {
        var vals = [[1, 2, 1, 2, 1, 2, 1, 2], [1, 1, 1, 1, 1, 1, 1, 1], [0, 1, 2, 3, 4, 5, 6, 7],
                    [7, 6, 5, 4, 3, 2, 1, 0], [5, 3, 2, 6, 1, 7, 4, 0]];
        for (var t of vals) {
          assertEqVec(type.swizzle(v, ...t), swizzle8(simdToArray(v), ...t));
        }
    } else {
        assertEq(lanes, 16);

        var vals = [[11, 2, 11, 2, 11, 2, 11, 2, 11, 2, 11, 2, 11, 2, 11, 2],
                    [1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1],
                    [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15],
                    [15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0],
                    [5, 14, 3, 2, 6, 9, 1, 10, 7, 11, 4, 0, 13, 15, 8, 12]];
        for (var t of vals) {
          assertEqVec(type.swizzle(v, ...t), swizzle16(simdToArray(v), ...t));
        }
    }

    // Test that we throw if an lane argument isn't an int32 or isn't in bounds.
    if (lanes == 2) {
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
    } else if (lanes == 4) {
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
    } else if (lanes == 8) {
        assertThrowsInstanceOf(() => type.swizzle(v, 0, 0, 0, 0, 0, 0, 0, 0.5), TypeError);
        assertThrowsInstanceOf(() => type.swizzle(v, 0, 0, 0, 0, 0, 0, 0, {}), TypeError);
        assertThrowsInstanceOf(() => type.swizzle(v, 0, 0, 0, 0, 0, 0, 0, {valueOf: function(){return 42}}), TypeError);
        assertThrowsInstanceOf(() => type.swizzle(v, 0, 0, 0, 0, 0, 0, 0, "one"), TypeError);
        assertThrowsInstanceOf(() => type.swizzle(v, 0, 0, 0, 0, 0, 0, 0, null), TypeError);
        assertThrowsInstanceOf(() => type.swizzle(v, 0, 0, 0, 0, 0, 0, 0, undefined), TypeError);
        assertThrowsInstanceOf(() => type.swizzle(v, 0, 0, 0, 0, 0, 0, 0, true), TypeError);

        // In bounds is [0, 7]
        assertThrowsInstanceOf(() => type.swizzle(v, 0, 0, 0, 0, 0, 0, 0, -1), TypeError);
        assertThrowsInstanceOf(() => type.swizzle(v, 0, 0, 0, 0, 0, 0, 0, 8), TypeError);
    } else {
        assertEq(lanes, 16);

        assertThrowsInstanceOf(() => type.swizzle(v, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0.5), TypeError);
        assertThrowsInstanceOf(() => type.swizzle(v, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, {}), TypeError);
        assertThrowsInstanceOf(() => type.swizzle(v, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, {valueOf: function(){return 42}}), TypeError);
        assertThrowsInstanceOf(() => type.swizzle(v, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "one"), TypeError);
        assertThrowsInstanceOf(() => type.swizzle(v, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, null), TypeError);
        assertThrowsInstanceOf(() => type.swizzle(v, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, undefined), TypeError);
        assertThrowsInstanceOf(() => type.swizzle(v, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, true), TypeError);

        // In bounds is [0, 15]
        assertThrowsInstanceOf(() => type.swizzle(v, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1), TypeError);
        assertThrowsInstanceOf(() => type.swizzle(v, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 16), TypeError);
    }
}

function testSwizzleInt8x16() {
    var v = int16x8(1, 2, 3, 4, 5, 6, 7, 8);

    assertThrowsInstanceOf(function() {
        int8x16.swizzle(v, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    }, TypeError);

    testSwizzleForType(int8x16);
}

function testSwizzleInt16x8() {
    var v = int8x16(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);

    assertThrowsInstanceOf(function() {
        int16x8.swizzle(v, 0, 0, 0, 0, 0, 0, 0, 0);
    }, TypeError);

    testSwizzleForType(int16x8);
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

function shuffle8(lhsa, rhsa, s0, s1, s2, s3, s4, s5, s6, s7, s8) {
    return [(s0 < 8 ? lhsa : rhsa)[s0 % 8],
            (s1 < 8 ? lhsa : rhsa)[s1 % 8],
            (s2 < 8 ? lhsa : rhsa)[s2 % 8],
            (s3 < 8 ? lhsa : rhsa)[s3 % 8],
            (s4 < 8 ? lhsa : rhsa)[s4 % 8],
            (s5 < 8 ? lhsa : rhsa)[s5 % 8],
            (s6 < 8 ? lhsa : rhsa)[s6 % 8],
            (s7 < 8 ? lhsa : rhsa)[s7 % 8]];
}

function shuffle16(lhsa, rhsa, s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, s11, s12, s13, s14, s15) {
    return [(s0 < 16 ? lhsa : rhsa)[s0 % 16],
            (s1 < 16 ? lhsa : rhsa)[s1 % 16],
            (s2 < 16 ? lhsa : rhsa)[s2 % 16],
            (s3 < 16 ? lhsa : rhsa)[s3 % 16],
            (s4 < 16 ? lhsa : rhsa)[s4 % 16],
            (s5 < 16 ? lhsa : rhsa)[s5 % 16],
            (s6 < 16 ? lhsa : rhsa)[s6 % 16],
            (s7 < 16 ? lhsa : rhsa)[s7 % 16],
            (s8 < 16 ? lhsa : rhsa)[s8 % 16],
            (s9 < 16 ? lhsa : rhsa)[s9 % 16],
            (s10 < 16 ? lhsa : rhsa)[s10 % 16],
            (s11 < 16 ? lhsa : rhsa)[s11 % 16],
            (s12 < 16 ? lhsa : rhsa)[s12 % 16],
            (s13 < 16 ? lhsa : rhsa)[s13 % 16],
            (s14 < 16 ? lhsa : rhsa)[s14 % 16],
            (s15 < 16 ? lhsa : rhsa)[s15 % 16]];
}

function testShuffleForType(type) {
    var lanes = getNumberOfLanesFromType(type);
    var lhs, rhs;
    if (lanes == 2) {
        lhs = type(1, 2);
        rhs = type(3, 4);
    } else if (lanes == 4) {
        lhs = type(1, 2, 3, 4);
        rhs = type(5, 6, 7, 8);
    } else if (lanes == 8) {
        lhs = type(1, 2, 3, 4, 5, 6, 7, 8);
        rhs = type(9, 10, 11, 12, 13, 14, 15, 16);
    } else {
        assertEq(lanes, 16);

        lhs = type(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
        rhs = type(17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32);
    }

    assertThrowsInstanceOf(() => type.shuffle(lhs)                   , TypeError);
    assertThrowsInstanceOf(() => type.shuffle(lhs, rhs)              , TypeError);
    assertThrowsInstanceOf(() => type.shuffle(lhs, rhs, 0)           , TypeError);
    assertThrowsInstanceOf(() => type.shuffle(lhs, rhs, 0, 1, 2)     , TypeError);
    assertThrowsInstanceOf(() => type.shuffle(lhs, 0, 1, 2, 7, rhs)  , TypeError);

    // Test all possible shuffles.
    var x, y, z, w;
    if (lanes == 2) {
        var x, y;
        for (var i = 0; i < Math.pow(4, 2); i++) {
            [x, y] = [i & 3, (i >> 3) & 3];
            assertEqVec(type.shuffle(lhs, rhs, x, y),
                        shuffle2(simdToArray(lhs), simdToArray(rhs), x, y));
        }
    } else if (lanes == 4) {
        var x, y, z, w;
        for (var i = 0; i < Math.pow(8, 4); i++) {
            [x, y, z, w] = [i & 7, (i >> 3) & 7, (i >> 6) & 7, (i >> 9) & 7];
            assertEqVec(type.shuffle(lhs, rhs, x, y, z, w),
                        shuffle4(simdToArray(lhs), simdToArray(rhs), x, y, z, w));
        }
    } else if (lanes == 8) {
        var s0, s1, s2, s3, s4, s5, s6, s7;
        var vals = [[15, 8, 15, 8, 15, 8, 15, 8], [9, 7, 9, 7, 9, 7, 9, 7],
                    [7, 3, 8, 9, 2, 15, 14, 6], [2, 2, 2, 2, 2, 2, 2, 2],
                    [8, 8, 8, 8, 8, 8, 8, 8], [11, 11, 11, 11, 11, 11, 11, 11]];
        for (var t of vals) {
            [s0, s1, s2, s3, s4, s5, s6, s7] = t;
            assertEqVec(type.shuffle(lhs, rhs, s0, s1, s2, s3, s4, s5, s6, s7),
                        shuffle8(simdToArray(lhs), simdToArray(rhs), s0, s1, s2, s3, s4, s5, s6, s7));
        }
    } else {
        assertEq(lanes, 16);

        var s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, s11, s12, s13, s14, s15;
        var vals = [[30, 16, 30, 16, 30, 16, 30, 16, 30, 16, 30, 16, 30, 16, 30, 16],
                    [19, 17, 19, 17, 19, 17, 19, 17, 19, 17, 19, 17, 19, 17, 19, 17],
                    [7, 3, 8, 18, 9, 21, 2, 15, 14, 6, 16, 22, 29, 31, 30, 1],
                    [2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2],
                    [16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16],
                    [21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21]];
        for (var t of vals) {
            [s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, s11, s12, s13, s14, s15] = t;
            assertEqVec(type.shuffle(lhs, rhs, s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, s11, s12, s13, s14, s15),
                        shuffle16(simdToArray(lhs), simdToArray(rhs), s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, s11, s12, s13, s14, s15));
        }
    }

    // Test that we throw if an lane argument isn't an int32 or isn't in bounds.
    if (lanes == 2) {
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
    } else if (lanes == 4) {
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
    } else if (lanes == 8) {
        assertThrowsInstanceOf(() => type.swizzle(lhs, rhs, 0, 0, 0, 0, 0, 0, 0, 0.5), TypeError);
        assertThrowsInstanceOf(() => type.swizzle(lhs, rhs, 0, 0, 0, 0, 0, 0, 0, {}), TypeError);
        assertThrowsInstanceOf(() => type.swizzle(lhs, rhs, 0, 0, 0, 0, 0, 0, 0, {valueOf: function(){return 42}}), TypeError);
        assertThrowsInstanceOf(() => type.swizzle(lhs, rhs, 0, 0, 0, 0, 0, 0, 0, "one"), TypeError);
        assertThrowsInstanceOf(() => type.swizzle(lhs, rhs, 0, 0, 0, 0, 0, 0, 0, null), TypeError);
        assertThrowsInstanceOf(() => type.swizzle(lhs, rhs, 0, 0, 0, 0, 0, 0, 0, undefined), TypeError);
        assertThrowsInstanceOf(() => type.swizzle(lhs, rhs, 0, 0, 0, 0, 0, 0, 0, true), TypeError);

        // In bounds is [0, 15]
        assertThrowsInstanceOf(() => type.swizzle(lhs, rhs, 0, 0, 0, 0, 0, 0, 0, -1), TypeError);
        assertThrowsInstanceOf(() => type.swizzle(lhs, rhs, 0, 0, 0, 0, 0, 0, 0, 16), TypeError);
    } else {
        assertEq(lanes, 16);

        assertThrowsInstanceOf(() => type.swizzle(lhs, rhs, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0.5), TypeError);
        assertThrowsInstanceOf(() => type.swizzle(lhs, rhs, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, {}), TypeError);
        assertThrowsInstanceOf(() => type.swizzle(lhs, rhs, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, {valueOf: function(){return 42}}), TypeError);
        assertThrowsInstanceOf(() => type.swizzle(lhs, rhs, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "one"), TypeError);
        assertThrowsInstanceOf(() => type.swizzle(lhs, rhs, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, null), TypeError);
        assertThrowsInstanceOf(() => type.swizzle(lhs, rhs, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, undefined), TypeError);
        assertThrowsInstanceOf(() => type.swizzle(lhs, rhs, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, true), TypeError);

        // In bounds is [0, 31]
        assertThrowsInstanceOf(() => type.swizzle(lhs, rhs, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1), TypeError);
        assertThrowsInstanceOf(() => type.swizzle(lhs, rhs, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 32), TypeError);
    }
}

function testShuffleInt8x16() {
    var v = int16x8(1, 2, 3, 4, 5, 6, 7, 8);

    assertThrowsInstanceOf(function() {
        int8x16.shuffle(v, v, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    }, TypeError);

    testShuffleForType(int8x16);
}

function testShuffleInt16x8() {
    var v = int8x16(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);

    assertThrowsInstanceOf(function() {
        int16x8.shuffle(v, v, 0, 0, 0, 0, 0, 0, 0, 0);
    }, TypeError);

    testShuffleForType(int16x8);
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

testSwizzleInt8x16();
testSwizzleInt16x8();
testSwizzleInt32x4();
testSwizzleFloat32x4();
testSwizzleFloat64x2();
testShuffleInt8x16();
testShuffleInt16x8();
testShuffleInt32x4();
testShuffleFloat32x4();
testShuffleFloat64x2();

if (typeof reportCompare === "function")
    reportCompare(true, true);
