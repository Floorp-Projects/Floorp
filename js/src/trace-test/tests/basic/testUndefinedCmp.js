function testUndefinedCmp() {
    var a = false;
    for (var j = 0; j < 4; ++j) { if (undefined < false) { a = true; } }
    return a;
}
assertEq(testUndefinedCmp(), false);
