function testBooleans(a, b) {
    var res = 0;
    if (a && b) res += 2;
    if (b || a) res += 1;
    return res;
}
assertEq(testBooleans(false, false), 0);
assertEq(testBooleans(false, true), 1);
assertEq(testBooleans(true, false), 1);
assertEq(testBooleans(true, true), 3);

function testShortCircuit(a) {
    var b = 0;
    ++a && a++;
    a || (b = 100);
    return a + b;
}
assertEq(testShortCircuit(0), 2);
assertEq(testShortCircuit(-2), 100);
assertEq(testShortCircuit(-1), 100);

function testValues(a, b) {
    if (a && b) return 2;
    if (b || a) return 1;
    return 0;
}
assertEq(testValues(false, true), 1);
assertEq(testValues("foo", 22), 2);
assertEq(testValues(null, ""), 0);
assertEq(testValues(Math.PI, undefined), 1);
assertEq(testValues(Math.abs, 2.2), 2);
