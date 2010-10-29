load(libdir + 'range.js');

function testArrayComp1() {
    return [a for (a in range(0, 10))].join('');
}
assertEq(testArrayComp1(), '0123456789');
