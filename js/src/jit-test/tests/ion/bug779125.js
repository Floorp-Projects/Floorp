function test() {
    for (var i = 0; i < 60; i++) {
        x = ''.charAt(-1);
        assertEq(x, "");
    }
}
test();
