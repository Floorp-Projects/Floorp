function a1(a2) {
    return 10 - a2;
}
a3 = a1(-2147483648);
assertEq(a3, 2147483658);
