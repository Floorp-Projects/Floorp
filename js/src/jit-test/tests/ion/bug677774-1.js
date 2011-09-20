function min(a, b) {

    if (a < b) {
        return a;
    }
    else {
        return b;
    }
}

assertEq(min(6, 5), 5);
assertEq(min(42, 1337), 42);
assertEq(min(-12, 6), -12)
assertEq(min(5, -6), -6)
assertEq(min(-3, -2), -3)
assertEq(min(-5, -6), -6)
