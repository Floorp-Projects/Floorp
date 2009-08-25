function testFloatArrayIndex() {
    var a = [];
    for (var i = 0; i < 10; ++i) {
        a[3] = 5;
        a[3.5] = 7;
    }
    return a[3] + "," + a[3.5];
}
assertEq(testFloatArrayIndex(), "5,7");
