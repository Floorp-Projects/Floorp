function f(...rest) {
    function nested() {
        return arguments.length;
    }
    return nested;
}
assertEq(f()(1, 2, 3), 3);
