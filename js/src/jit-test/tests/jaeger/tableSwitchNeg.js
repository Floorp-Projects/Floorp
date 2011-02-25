
function f(a) {
    switch(a) {
        case -1: return 1;
        case -2: return 2;
        case -5: return 5;
        default: return 10;
    }
}

assertEq(f(-1), 1);
assertEq(f(-2), 2);
assertEq(f(-5), 5);

assertEq(f(-3), 10);
assertEq(f(-6), 10);
assertEq(f(0), 10);
assertEq(f(1), 10);

assertEq(f(-2147483647), 10);
assertEq(f(-2147483648), 10);
assertEq(f(-2147483649), 10);

assertEq(f(2147483647), 10);
assertEq(f(2147483648), 10);
assertEq(f(2147483649), 10);

