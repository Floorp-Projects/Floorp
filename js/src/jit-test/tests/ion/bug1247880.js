function f(x) {
    var a = x;
    a = Number ? a | 0 : 0;
    a = a >>> 0;
    a = Math.imul(0x100000001, a);
    a = a % 2;
    a = a | 0;
    return a;
};

assertEq(f(0), 0);
assertEq(f(-1), -1);
