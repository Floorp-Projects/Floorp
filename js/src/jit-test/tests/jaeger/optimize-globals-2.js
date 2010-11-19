
function testNaN(x) {
    var x = NaN;
    assertEq(isNaN(x), true);
}
testNaN();

function testInfinity(x) {
    return (x === Infinity);
}
assertEq(testInfinity(Infinity), true);
assertEq(testInfinity(6), false);
assertEq(testInfinity(-Infinity), false);

function testUndefined(x) {
    return (x === undefined);
}
assertEq(testUndefined(undefined), true);
assertEq(testUndefined(), true);
assertEq(testUndefined(5), false);
