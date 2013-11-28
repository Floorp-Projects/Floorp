function sprod(x, y) {
    var iprod = Math.imul(x | 0, y | 0);
    var fprod = (x | 0) * (y | 0);
    return iprod + fprod;
}
assertEq(sprod(2, 2), 8);
assertEq(sprod(0x10000, 0x10000), 0x100000000);
