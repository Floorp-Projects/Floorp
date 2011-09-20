function foldme() {
    var v0 = 0x7fffffff;
    var v1 = 1;
    var v2 = v0 + v1;
    return v2;
}
assertEq(foldme(),2147483648);

