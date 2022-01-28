// Pass |null| as argument to be more robust against code folding.
function testNullWithInt32OrBool(nullVal) {
    var vals = [0, 1, true, false, null];
    for (var v of vals) {
        assertEq(v + nullVal, Number(v));
        assertEq(v - nullVal, Number(v));
        assertEq(v * nullVal, 0);
        var res = v / nullVal;
        assertEq(isNaN(res) || res === Infinity, true);
        assertEq(v % nullVal, NaN);
        assertEq(v ** nullVal, 1);

        assertEq(nullVal + v, Number(v));
        assertEq(nullVal - v, 0 - Number(v));
        assertEq(nullVal * v, 0);
        res = nullVal / v;
        assertEq(isNaN(res) || res === 0, true);
        res = nullVal % v;
        assertEq(isNaN(res) || res === 0, true);
        res = nullVal ** v;
        assertEq(res === 0 || res === 1, true);
    }
}
for (var i = 0; i < 15; i++) {
    testNullWithInt32OrBool(null);
}

function testUndefinedWithOther(undefinedVal) {
    var vals = [1.1, NaN, true, false, null, undefined];
    for (var v of vals) {
        assertEq(v + undefinedVal, NaN);
        assertEq(v - undefinedVal, NaN);
        assertEq(v * undefinedVal, NaN);
        assertEq(v / undefinedVal, NaN);
        assertEq(v % undefinedVal, NaN);
        assertEq(v ** undefinedVal, NaN);

        assertEq(undefinedVal + v, NaN);
        assertEq(undefinedVal - v, NaN);
        assertEq(undefinedVal * v, NaN);
        assertEq(undefinedVal / v, NaN);
        assertEq(undefinedVal % v, NaN);
        var res = undefinedVal ** v;
        if (v === false || v === null) {
            assertEq(res, 1);
        } else {
            assertEq(res, NaN);
        }
    }
}
for (var i = 0; i < 15; i++) {
    testUndefinedWithOther(undefined);
}

function testBooleanWithDouble(trueVal, falseVal) {
    var vals = [1.1, 2.2, 5, 6, 3.14];
    for (var v of vals) {
        assertEq(v + falseVal, v);
        assertEq(v - falseVal, v);
        assertEq(v * falseVal, 0);
        assertEq(v / falseVal, Infinity);
        assertEq(v % falseVal, NaN);
        assertEq(v ** falseVal, 1);

        assertEq(trueVal + v, v + 1);
        assertEq(trueVal - v, 1 - v);
        assertEq(trueVal * v, v);
        assertEq(trueVal / v, 1 / v);
        assertEq(trueVal % v, 1);
        assertEq(trueVal ** v, 1);
    }
}
for (var i = 0; i < 15; i++) {
    testBooleanWithDouble(true, false);
}
