function f(x, y) {
    -(undefined ? 0 : 0);
    assertEq(y === y, true);
    return 0;
}
f(1, 2);
{
    f(3, 3.14);
    f(true, f(4, 5));

    function g() {}
}
