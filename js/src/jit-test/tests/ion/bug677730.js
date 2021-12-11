function f0(p) {
    return p + 1;
}
assertEq(f0(0x7fffffff), 0x80000000);
