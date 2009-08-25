function testModuloWithNegative1() {
    var v = 0;
    for (var i = 0; i < 2; ++i) {
        c = v;
        v -= 1;
        for (var j = 0; j < 2; ++j)
            c %= -1;
    }
    return 1/c;
}
assertEq(testModuloWithNegative1(), -Infinity);
