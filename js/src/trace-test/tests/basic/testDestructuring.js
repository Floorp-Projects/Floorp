function testDestructuring() {
    var t = 0;
    for (var i = 0; i < HOTLOOP + 1; ++i) {
        var [r, g, b] = [1, 1, 1];
        t += r + g + b;
    }
    return t
}
assertEq(testDestructuring(), (HOTLOOP + 1) * 3);
