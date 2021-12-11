/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

// Some tests regarding conversion to Float32
assertEq(Math.fround(), NaN);

// Special values
assertEq(Math.fround(NaN), NaN);
assertEq(Math.fround(-Infinity), -Infinity);
assertEq(Math.fround(Infinity), Infinity);
assertEq(Math.fround(-0), -0);
assertEq(Math.fround(+0), +0);

// Polyfill function for Float32 conversion
var toFloat32 = (function() {
    var f32 = new Float32Array(1);
    function f(x) {
        f32[0] = x;
        return f32[0];
    }
    return f;
})();

// A test on a certain range of numbers, including big numbers, so that
// we get a loss in precision for some of them.
for (var i = 0; i < 64; ++i) {
    var p = Math.pow(2, i) + 1;
    assertEq(Math.fround(p), toFloat32(p));
    assertEq(Math.fround(-p), toFloat32(-p));
}

/********************************************
/* Tests on maximal Float32 / Double values *
/*******************************************/
function maxValue(exponentWidth, significandWidth) {
    var n = 0;
    var maxExp = Math.pow(2, exponentWidth - 1) - 1;
    for (var i = significandWidth; i >= 0; i--)
        n += Math.pow(2, maxExp - i);
    return n;
}

var DBL_MAX = maxValue(11, 52);
assertEq(DBL_MAX, Number.MAX_VALUE); // sanity check

// Finite as a double, too big for a float
assertEq(Math.fround(DBL_MAX), Infinity);

var FLT_MAX = maxValue(8, 23);
assertEq(Math.fround(FLT_MAX), FLT_MAX);
assertEq(Math.fround(FLT_MAX + Math.pow(2, Math.pow(2, 8 - 1) - 1 - 23 - 2)), FLT_MAX); // round-nearest rounds down to FLT_MAX
assertEq(Math.fround(FLT_MAX + Math.pow(2, Math.pow(2, 8 - 1) - 1 - 23 - 1)), Infinity); // no longer rounds down to FLT_MAX

/*********************************************************
/******* Tests on denormalizations and roundings *********
/********************************************************/

function minValue(exponentWidth, significandWidth) {
    return Math.pow(2, -(Math.pow(2, exponentWidth - 1) - 2) - significandWidth);
}

var DBL_MIN = Math.pow(2, -1074);
assertEq(DBL_MIN, Number.MIN_VALUE); // sanity check

// Too small for a float
assertEq(Math.fround(DBL_MIN), 0);

var FLT_MIN = minValue(8, 23);
assertEq(Math.fround(FLT_MIN), FLT_MIN);

assertEq(Math.fround(FLT_MIN / 2), 0); // halfway, round-nearest rounds down to 0 (even)
assertEq(Math.fround(FLT_MIN / 2 + Math.pow(2, -202)), FLT_MIN); // first double > FLT_MIN / 2, rounds up to FLT_MIN

assertEq(Math.fround(-FLT_MIN), -FLT_MIN);

assertEq(Math.fround(-FLT_MIN / 2), -0); // halfway, round-nearest rounds up to -0 (even)
assertEq(Math.fround(-FLT_MIN / 2 - Math.pow(2, -202)), -FLT_MIN); // first double < -FLT_MIN / 2, rounds down to -FLT_MIN

reportCompare(0, 0, "ok");
