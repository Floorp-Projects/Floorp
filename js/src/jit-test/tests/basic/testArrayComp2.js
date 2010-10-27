load(libdir + 'range.js');

function testArrayComp2() {
    return [a + b for (a in range(0, 5)) for (b in range(0, 5))].join('');
}

assertEq(testArrayComp2(), '0123412345234563456745678');
