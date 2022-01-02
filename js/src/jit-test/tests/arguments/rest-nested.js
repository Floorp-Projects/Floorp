function f(...rest) {
    function nested () {
        return rest;
    }
    return nested;
}
assertEq(f(1, 2, 3)().toString(), [1, 2, 3].toString());