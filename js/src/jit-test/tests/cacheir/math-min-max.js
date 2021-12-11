function test1ArgInt32() {
    function test(x, expected) {
        assertEq(Math.max(x), expected);
        assertEq(Math.min(x), expected);
    }
    for (var i = 0; i < 20; i++) {
        test(i, i);
    }
    // Fail Int32 guard.
    test(true, 1);
    test({}, NaN);
}
test1ArgInt32();

function test1ArgNumber() {
    function test(x, expected) {
        assertEq(Math.max(x), expected);
        assertEq(Math.min(x), expected);
    }
    for (var i = 0; i < 20; i++) {
        test(3.14, 3.14);
        test(-0, -0);
        test(i, i);
    }
    // Fail Number guard.
    test(true, 1);
    test({}, NaN);
}
test1ArgNumber();

function test1ArgInt32ThenNumber() {
    function test(x, expected) {
        assertEq(Math.max(x), expected);
        assertEq(Math.min(x), expected);
    }
    for (var i = 0; i < 20; i++) {
        test(i, i);
    }
    for (var i = 0; i < 10; i++) {
        test(i * 3.14, i * 3.14);
    }
}
test1ArgInt32ThenNumber();

function test2ArgsInt32() {
    function test(x, y, expectedMax, expectedMin) {
        assertEq(Math.max(x, y), expectedMax);
        assertEq(Math.min(x, y), expectedMin);
    }
    for (var i = 0; i < 20; i++) {
        test(0, i, i, 0);
        test(-9, -1, -1, -9);
        test(0, 0, 0, 0);
    }
    // Fail Int32 guard.
    test(0, "3", 3, 0);
    test({}, 2, NaN, NaN);
}
test2ArgsInt32();

function test2ArgsNumber() {
    function test(x, y, expectedMax, expectedMin) {
        assertEq(Math.max(x, y), expectedMax);
        assertEq(Math.min(x, y), expectedMin);
    }
    for (var i = 0; i < 20; i++) {
        test(1.1, 2.2, 2.2, 1.1);
        test(8, NaN, NaN, NaN);
        test(NaN, 8, NaN, NaN);
        test(-0, i, i, -0);
        test(Infinity, -0, Infinity, -0);
        test(-Infinity, Infinity, Infinity, -Infinity);
    }
    // Fail Number guard.
    test(-0, "3", 3, -0);
    test({}, 2.1, NaN, NaN);
}
test2ArgsNumber();

function test2ArgsInt32ThenNumber() {
    function test(x, y, expectedMax, expectedMin) {
        assertEq(Math.max(x, y), expectedMax);
        assertEq(Math.min(x, y), expectedMin);
    }
    for (var i = 0; i < 20; i++) {
        test(-1, i, i, -1);
    }
    for (var i = 0; i < 10; i++) {
        test(-0, i, i, -0);
    }
}
test2ArgsInt32ThenNumber();

function test3ArgsInt32() {
    function test(a, b, c, expectedMax, expectedMin) {
        assertEq(Math.max(a, b, c), expectedMax);
        assertEq(Math.min(a, b, c), expectedMin);
    }
    for (var i = 0; i < 20; i++) {
        test(30, 100, i, 100, i);
        test(i, 0, -2, i, -2);
    }
    // Fail Int32 guard.
    test(0, 1, "2", 2, 0);
    test(-0, 1, 2, 2, -0);
}
test3ArgsInt32();

function test3ArgsNumber() {
    function test(a, b, c, expectedMax, expectedMin) {
        assertEq(Math.max(a, b, c), expectedMax);
        assertEq(Math.min(a, b, c), expectedMin);
    }
    for (var i = 0; i < 20; i++) {
        test(100, i, -0, 100, -0);
        test(i, NaN, -1, NaN, NaN);
    }
    // Fail Number guard.
    test(-0, "3", 1, 3, -0);
    test("9", 1.1, 3, 9, 1.1);
}
test3ArgsNumber();

function test3ArgsInt32ThenNumber() {
    function test(a, b, c, expectedMax, expectedMin) {
        assertEq(Math.max(a, b, c), expectedMax);
        assertEq(Math.min(a, b, c), expectedMin);
    }
    for (var i = 0; i < 20; i++) {
        test(30, 100, i, 100, i);
    }
    for (var i = 0; i < 10; i++) {
        test(123.4, 100, i, 123.4, i);
    }
}
test3ArgsInt32ThenNumber();

function test4ArgsInt32() {
    function test(a, b, c, d, expectedMax, expectedMin) {
        assertEq(Math.max(a, b, c, d), expectedMax);
        assertEq(Math.min(a, b, c, d), expectedMin);
    }
    for (var i = 0; i < 20; i++) {
        test(30, 100, i, 0, 100, 0);
        test(i, 0, -1, -2, i, -2);
    }
    // Fail Int32 guard.
    test(0, 1, 2, "3", 3, 0);
    test(-0, 1, 2, 3, 3, -0);
}
test4ArgsInt32();

function test4ArgsNumber() {
    function test(a, b, c, d, expectedMax, expectedMin) {
        assertEq(Math.max(a, b, c, d), expectedMax);
        assertEq(Math.min(a, b, c, d), expectedMin);
    }
    for (var i = 0; i < 20; i++) {
        test(3.1, 100, i, -0, 100, -0);
        test(i, NaN, -1, -2, NaN, NaN);
    }
    // Fail Number guard.
    test(-0, 1, 2, "3", 3, -0);
    test("9", 1.1, 2, 3, 9, 1.1);
}
test4ArgsNumber();

function test4ArgsInt32ThenNumber() {
    function test(a, b, c, d, expectedMax, expectedMin) {
        assertEq(Math.max(a, b, c, d), expectedMax);
        assertEq(Math.min(a, b, c, d), expectedMin);
    }
    for (var i = 0; i < 20; i++) {
        test(i << 1, i - 100, -1, -2, i * 2, i - 100);
    }
    for (var i = 0; i < 10; i++) {
        test(i * 1.1, i, -0, 0, i * 1.1, -0);
    }
}
test4ArgsInt32ThenNumber();
