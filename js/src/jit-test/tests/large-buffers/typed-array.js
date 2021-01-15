var gb = 1 * 1024 * 1024 * 1024;

var ta = new Uint8Array(4 * gb + 10);

function testSetFromTyped() {
    var ta2 = new Int8Array(10);
    ta2[0] = 23;
    ta2[9] = -10;
    ta.set(ta2, 4 * gb);
    assertEq(ta[4 * gb + 0], 23);
    assertEq(ta[4 * gb + 9], 246);
}
testSetFromTyped();

function testSetFromOther() {
    ta.set([12, 34], 4 * gb + 4);
    assertEq(ta[4 * gb + 4], 12);
    assertEq(ta[4 * gb + 5], 34);
}
testSetFromOther();
