function testBitwise() {
    var x = 10000;
    var y = 123456;
    var z = 987234;
    for (var i = 0; i < 50; i++) {
        x = x ^ y;
        y = y | z;
        z = ~x;
    }
    return x + y + z;
}
assertEq(testBitwise(), -1298);
