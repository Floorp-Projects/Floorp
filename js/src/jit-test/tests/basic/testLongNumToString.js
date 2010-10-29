function testLongNumToString() {
    var s;
    for (var i = 0; i < 5; i++)
        s = (0x08000000).toString(2);
    return s;
}
assertEq(testLongNumToString(), '1000000000000000000000000000');
