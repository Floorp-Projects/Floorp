function testSubstring() {
    for (var i = 0; i < 5; ++i) {
        actual = "".substring(5);
    }
    return actual;
}
assertEq(testSubstring(), "");
