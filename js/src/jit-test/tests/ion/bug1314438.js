
function g(x) {
    return (-1 % x && Math.cos(8) >>> 0);
}
g(2);
assertEq(uneval(g(-1)), "-0");
