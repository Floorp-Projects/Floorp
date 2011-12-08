function testIntOverflow() {
    // int32_max - 7
    var ival = 2147483647 - 7;
    for (var i = 0; i < 30; i++) {
        ival += 30;
    }
    return (ival < 2147483647);
}
assertEq(testIntOverflow(), false);
