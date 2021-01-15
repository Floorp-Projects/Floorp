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

function testCopyWithin() {
    // Large |start|.
    ta[ta.length - 1] = 3;
    ta.copyWithin(0, 4 * gb);
    assertEq(ta[9], 3);

    // Large relative |start|.
    ta[ta.length - 1] = 4;
    ta.copyWithin(0, -10);
    assertEq(ta[9], 4);

    // Large |start| and |end|.
    ta[ta.length - 3] = 5;
    ta[ta.length - 2] = 66;
    ta[1] = 1;
    ta.copyWithin(0, ta.length - 3, ta.length - 2);
    assertEq(ta[0], 5);
    assertEq(ta[1], 1);

    // Large |target| and |start|.
    ta.copyWithin(4 * gb + 5, 4 * gb + 7);
    assertEq(ta[4 * gb + 6], 66);

    // Large |target|.
    ta[6] = 117;
    ta.copyWithin(4 * gb);
    assertEq(ta[4 * gb + 6], 117);
}
testCopyWithin();
