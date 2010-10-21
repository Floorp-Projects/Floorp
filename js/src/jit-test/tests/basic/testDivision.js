function testDivision() {
    var a = 32768;
    var b;
    while (b !== 1) {
        b = a / 2;
        a = b;
    }
    return a;
}
assertEq(testDivision(), 1);
