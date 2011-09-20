function test_lsh(x, y) {
    return x << y;
}

function test_ursh(x, y) {
    return x >>> y;
}

function test_rsh(x, y) {
    return x >> y;
}

var x = 1;
assertEq(test_rsh(x, -1), 0);
assertEq(test_rsh(-1, x), -1);
assertEq(test_ursh(x, -1), 0);
assertEq(test_ursh(-1, x), 2147483647);
assertEq(test_lsh(x, -1), -2147483648);
assertEq(test_lsh(-1, x), -2);
assertEq(test_rsh(x, 1), 0);
assertEq(test_rsh(1, x), 0);
assertEq(test_ursh(x, 1), 0);
assertEq(test_ursh(1, x), 0);
assertEq(test_lsh(x, 1), 2);
assertEq(test_lsh(1, x), 2);
assertEq(test_rsh(x, 0), 1);
assertEq(test_rsh(0, x), 0);
assertEq(test_ursh(x, 0), 1);
assertEq(test_ursh(0, x), 0);
assertEq(test_lsh(x, 0), 1);
assertEq(test_lsh(0, x), 0);
assertEq(test_rsh(x, 0xffffffff), 0);
assertEq(test_rsh(0xffffffff, x), -1);
assertEq(test_ursh(x, 0xffffffff), 0);
assertEq(test_ursh(0xffffffff, x), 2147483647);
assertEq(test_lsh(x, 0xffffffff), -2147483648);
assertEq(test_lsh(0xffffffff, x), -2);
assertEq(test_rsh(x, "10.6"), 0);
assertEq(test_rsh("10.6", x), 5);
assertEq(test_ursh(x, "10.6"), 0);
assertEq(test_ursh("10.6", x), 5);
assertEq(test_lsh(x, "10.6"), 1024);
assertEq(test_lsh("10.6", x), 20);
assertEq(test_rsh(x, 2147483648), 1);
assertEq(test_rsh(2147483648, x), -1073741824);
assertEq(test_ursh(x, 2147483648), 1);
assertEq(test_ursh(2147483648, x), 1073741824);
assertEq(test_lsh(x, 2147483648), 1);
assertEq(test_lsh(2147483648, x), 0);
assertEq(test_rsh(x, 4294967296), 1);
assertEq(test_rsh(4294967296, x), 0);
assertEq(test_ursh(x, 4294967296), 1);
assertEq(test_ursh(4294967296, x), 0);
assertEq(test_lsh(x, 4294967296), 1);
assertEq(test_lsh(4294967296, x), 0);
assertEq(test_rsh(x, undefined), 1);
assertEq(test_rsh(undefined, x), 0);
assertEq(test_ursh(x, undefined), 1);
assertEq(test_ursh(undefined, x), 0);
assertEq(test_lsh(x, undefined), 1);
assertEq(test_lsh(undefined, x), 0);
assertEq(test_rsh(x, null), 1);
assertEq(test_rsh(null, x), 0);
assertEq(test_ursh(x, null), 1);
assertEq(test_ursh(null, x), 0);
assertEq(test_lsh(x, null), 1);
assertEq(test_lsh(null, x), 0);
assertEq(test_rsh(x, false), 1);
assertEq(test_rsh(false, x), 0);
assertEq(test_ursh(x, false), 1);
assertEq(test_ursh(false, x), 0);
assertEq(test_lsh(x, false), 1);
assertEq(test_lsh(false, x), 0);
assertEq(test_rsh(x, true), 0);
assertEq(test_rsh(true, x), 0);
assertEq(test_ursh(x, true), 0);
assertEq(test_ursh(true, x), 0);
assertEq(test_lsh(x, true), 2);
assertEq(test_lsh(true, x), 2);
assertEq(test_rsh(x, -1.5), 0);
assertEq(test_rsh(-1.5, x), -1);
assertEq(test_ursh(x, -1.5), 0);
assertEq(test_ursh(-1.5, x), 2147483647);
assertEq(test_lsh(x, -1.5), -2147483648);
assertEq(test_lsh(-1.5, x), -2);

var x = 0;
assertEq(test_rsh(x, -1), 0);
assertEq(test_rsh(-1, x), -1);
assertEq(test_ursh(x, -1), 0);
assertEq(test_ursh(-1, x), 4294967295);
assertEq(test_lsh(x, -1), 0);
assertEq(test_lsh(-1, x), -1);
assertEq(test_rsh(x, 1), 0);
assertEq(test_rsh(1, x), 1);
assertEq(test_ursh(x, 1), 0);
assertEq(test_ursh(1, x), 1);
assertEq(test_lsh(x, 1), 0);
assertEq(test_lsh(1, x), 1);
assertEq(test_rsh(x, 0), 0);
assertEq(test_rsh(0, x), 0);
assertEq(test_ursh(x, 0), 0);
assertEq(test_ursh(0, x), 0);
assertEq(test_lsh(x, 0), 0);
assertEq(test_lsh(0, x), 0);
assertEq(test_rsh(x, 0xffffffff), 0);
assertEq(test_rsh(0xffffffff, x), -1);
assertEq(test_ursh(x, 0xffffffff), 0);
assertEq(test_ursh(0xffffffff, x), 4294967295);
assertEq(test_lsh(x, 0xffffffff), 0);
assertEq(test_lsh(0xffffffff, x), -1);
assertEq(test_rsh(x, "10.6"), 0);
assertEq(test_rsh("10.6", x), 10);
assertEq(test_ursh(x, "10.6"), 0);
assertEq(test_ursh("10.6", x), 10);
assertEq(test_lsh(x, "10.6"), 0);
assertEq(test_lsh("10.6", x), 10);
assertEq(test_rsh(x, 2147483648), 0);
assertEq(test_rsh(2147483648, x), -2147483648);
assertEq(test_ursh(x, 2147483648), 0);
assertEq(test_ursh(2147483648, x), 2147483648);
assertEq(test_lsh(x, 2147483648), 0);
assertEq(test_lsh(2147483648, x), -2147483648);
assertEq(test_rsh(x, 4294967296), 0);
assertEq(test_rsh(4294967296, x), 0);
assertEq(test_ursh(x, 4294967296), 0);
assertEq(test_ursh(4294967296, x), 0);
assertEq(test_lsh(x, 4294967296), 0);
assertEq(test_lsh(4294967296, x), 0);
assertEq(test_rsh(x, undefined), 0);
assertEq(test_rsh(undefined, x), 0);
assertEq(test_ursh(x, undefined), 0);
assertEq(test_ursh(undefined, x), 0);
assertEq(test_lsh(x, undefined), 0);
assertEq(test_lsh(undefined, x), 0);
assertEq(test_rsh(x, null), 0);
assertEq(test_rsh(null, x), 0);
assertEq(test_ursh(x, null), 0);
assertEq(test_ursh(null, x), 0);
assertEq(test_lsh(x, null), 0);
assertEq(test_lsh(null, x), 0);
assertEq(test_rsh(x, false), 0);
assertEq(test_rsh(false, x), 0);
assertEq(test_ursh(x, false), 0);
assertEq(test_ursh(false, x), 0);
assertEq(test_lsh(x, false), 0);
assertEq(test_lsh(false, x), 0);
assertEq(test_rsh(x, true), 0);
assertEq(test_rsh(true, x), 1);
assertEq(test_ursh(x, true), 0);
assertEq(test_ursh(true, x), 1);
assertEq(test_lsh(x, true), 0);
assertEq(test_lsh(true, x), 1);
assertEq(test_rsh(x, -1.5), 0);
assertEq(test_rsh(-1.5, x), -1);
assertEq(test_ursh(x, -1.5), 0);
assertEq(test_ursh(-1.5, x), 4294967295);
assertEq(test_lsh(x, -1.5), 0);
assertEq(test_lsh(-1.5, x), -1);

var x = -1;
assertEq(test_rsh(x, -1), -1);
assertEq(test_rsh(-1, x), -1);
assertEq(test_ursh(x, -1), 1);
assertEq(test_ursh(-1, x), 1);
assertEq(test_lsh(x, -1), -2147483648);
assertEq(test_lsh(-1, x), -2147483648);
assertEq(test_rsh(x, 1), -1);
assertEq(test_rsh(1, x), 0);
assertEq(test_ursh(x, 1), 2147483647);
assertEq(test_ursh(1, x), 0);
assertEq(test_lsh(x, 1), -2);
assertEq(test_lsh(1, x), -2147483648);
assertEq(test_rsh(x, 0), -1);
assertEq(test_rsh(0, x), 0);
assertEq(test_ursh(x, 0), 4294967295);
assertEq(test_ursh(0, x), 0);
assertEq(test_lsh(x, 0), -1);
assertEq(test_lsh(0, x), 0);
assertEq(test_rsh(x, 0xffffffff), -1);
assertEq(test_rsh(0xffffffff, x), -1);
assertEq(test_ursh(x, 0xffffffff), 1);
assertEq(test_ursh(0xffffffff, x), 1);
assertEq(test_lsh(x, 0xffffffff), -2147483648);
assertEq(test_lsh(0xffffffff, x), -2147483648);
assertEq(test_rsh(x, "10.6"), -1);
assertEq(test_rsh("10.6", x), 0);
assertEq(test_ursh(x, "10.6"), 4194303);
assertEq(test_ursh("10.6", x), 0);
assertEq(test_lsh(x, "10.6"), -1024);
assertEq(test_lsh("10.6", x), 0);
assertEq(test_rsh(x, 2147483648), -1);
assertEq(test_rsh(2147483648, x), -1);
assertEq(test_ursh(x, 2147483648), 4294967295);
assertEq(test_ursh(2147483648, x), 1);
assertEq(test_lsh(x, 2147483648), -1);
assertEq(test_lsh(2147483648, x), 0);
assertEq(test_rsh(x, 4294967296), -1);
assertEq(test_rsh(4294967296, x), 0);
assertEq(test_ursh(x, 4294967296), 4294967295);
assertEq(test_ursh(4294967296, x), 0);
assertEq(test_lsh(x, 4294967296), -1);
assertEq(test_lsh(4294967296, x), 0);
assertEq(test_rsh(x, undefined), -1);
assertEq(test_rsh(undefined, x), 0);
assertEq(test_ursh(x, undefined), 4294967295);
assertEq(test_ursh(undefined, x), 0);
assertEq(test_lsh(x, undefined), -1);
assertEq(test_lsh(undefined, x), 0);
assertEq(test_rsh(x, null), -1);
assertEq(test_rsh(null, x), 0);
assertEq(test_ursh(x, null), 4294967295);
assertEq(test_ursh(null, x), 0);
assertEq(test_lsh(x, null), -1);
assertEq(test_lsh(null, x), 0);
assertEq(test_rsh(x, false), -1);
assertEq(test_rsh(false, x), 0);
assertEq(test_ursh(x, false), 4294967295);
assertEq(test_ursh(false, x), 0);
assertEq(test_lsh(x, false), -1);
assertEq(test_lsh(false, x), 0);
assertEq(test_rsh(x, true), -1);
assertEq(test_rsh(true, x), 0);
assertEq(test_ursh(x, true), 2147483647);
assertEq(test_ursh(true, x), 0);
assertEq(test_lsh(x, true), -2);
assertEq(test_lsh(true, x), -2147483648);
assertEq(test_rsh(x, -1.5), -1);
assertEq(test_rsh(-1.5, x), -1);
assertEq(test_ursh(x, -1.5), 1);
assertEq(test_ursh(-1.5, x), 1);
assertEq(test_lsh(x, -1.5), -2147483648);
assertEq(test_lsh(-1.5, x), -2147483648);



assertEq(test_ursh(0, -2147483648), 0);
assertEq(test_ursh(0, 2147483648), 0);
assertEq(test_ursh(0, 45), 0);
assertEq(test_ursh(0, -45), 0);
assertEq(test_ursh(100, -2147483648), 100);
assertEq(test_ursh(100, 2147483648), 100);
assertEq(test_ursh(100, 45), 0);
assertEq(test_ursh(100, -45), 0);
assertEq(test_ursh(-100, -2147483648), 4294967196);
assertEq(test_ursh(-100, 2147483648), 4294967196);
assertEq(test_ursh(-100, 45), 524287);
assertEq(test_ursh(-100, -45), 8191);



function test1() {
    var i = 0;
    return 2147483647 >>> i;
}
assertEq(test1(), 2147483647);
function test2() {
    var i = 1;
    return 2147483647 >>> i;
}
assertEq(test2(), 1073741823);
function test3() {
    var i = 0;
    return -1 >>> i;
}
assertEq(test3(), 4294967295);
function test4() {
    var i = 3;
    return -1 >>> i;
}
assertEq(test4(), 536870911);
function test5() {
    var i = 0;
    return -3648 >>> i;
}
assertEq(test5(), 4294963648);
