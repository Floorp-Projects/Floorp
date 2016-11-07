
function f(x) {
    return Math.pow(x, x) && 0;
}
f(0);
assertEq(uneval(f(-999)), "-0");

function g(x) {
    return (-1 % x && Math.cos(8) >>> 0);
}
g(2);
assertEq(uneval(g(-1)), "-0");
