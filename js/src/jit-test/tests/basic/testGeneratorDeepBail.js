function testGeneratorDeepBail() {
    function* g() { yield 2; }
    var iterables = [[1], [], [], [], g()];

    var total = 0;
    for (let i = 0; i < iterables.length; i++)
        for (let j of iterables[i])
            total += j;
    return total;
}
assertEq(testGeneratorDeepBail(), 3);
