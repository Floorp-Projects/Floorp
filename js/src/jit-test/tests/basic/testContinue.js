function testContinue() {
    var i;
    var total = 0;
    for (i = 0; i < 20; ++i) {
        if (i == 11)
            continue;
        total++;
    }
    return total;
}
assertEq(testContinue(), 19);
