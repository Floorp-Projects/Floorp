
function f(x) {
    return (x % 123.45) >> x;
}
assertEq(f(-123), -4);
