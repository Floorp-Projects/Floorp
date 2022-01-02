// This test case is made to be run with --ion-eager.

function ret2() {
    return 2;
}

// Check with default case in the middle.
function test0(x) {
    var res = 0;
    switch (x) {
    case (res |= 0x40, 1): // x === 1
        res |= 0x1;
    default: // otherwise
        res |= 0x2;
    case (res |= 0x80, ret2()): // x === 2
        res |= 0x4;
    case (res |= 0x100, 1 + ret2()): // x === 3
        res |= 0x8;
        break;
    case (res |= 0x200, 0): // x === 0
        res |= 0x10;
    }
    res |= 0x20;
    return res;
}

assertEq(test0(0), 0x40 | 0x80 | 0x100 | 0x200 | 0x10 | 0x20);
assertEq(test0(1), 0x40 | 0x1 | 0x2 | 0x4 | 0x8 | 0x20);
assertEq(test0(2), 0x40 | 0x80 | 0x4 | 0x8 | 0x20);
assertEq(test0(3), 0x40 | 0x80 | 0x100 | 0x8 | 0x20);
assertEq(test0(4), 0x40 | 0x80 | 0x100 | 0x200 | 0x2 | 0x4 | 0x8 | 0x20);

// Check with no default and only one case.
function test1(x) {
    var res = 0;
    switch (x) {
    case (res |= 0x1, ret2()): // x === 2
        res |= 0x2;
    }
    res |= 0x4;
    return res;
}

assertEq(test1(1), 0x1 | 0x4); // default.
assertEq(test1(2), 0x1 | 0x2 | 0x4); // case.

// Check with default case identical to a case.
function test2(x) {
    var res = 0;
    switch (x) {
    case (res |= 0x1, 0):
        res |= 0x10;
        break;
    default:
    case (res |= 0x2, 1):
        res |= 0x20;
        break;
    case (res |= 0x4, ret2()): // x === 2
        res |= 0x40;
    }
    res |= 0x100;
    return res;
}

assertEq(test2(0), 0x1 | 0x10 | 0x100);
assertEq(test2(1), 0x1 | 0x2 | 0x20 | 0x100);
assertEq(test2(2), 0x1 | 0x2 | 0x4 | 0x40 | 0x100);
assertEq(test2(3), 0x1 | 0x2 | 0x4 | 0x20 | 0x100);

// Check a non-break, non-empty default at the end.
function test3(x) {
    var res = 0;
    switch (x) {
    case (res |= 0x1, 1):
        res |= 0x10;
    case (res |= 0x2, ret2()): // x === 2
        res |= 0x20;
    default:
        res |= 0x40;
    }
    res |= 0x100;
    return res;
}

assertEq(test3(1), 0x1 | 0x10 | 0x20 | 0x40 | 0x100);
assertEq(test3(2), 0x1 | 0x2 | 0x20 | 0x40 | 0x100);
assertEq(test3(3), 0x1 | 0x2 | 0x40 | 0x100);

// Check cfg in condition of non-last case with no break. (reverse post order failure ?)
function test4(x) {
    var res = 0;
    switch (x) {
    case (res |= 0x1, (x ? 1 : 0)):
        res |= 0x10;
    case (res |= 0x2, ret2()): // x === 2
        res |= 0x20;
    }
    res |= 0x100;
    return res;
}

assertEq(test4(0), 0x1 | 0x10 | 0x20 | 0x100);
assertEq(test4(1), 0x1 | 0x10 | 0x20 | 0x100);
assertEq(test4(2), 0x1 | 0x2 | 0x20 | 0x100);
