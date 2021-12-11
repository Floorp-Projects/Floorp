
function test(a) {
    return (a)?a:0;
}
function test2(a) {
    return (a)?0:a;
}

function isNegativeZero(x) {
    return x===0 && (1/x)===-Infinity;
}

test(0)
assertEq(isNegativeZero(test(-0)), false)
assertEq(isNegativeZero(test(-0)), false)
assertEq(isNegativeZero(test2(-0)), true)
assertEq(isNegativeZero(test2(-0)), true)
