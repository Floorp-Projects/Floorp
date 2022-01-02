function test1() {
    function mod(x, y) {
        return x % y;
    }
    for (var i=0; i<60; i++) {
        assertEq(mod(4, 2), 0);
        assertEq(mod(5.5, 2.5), 0.5);
        assertEq(mod(10.3, 0), NaN);
        assertEq(mod(-0, -3), -0);

    }
}
test1();
